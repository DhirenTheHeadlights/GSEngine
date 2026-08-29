export module gse.ecs:task_graph;

import std;

import gse.concurrency;
import gse.core;
import gse.containers;
import gse.log;

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
			std::vector<async::checked_handle> waiters;
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

	std::vector<async::checked_handle> handles;
	{
		std::lock_guard lock(slot->waiter_lock);
		handles = std::move(slot->waiters);
	}

	for (const async::checked_handle& h : handles) {
		if (!async::resume_checked(h)) {
			log::println(
				log::level::error,
				log::category::task,
				"task_graph: waiter on state {} was not resumed because its coroutine frame had already been destroyed",
				state_type
			);
		}
	}
}

auto gse::task_graph::reset_state(const id state_type) -> void {
	auto* slot = get_or_create_slot(state_type);
	slot->ready.store(false, std::memory_order_release);

	std::vector<async::checked_handle> stale;
	{
		std::lock_guard lock(slot->waiter_lock);
		stale = std::move(slot->waiters);
	}

	if (!stale.empty()) {
		log::println(
			log::level::error,
			log::category::task,
			"task_graph: reset state {} while {} waiter(s) were still parked; they belong to a finished dispatch and are being dropped",
			state_type,
			stale.size()
		);
	}
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
			const async::checked_handle tracked = async::track_frame(h);
			bool ready_now = false;
			{
				std::lock_guard lock(slot->waiter_lock);
				if (slot->ready.load(std::memory_order_acquire)) {
					ready_now = true;
				}
				else {
					slot->waiters.push_back(tracked);
				}
			}

			if (ready_now) {
				async::resume_checked(tracked);
			}
		}

		auto await_resume() const noexcept -> void {
		}
	};

	co_await state_awaiter{ slot };
}
