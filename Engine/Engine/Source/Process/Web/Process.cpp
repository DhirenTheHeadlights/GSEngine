module gse.process;

import std;

auto gse::process::open_url(std::string_view) -> void {}

auto gse::process::run_capture(const capture_request&) -> run_outcome {
	return std::unexpected(run_error::launch_failed);
}
