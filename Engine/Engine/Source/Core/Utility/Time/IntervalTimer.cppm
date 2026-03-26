export module gse.utility:interval_timer;

import std;

import gse.assert;
import gse.math;

import :clock;
import :system_clock;

export namespace gse {
	template <typename T = float>
	class interval_timer {
	public:
		explicit interval_timer(
			time_t<T> interval
		);

		auto tick(
			time_t<T> dt = system_clock::dt<time_t<T>>()
		) -> bool;

		auto reset(
		) -> void;
	private:
		time_t<T> m_interval;
		time_t<T> m_accumulated = time_t<T>(0);
	};
}

template <typename T>
gse::interval_timer<T>::interval_timer(const time_t<T> interval) : m_interval(interval) {
	assert(
		m_interval >= time_t<T>(0),
		std::source_location::current(),
		"Interval must be non-negative"
	);
}

template <typename T>
auto gse::interval_timer<T>::tick(const time_t<T> dt) -> bool {
	assert(
		dt >= time_t<T>(0),
		std::source_location::current(),
		"Delta time must be non-negative"
	);

	m_accumulated += dt;
	if (m_accumulated >= m_interval) {
		m_accumulated -= m_interval;
		return true;
	}
	return false;
}

template <typename T>
auto gse::interval_timer<T>::reset() -> void {
	m_accumulated = {};
}
