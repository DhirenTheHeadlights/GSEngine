export module gse.ecs:task_graph;

import gse.assert;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.log;
import std;

export namespace gse {
	class task_graph {
		struct state_slot;

	public:
		struct ready_awaiter {
			state_slot* slot;

			[[nodiscard]] auto await_ready() const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> h
			) const noexcept -> bool;

			auto await_resume() const noexcept -> void;
		};

		auto add_state(
			id state_type,
			bool ready
		) -> void;

		auto clear() -> void;

		auto notify_state_ready(
			id state_type
		) -> void;

		auto reset_state(
			id state_type
		) -> void;

		auto wait_state_ready(
			id state_type
		) -> ready_awaiter;

		[[nodiscard]] auto is_state_ready(
			id state_type
		) const -> bool;

	private:
		struct state_slot {
			std::atomic<bool> ready{ false };
			std::mutex waiter_lock;
			std::vector<async::checked_handle> waiters;
		};

		auto slot_of(
			id state_type
		) const -> state_slot&;

		std::flat_map<id, std::unique_ptr<state_slot>> m_states;
	};
}

auto gse::task_graph::add_state(const id state_type, const bool ready) -> void {
	m_states.try_emplace(state_type, std::make_unique<state_slot>(ready));
}

auto gse::task_graph::clear() -> void {
	for (const auto& slot : std::views::values(m_states)) {
		slot->ready.store(false, std::memory_order_relaxed);
	}
}

auto gse::task_graph::slot_of(const id state_type) const -> state_slot& {
	const auto it = m_states.find(state_type);
	assert(it != m_states.end(), "task_graph has no slot for state {}; slots are added when a system node is registered", state_type);
	return *it->second;
}

auto gse::task_graph::notify_state_ready(const id state_type) -> void {
	auto& slot = slot_of(state_type);
	slot.ready.store(true, std::memory_order_release);

	std::vector<async::checked_handle> handles;
	{
		std::lock_guard _(slot.waiter_lock);
		handles = std::move(slot.waiters);
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
	slot_of(state_type).ready.store(false, std::memory_order_release);
}

auto gse::task_graph::is_state_ready(const id state_type) const -> bool {
	return slot_of(state_type).ready.load(std::memory_order_acquire);
}

auto gse::task_graph::wait_state_ready(const id state_type) -> ready_awaiter {
	return {
		.slot = &slot_of(state_type),
	};
}

auto gse::task_graph::ready_awaiter::await_ready() const noexcept -> bool {
	return slot->ready.load(std::memory_order_acquire);
}

auto gse::task_graph::ready_awaiter::await_suspend(const std::coroutine_handle<> h) const noexcept -> bool {
	std::lock_guard _(slot->waiter_lock);
	if (slot->ready.load(std::memory_order_acquire)) {
		return false;
	}
	slot->waiters.push_back(async::track_frame(h));
	return true;
}

auto gse::task_graph::ready_awaiter::await_resume() const noexcept -> void {}
