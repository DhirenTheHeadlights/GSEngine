export module gse.ecs:task_graph;

import std;

import gse.concurrency;
import gse.core;
import gse.containers;

export namespace gse {
	class task_graph {
	public:
		auto clear() -> void;

		auto notify_state_ready(
			id state_type
		) -> void;

		auto reset_state(
			id state_type
		) -> void;

		auto wait_state_ready(
			id state_type
		) -> async::task<>;

		auto is_state_ready(
			id state_type
		) -> bool;

	private:
		struct state_slot {
			std::atomic<bool> ready{ false };
			std::mutex waiter_lock;
			std::vector<std::coroutine_handle<>> waiters;
		};

		auto get_or_create_slot(
			id state_type
		) -> state_slot*;

		std::flat_map<id, std::unique_ptr<state_slot>> m_states;
		std::shared_mutex m_states_mutex;
	};
}

auto gse::task_graph::clear() -> void {
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
	m_states.try_emplace(state_type, std::move(slot));
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
		h.resume();
	}
}

auto gse::task_graph::reset_state(const id state_type) -> void {
	auto* slot = get_or_create_slot(state_type);
	slot->ready.store(false, std::memory_order_release);
}

auto gse::task_graph::is_state_ready(const id state_type) -> bool {
	auto* slot = get_or_create_slot(state_type);
	return slot->ready.load(std::memory_order_acquire);
}

auto gse::task_graph::wait_state_ready(const id state_type) -> async::task<> {
	auto* slot = get_or_create_slot(state_type);
	if (slot->ready.load(std::memory_order_acquire)) {
		co_return;
	}

	struct state_awaiter {
		state_slot* slot;

		[[nodiscard]] auto await_ready() const noexcept -> bool {
			return false;
		}

		auto await_suspend(std::coroutine_handle<> h) const noexcept -> void {
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
				h.resume();
			}
		}

		auto await_resume() const noexcept -> void {
		}
	};

	co_await state_awaiter{ slot };
}
