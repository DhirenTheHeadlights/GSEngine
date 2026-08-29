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

	private:
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
