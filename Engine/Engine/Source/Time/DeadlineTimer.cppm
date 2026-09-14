export module gse.time:deadline_timer;

import std;

import gse.math;

import :frame_demand;
import :system_clock;

export namespace gse {
	class deadline_timer {
	public:
		auto arm(
			time delay
		) -> void;

		auto disarm() -> void;

		[[nodiscard]] auto armed() const -> bool;

		[[nodiscard]] auto due() const -> bool;

	private:
		std::optional<time_t<double, seconds>> m_deadline;
	};
}

auto gse::deadline_timer::arm(const time delay) -> void {
	m_deadline = system_clock::now<time_t<double, seconds>>() + delay;
}

auto gse::deadline_timer::disarm() -> void {
	m_deadline.reset();
}

auto gse::deadline_timer::armed() const -> bool {
	return m_deadline.has_value();
}

auto gse::deadline_timer::due() const -> bool {
	if (!m_deadline) {
		return false;
	}
	if (system_clock::now<time_t<double, seconds>>() >= *m_deadline) {
		return true;
	}
	frame_demand::request_frame_at(*m_deadline);
	return false;
}
