export module gse.concurrency:manual_event;

import std;

import gse.core;

export namespace gse::async {
	class manual_event : non_copyable, non_movable {
	public:
		manual_event() = default;

		~manual_event() = default;

		struct awaiter {
			manual_event* m_event;

			[[nodiscard]] auto await_ready() const noexcept -> bool;

			[[nodiscard]] auto await_suspend(
				std::coroutine_handle<> h
			) const noexcept -> bool;

			static auto await_resume() noexcept -> void;
		};

		auto wait() -> awaiter;

		auto set() -> void;

		auto reset() -> void;

		auto is_set() const -> bool;

	private:
		mutable std::mutex m_state;
		bool m_set = false;
		std::vector<std::coroutine_handle<>> m_waiters;
	};
}

auto gse::async::manual_event::awaiter::await_ready() const noexcept -> bool {
	std::lock_guard lock(m_event->m_state);
	return m_event->m_set;
}

auto gse::async::manual_event::awaiter::await_suspend(const std::coroutine_handle<> h) const noexcept -> bool {
	std::lock_guard lock(m_event->m_state);
	if (m_event->m_set) {
		return false;
	}
	m_event->m_waiters.push_back(h);
	return true;
}

auto gse::async::manual_event::awaiter::await_resume() noexcept -> void {
}

auto gse::async::manual_event::wait() -> awaiter {
	return awaiter{ this };
}

auto gse::async::manual_event::set() -> void {
	std::vector<std::coroutine_handle<>> to_wake;
	{
		std::lock_guard lock(m_state);
		if (m_set) {
			return;
		}
		m_set = true;
		to_wake.swap(m_waiters);
	}
	for (const auto h : to_wake) {
		h.resume();
	}
}

auto gse::async::manual_event::reset() -> void {
	std::lock_guard lock(m_state);
	m_set = false;
}

auto gse::async::manual_event::is_set() const -> bool {
	std::lock_guard lock(m_state);
	return m_set;
}
