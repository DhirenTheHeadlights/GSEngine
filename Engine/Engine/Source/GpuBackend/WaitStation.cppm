export module gse.gpu_backend:wait_station;

import std;

import gse.core;

export namespace gse::gpu {
	class wait_station final : public non_copyable {
	public:
		wait_station() = default;

		wait_station(
			wait_station&& other
		) noexcept;

		auto operator=(
			wait_station&& other
		) noexcept -> wait_station&;

		[[nodiscard]] auto progress() const -> std::uint64_t;

		[[nodiscard]] auto reached(
			std::uint64_t value
		) const -> bool;

		auto park(
			std::uint64_t value,
			std::coroutine_handle<> handle
		) -> void;

		auto advance(
			std::uint64_t reached_value
		) -> void;

	private:
		struct waiter {
			std::uint64_t value;
			std::coroutine_handle<> handle;
		};

		std::atomic<std::uint64_t> m_progress{ 0 };
		std::vector<waiter> m_waiters;
		std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>();
	};
}

gse::gpu::wait_station::wait_station(wait_station&& other) noexcept
	: m_progress(other.m_progress.load(std::memory_order_relaxed)),
	  m_waiters(std::move(other.m_waiters)),
	  m_mutex(std::move(other.m_mutex)) {
}

auto gse::gpu::wait_station::operator=(wait_station&& other) noexcept -> wait_station& {
	if (this != &other) {
		m_progress.store(other.m_progress.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_waiters = std::move(other.m_waiters);
		m_mutex = std::move(other.m_mutex);
	}
	return *this;
}

auto gse::gpu::wait_station::progress() const -> std::uint64_t {
	return m_progress.load(std::memory_order_acquire);
}

auto gse::gpu::wait_station::reached(const std::uint64_t value) const -> bool {
	return m_progress.load(std::memory_order_acquire) >= value;
}

auto gse::gpu::wait_station::park(const std::uint64_t value, std::coroutine_handle<> handle) -> void {
	std::lock_guard lock(*m_mutex);
	m_waiters.push_back({
		.value = value,
		.handle = handle,
	});
}

auto gse::gpu::wait_station::advance(const std::uint64_t reached_value) -> void {
	std::vector<std::coroutine_handle<>> ready;
	{
		std::lock_guard lock(*m_mutex);
		const auto current = m_progress.load(std::memory_order_relaxed);
		if (reached_value > current) {
			m_progress.store(reached_value, std::memory_order_release);
		}
		const auto snapshot = m_progress.load(std::memory_order_relaxed);
		std::erase_if(
			m_waiters,
			[&](const waiter& w) {
				if (w.value <= snapshot) {
					ready.push_back(w.handle);
					return true;
				}
				return false;
			}
		);
	}
	for (auto handle : ready) {
		handle.resume();
	}
}
