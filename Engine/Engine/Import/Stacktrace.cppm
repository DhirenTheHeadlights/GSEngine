export module gse.stacktrace;

import std;

export namespace gse {
	auto capture_stacktrace(
		std::size_t skip_frames = 1
	) -> std::string;

	auto install_crash_handlers() -> void;
}
