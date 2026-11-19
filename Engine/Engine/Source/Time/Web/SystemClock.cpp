module gse.time:system_clock_impl;

import std;

import gse.math;

import :system_clock;

auto gse::system_clock::from_query_performance_counter(std::uint64_t) -> time_t<double> {
	return now<time_t<double>>();
}
