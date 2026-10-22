export module gse.process;

import std;

import gse.math;

export namespace gse::process {
	enum class run_error : std::uint8_t {
		launch_failed,
		timed_out,
		cancelled,
	};

	using run_outcome = std::expected<int, run_error>;

	struct capture_request {
		std::string_view command_line;
		std::filesystem::path working_dir;
		std::filesystem::path output_path;
		std::optional<std::filesystem::path> error_path = {};
		std::filesystem::path path_prefix;
		time limit;
		std::stop_token cancel = {};
		std::optional<std::filesystem::path> input_path = {};
	};

	auto run_capture(
		const capture_request& request
	) -> run_outcome;

	auto open_url(
		std::string_view url
	) -> void;

	auto temporary_path(
		std::string_view tag,
		std::string_view extension
	) -> std::filesystem::path;
}

auto gse::process::temporary_path(const std::string_view tag, const std::string_view extension) -> std::filesystem::path {
	std::random_device random;
	const std::filesystem::path directory = std::filesystem::temp_directory_path();
	for (;;) {
		const std::filesystem::path candidate = directory / std::format(
			"gse_{}_{:08x}{:08x}{:08x}{:08x}.{}",
			tag, random(), random(), random(), random(), extension
		);
		std::error_code ec;
		if (!std::filesystem::exists(candidate, ec)) {
			return candidate;
		}
	}
}
