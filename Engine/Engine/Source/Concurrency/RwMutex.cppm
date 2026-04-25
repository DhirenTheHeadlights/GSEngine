export module gse.concurrency:rw_mutex;

import std;

import gse.core;

import :task;

export namespace gse::async {
	class rw_mutex {
	public:
		rw_mutex() = default;

		rw_mutex(
			const rw_mutex&
		) = delete;

		rw_mutex(
			rw_mutex&&
		) = delete;

		auto operator=(
			const rw_mutex&
		) -> rw_mutex& = delete;

		auto operator=(
			rw_mutex&&
		) -> rw_mutex& = delete;

		struct shared_awaiter {
			rw_mutex* m_mutex;

			auto await_ready(
			) const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> h
			) const noexcept -> bool;

			auto await_resume(
			) const noexcept -> void;
		};

		struct exclusive_awaiter {
			rw_mutex* m_mutex;

			auto await_ready(
			) const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> h
			) const noexcept -> bool;

			auto await_resume(
			) const noexcept -> void;
		};

		auto lock_shared(
		) -> shared_awaiter;

		auto lock_exclusive(
		) -> exclusive_awaiter;

		auto unlock_shared(
		) -> void;

		auto unlock_exclusive(
		) -> void;

	private:
		std::mutex m_state;
		int m_shared_count = 0;
		bool m_exclusive_held = false;
		std::deque<std::coroutine_handle<>> m_shared_waiters;
		std::deque<std::coroutine_handle<>> m_exclusive_waiters;
	};

	class rw_mutex_registry {
	public:
		auto mutex_for(
			id type
		) -> rw_mutex&;

	private:
		std::unordered_map<id, std::unique_ptr<rw_mutex>> m_mutexes;
		std::shared_mutex m_map_mutex;
	};
}

auto gse::async::rw_mutex::shared_awaiter::await_ready() const noexcept -> bool {
	std::lock_guard lock(m_mutex->m_state);
	if (!m_mutex->m_exclusive_held && m_mutex->m_exclusive_waiters.empty()) {
		++m_mutex->m_shared_count;
		return true;
	}
	return false;
}

auto gse::async::rw_mutex::shared_awaiter::await_suspend(const std::coroutine_handle<> h) const noexcept -> bool {
	std::lock_guard lock(m_mutex->m_state);
	if (!m_mutex->m_exclusive_held && m_mutex->m_exclusive_waiters.empty()) {
		++m_mutex->m_shared_count;
		return false;
	}
	m_mutex->m_shared_waiters.push_back(h);
	return true;
}

auto gse::async::rw_mutex::shared_awaiter::await_resume() const noexcept -> void {}

auto gse::async::rw_mutex::exclusive_awaiter::await_ready() const noexcept -> bool {
	std::lock_guard lock(m_mutex->m_state);
	if (!m_mutex->m_exclusive_held && m_mutex->m_shared_count == 0) {
		m_mutex->m_exclusive_held = true;
		return true;
	}
	return false;
}

auto gse::async::rw_mutex::exclusive_awaiter::await_suspend(const std::coroutine_handle<> h) const noexcept -> bool {
	std::lock_guard lock(m_mutex->m_state);
	if (!m_mutex->m_exclusive_held && m_mutex->m_shared_count == 0) {
		m_mutex->m_exclusive_held = true;
		return false;
	}
	m_mutex->m_exclusive_waiters.push_back(h);
	return true;
}

auto gse::async::rw_mutex::exclusive_awaiter::await_resume() const noexcept -> void {}

auto gse::async::rw_mutex::lock_shared() -> shared_awaiter {
	return { this };
}

auto gse::async::rw_mutex::lock_exclusive() -> exclusive_awaiter {
	return { this };
}

auto gse::async::rw_mutex::unlock_shared() -> void {
	std::vector<std::coroutine_handle<>> to_wake;
	{
		std::lock_guard lock(m_state);
		--m_shared_count;
		if (m_shared_count == 0 && !m_exclusive_waiters.empty()) {
			const auto h = m_exclusive_waiters.front();
			m_exclusive_waiters.pop_front();
			m_exclusive_held = true;
			to_wake.push_back(h);
		}
	}
	for (const auto h : to_wake) {
		task::post([h] { h.resume(); });
	}
}

auto gse::async::rw_mutex::unlock_exclusive() -> void {
	std::vector<std::coroutine_handle<>> to_wake;
	{
		std::lock_guard lock(m_state);
		m_exclusive_held = false;
		if (!m_exclusive_waiters.empty()) {
			const auto h = m_exclusive_waiters.front();
			m_exclusive_waiters.pop_front();
			m_exclusive_held = true;
			to_wake.push_back(h);
		}
		else {
			while (!m_shared_waiters.empty()) {
				to_wake.push_back(m_shared_waiters.front());
				m_shared_waiters.pop_front();
				++m_shared_count;
			}
		}
	}
	for (const auto h : to_wake) {
		task::post([h] { h.resume(); });
	}
}

auto gse::async::rw_mutex_registry::mutex_for(const id type) -> rw_mutex& {
	{
		std::shared_lock lock(m_map_mutex);
		if (const auto it = m_mutexes.find(type); it != m_mutexes.end()) {
			return *it->second;
		}
	}

	std::unique_lock lock(m_map_mutex);
	if (const auto it = m_mutexes.find(type); it != m_mutexes.end()) {
		return *it->second;
	}
	auto fresh = std::make_unique<rw_mutex>();
	auto& ref = *fresh;
	m_mutexes.emplace(type, std::move(fresh));
	return ref;
}
