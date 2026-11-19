module gse.diag:watchdog_platform;

import std;

import :watchdog;

auto gse::watchdog::open_stack_capture() -> void {}

auto gse::watchdog::close_stack_capture() -> void {}

auto gse::watchdog::capture_all_thread_stacks() -> std::string {
	return {};
}
