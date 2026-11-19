module gse.process;

import std;

extern "C" auto gse_host_open_url(const char* url, std::size_t length) -> void;

auto gse::process::open_url(const std::string_view url) -> void {
	gse_host_open_url(url.data(), url.size());
}

auto gse::process::run_capture(const capture_request&) -> run_outcome {
	return std::unexpected(run_error::launch_failed);
}
