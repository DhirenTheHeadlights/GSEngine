export module gse.concurrency:task_graph;

import std;

import gse.core;
import gse.containers;
import gse.diag;
import gse.log;

import :async_task;
import :task;

export namespace gse {
	class task_graph {
	public:
		using node_id = std::size_t;

		static constexpr std::size_t default_node_capacity = 128;

		explicit task_graph(
			std::size_t node_capacity = default_node_capacity
		);

		auto submit(
			id name,
			async::task<> coroutine,
			std::vector<id> reads = {},
			std::vector<id> writes = {}
		) -> node_id;

		auto execute(
		) -> void;

		auto clear(
		) -> void;

		auto notify_state_ready(
			id state_type
		) -> void;

		auto wait_state_ready(
			id state_type
		) -> async::task<>;

	private:
		struct graph_node {
			id name;
			async::task<> coroutine;
			std::vector<id> reads;
			std::vector<id> writes;
			std::vector<node_id> dependents;
			std::atomic<std::size_t> in_degree{ 0 };
			std::atomic<bool> completed{ false };
		};

		struct state_slot {
			std::atomic<bool> ready{ false };
			std::mutex waiter_lock;
			std::vector<std::coroutine_handle<>> waiters;
		};

		auto build_edges(
		) const -> void;

		auto enqueue_ready(
			graph_node& node,
			task::group& work_group
		) -> void;

		auto get_or_create_slot(
			id state_type
		) -> state_slot*;

		linear_vector<std::unique_ptr<graph_node>> m_nodes;

		flat_map<id, std::unique_ptr<state_slot>> m_states;
		std::shared_mutex m_states_mutex;
	};
}

gse::task_graph::task_graph(const std::size_t node_capacity) : m_nodes(node_capacity) {}

auto gse::task_graph::submit(const id name, async::task<> coroutine, std::vector<id> reads, std::vector<id> writes) -> node_id {
	const auto idx = m_nodes.size();
	auto node = std::make_unique<graph_node>();
	node->name = name;
	node->coroutine = std::move(coroutine);
	node->reads = std::move(reads);
	node->writes = std::move(writes);
	m_nodes.push_back(std::move(node));
	return idx;
}

auto gse::task_graph::build_edges() const -> void {
	const auto n = m_nodes.size();
	for (std::size_t a = 0; a < n; ++a) {
		for (std::size_t b = a + 1; b < n; ++b) {
			auto& na = *m_nodes[a];
			auto& nb = *m_nodes[b];

			bool a_before_b = false;
			bool b_before_a = false;

			for (const auto& w : na.writes) {
				if (std::ranges::contains(nb.reads, w) ||
					std::ranges::contains(nb.writes, w)) {
					a_before_b = true;
					break;
				}
			}

			for (const auto& w : nb.writes) {
				if (std::ranges::contains(na.reads, w)) {
					b_before_a = true;
					break;
				}
			}

			if (a_before_b) {
				na.dependents.push_back(b);
				nb.in_degree.fetch_add(1, std::memory_order_relaxed);
			}
			else if (b_before_a) {
				nb.dependents.push_back(a);
				na.in_degree.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}
}

auto gse::task_graph::enqueue_ready(graph_node& node, task::group& work_group) -> void {
	work_group.post([this, &node, &work_group] {
		node.coroutine.start();

		node.completed.store(true, std::memory_order_release);

		for (const auto dep_id : node.dependents) {
			if (auto& dep = *m_nodes[dep_id]; dep.in_degree.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				enqueue_ready(dep, work_group);
			}
		}
	}, node.name);
}

auto gse::task_graph::execute() -> void {
	if (m_nodes.empty()) {
		return;
	}

	build_edges();

	task::group work_group(trace_id<"task_graph::execute">());

	std::vector<graph_node*> initially_ready;
	initially_ready.reserve(m_nodes.size());
	for (auto& node : m_nodes) {
		if (node->in_degree.load(std::memory_order_relaxed) == 0) {
			initially_ready.push_back(node.get());
		}
	}

	for (auto* node : initially_ready) {
		enqueue_ready(*node, work_group);
	}

	work_group.wait();
}

auto gse::task_graph::clear() -> void {
	m_nodes.clear();
	std::shared_lock lock(m_states_mutex);
	for (auto& slot : std::views::values(m_states)) {
		slot->ready.store(false, std::memory_order_relaxed);
		std::lock_guard waiter_lock(slot->waiter_lock);
		slot->waiters.clear();
	}
}

auto gse::task_graph::get_or_create_slot(const id state_type) -> state_slot* {
	{
		std::shared_lock lock(m_states_mutex);
		if (const auto it = m_states.find(state_type); it != m_states.end()) {
			return it->second.get();
		}
	}

	std::unique_lock lock(m_states_mutex);
	if (const auto it = m_states.find(state_type); it != m_states.end()) {
		return it->second.get();
	}
	auto slot = std::make_unique<state_slot>();
	auto* raw = slot.get();
	m_states.emplace(state_type, std::move(slot));
	return raw;
}

auto gse::task_graph::notify_state_ready(const id state_type) -> void {
	auto* slot = get_or_create_slot(state_type);
	slot->ready.store(true, std::memory_order_release);

	std::vector<std::coroutine_handle<>> handles;
	{
		std::lock_guard lock(slot->waiter_lock);
		handles = std::move(slot->waiters);
	}

	for (auto h : handles) {
		task::post([h] { h.resume(); });
	}
}

auto gse::task_graph::wait_state_ready(const id state_type) -> async::task<> {
	auto* slot = get_or_create_slot(state_type);
	if (slot->ready.load(std::memory_order_acquire)) {
		co_return;
	}

	struct state_awaiter {
		state_slot* slot;

		static auto await_ready(
		) noexcept -> bool {
			return false;
		}

		auto await_suspend(
			std::coroutine_handle<> h
		) const noexcept -> void {
			bool ready_now = false;
			{
				std::lock_guard lock(slot->waiter_lock);
				if (slot->ready.load(std::memory_order_acquire)) {
					ready_now = true;
				}
				else {
					slot->waiters.push_back(h);
				}
			}

			if (ready_now) {
				task::post([h] { h.resume(); });
			}
		}

		static auto await_resume(
		) noexcept -> void {}
	};

	co_await state_awaiter{ slot };
}
