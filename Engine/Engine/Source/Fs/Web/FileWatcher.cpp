module gse.fs:file_watcher_platform;

import std;

import :file_watcher;

auto gse::file_watcher::open_stream(watch_entry&) -> void {
}

auto gse::file_watcher::close_stream(watch_entry&) -> void {
}

auto gse::file_watcher::arm_stream(watch_entry&) -> bool {
	return false;
}

auto gse::file_watcher::drain_completions() -> std::size_t {
	return 0;
}

auto gse::file_watcher::destroy_completion_port() -> void {
}
