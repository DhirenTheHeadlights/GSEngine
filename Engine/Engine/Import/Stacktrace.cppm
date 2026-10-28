export module gse.stacktrace;

import std;

export namespace gse {
	struct crash_record {
		std::string reason;
		std::string detail;
		std::string thread;
		std::vector<std::string> frames;
		std::uintptr_t module_base = 0;
	};

	using crash_responder = auto (*)(const crash_record&) -> void;

	auto capture_stacktrace(
		std::size_t skip_frames = 1
	) -> std::string;

	auto install_crash_handlers() -> void;

	auto install_crash_responder(
		crash_responder responder
	) -> void;

	auto report_crash(
		std::string reason,
		std::string detail
	) -> void;

	auto clean_symbol(
		std::string symbol
	) -> std::string;
}
