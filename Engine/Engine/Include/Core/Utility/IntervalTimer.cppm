export module gse.utility:interval_timer;

import std;

import gse.assert;
import gse.physics.math;

import :clock;
import :system_clock;

export namespace gse {
	class interval_timer {
	public:
		explicit interval_timer(
			time interval
		);

		auto tick(
			time dt = system_clock::dt()
		) -> bool;

		auto reset(
		) -> void;
	private:
		time m_interval;
		time m_accumulated = time(0);
	};
}

gse::interval_timer::interval_timer(const time interval) : m_interval(interval) {
	assert(
		m_interval >= time(0),
		std::format("Interval must be non-negative, got {}", m_interval)
	);
}

auto gse::interval_timer::tick(const time dt) -> bool {
	assert(
		dt >= time(0),
		std::format("Delta time must be non-negative, got {}", dt)
	);

	m_accumulated += dt;
	if (m_accumulated >= m_interval) {
		m_accumulated -= m_interval;
		return true;
	}
	return false;
}

auto gse::interval_timer::reset() -> void {
	m_accumulated = {};
}
