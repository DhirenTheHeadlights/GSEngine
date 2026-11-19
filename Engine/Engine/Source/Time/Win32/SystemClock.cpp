module gse.time:system_clock_impl;

import gse.math;
import gse.win32;
import std;

import :system_clock;

auto gse::system_clock::from_query_performance_counter(const std::uint64_t ticks) -> time_t<double> {
	const auto before = now<time_t<double>>();
	const auto current = win32::performance_counter();
	const auto after = now<time_t<double>>();
	static const auto frequency = win32::performance_counter_frequency();
	if (current == 0 || frequency == 0) {
		return after;
	}
	const auto midpoint = before + (after - before) * 0.5;
	const auto delta_ns = (static_cast<double>(ticks) - static_cast<double>(current)) * (1.0e9 / static_cast<double>(frequency));
	return midpoint + nanoseconds(delta_ns);
}
