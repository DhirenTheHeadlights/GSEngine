export module gse.time:interval_timer;

import std;

import gse.assert;
import gse.math;

import :frame_demand;
import :system_clock;

export namespace gse {
	template <typename T = float>
	class interval_timer {
	public:
		explicit interval_timer(
			time_t<T> interval
		);

		auto tick() -> bool;

		auto reset() -> void;

	private:
		time_t<T> m_interval;
		std::optional<time_t<double, seconds>> m_deadline;
	};
}

template <typename T>
gse::interval_timer<T>::interval_timer(const time_t<T> interval) : m_interval(interval) {
	assert(m_interval >= time_t<T>{}, "Interval must be non-negative");
}

template <typename T>
auto gse::interval_timer<T>::tick() -> bool {
	const auto now = system_clock::now<time_t<double, seconds>>();
	if (!m_deadline) {
		m_deadline = now + m_interval;
	}

	if (now >= *m_deadline) {
		*m_deadline += m_interval;
		if (*m_deadline <= now) {
			m_deadline = now + m_interval;
		}
		return true;
	}
	frame_demand::request_frame_at(*m_deadline);
	return false;
}

template <typename T>
auto gse::interval_timer<T>::reset() -> void {
	m_deadline = system_clock::now<time_t<double, seconds>>() + m_interval;
}
