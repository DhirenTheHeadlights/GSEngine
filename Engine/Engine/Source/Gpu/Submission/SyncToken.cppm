export module gse.gpu:sync_token;

import std;

import gse.gpu_backend;

export namespace gse::gpu {
	class sync_token {
	public:
		sync_token() = default;

		sync_token(
			wait_station* station,
			std::uint64_t value
		);

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto ready() const -> bool;

		[[nodiscard]] auto operator co_await() const noexcept;

	private:
		struct awaiter {
			wait_station* m_station;
			std::uint64_t m_value;

			auto await_ready() const noexcept -> bool;

			auto await_suspend(
				std::coroutine_handle<> caller
			) -> bool;

			auto await_resume() const noexcept -> void;
		};

		wait_station* m_station = nullptr;
		std::uint64_t m_value = 0;
	};
}

gse::gpu::sync_token::sync_token(wait_station* station, const std::uint64_t value) : m_station(station), m_value(value) {
}

auto gse::gpu::sync_token::valid() const -> bool {
	return m_station != nullptr;
}

auto gse::gpu::sync_token::ready() const -> bool {
	if (!m_station) {
		return true;
	}
	return m_station->reached(m_value);
}

auto gse::gpu::sync_token::operator co_await() const noexcept {
	return awaiter{ m_station, m_value };
}

auto gse::gpu::sync_token::awaiter::await_ready() const noexcept -> bool {
	if (!m_station) {
		return true;
	}
	return m_station->reached(m_value);
}

auto gse::gpu::sync_token::awaiter::await_suspend(std::coroutine_handle<> caller) -> bool {
	if (!m_station) {
		return false;
	}
	if (m_station->reached(m_value)) {
		return false;
	}
	m_station->park(m_value, caller);
	return true;
}

auto gse::gpu::sync_token::awaiter::await_resume() const noexcept -> void {
}
