export module gse.graphics:loading;

import std;

import gse.core;

export namespace gse::loading {
	class state : public non_copyable, public non_movable {
	public:
		state() = default;
		~state() = default;

		auto set_phase(std::string p) -> void;

		auto set_progress(std::uint32_t done, std::uint32_t total) -> void;

		auto mark_finished() -> void;

		[[nodiscard]] auto phase() const -> std::string;

		[[nodiscard]] auto done() const -> std::uint32_t;

		[[nodiscard]] auto total() const -> std::uint32_t;

		[[nodiscard]] auto finished() const -> bool;

	private:
		mutable std::mutex m_mutex;
		std::string m_phase;
		std::uint32_t m_done = 0;
		std::uint32_t m_total = 0;
		std::atomic<bool> m_finished = false;
	};
}

auto gse::loading::state::set_phase(std::string p) -> void {
	std::lock_guard lock(m_mutex);
	m_phase = std::move(p);
}

auto gse::loading::state::set_progress(const std::uint32_t done, const std::uint32_t total) -> void {
	std::lock_guard lock(m_mutex);
	m_done = done;
	m_total = total;
}

auto gse::loading::state::mark_finished() -> void {
	m_finished.store(true, std::memory_order_release);
}

auto gse::loading::state::phase() const -> std::string {
	std::lock_guard lock(m_mutex);
	return m_phase;
}

auto gse::loading::state::done() const -> std::uint32_t {
	std::lock_guard lock(m_mutex);
	return m_done;
}

auto gse::loading::state::total() const -> std::uint32_t {
	std::lock_guard lock(m_mutex);
	return m_total;
}

auto gse::loading::state::finished() const -> bool {
	return m_finished.load(std::memory_order_acquire);
}
