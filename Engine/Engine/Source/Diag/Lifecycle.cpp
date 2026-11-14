module gse.diag:lifecycle_impl;

import std;

import gse.core;
import gse.log;
import gse.math;

import :lifecycle;
import :trace;

auto gse::report_lifecycle_transition(const std::string_view machine, const log::category category, const std::string_view from, const std::string_view to, const time dwell) -> void {
	log::println(category, "{}: {} -> {} after {:.1f:ms}", machine, from, to, dwell);
	trace::mark(find_or_generate_id(std::format("{}::{}", machine, to)));
}
