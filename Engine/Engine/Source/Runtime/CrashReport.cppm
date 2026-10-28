export module gse.runtime:crash_report;

import std;

import gse.config;
import gse.log;
import gse.meta;
import gse.stacktrace;

export namespace gse {
	constexpr std::string_view crash_report_extension = ".crash";
	constexpr std::string_view triaged_report_suffix = ".triaged";
	constexpr std::string_view stale_report_suffix = ".stale";
	constexpr std::string_view binary_field = "binary";

	auto write_crash_report(
		const crash_record& record
	) -> std::filesystem::path;

	auto binary_identity() -> std::string;
}

namespace gse {
	auto crash_report_path() -> std::filesystem::path;
}

auto gse::crash_report_path() -> std::filesystem::path {
	return config::crash_dir() / std::format("{}{}", log::active_file().stem(), crash_report_extension);
}

auto gse::binary_identity() -> std::string {
	std::error_code ec;
	const std::filesystem::path& exe = config::executable_file();
	const std::uintmax_t size = std::filesystem::file_size(exe, ec);
	if (ec) {
		return {};
	}
	const std::filesystem::file_time_type written = std::filesystem::last_write_time(exe, ec);
	if (ec) {
		return {};
	}
	return std::format("{} bytes, written {}", size, written);
}

auto gse::write_crash_report(const crash_record& record) -> std::filesystem::path {
	std::error_code ec;
	std::filesystem::create_directories(config::crash_dir(), ec);
	if (ec) {
		log::println(log::level::error, log::category::general, "crash report: {} could not be created ({})", config::crash_dir(), ec.message());
		return {};
	}

	const std::filesystem::path path = crash_report_path();
	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		log::println(log::level::error, log::category::general, "crash report: {} could not be opened for writing", path);
		return {};
	}

	out << std::format("reason: {}\n", record.reason);
	out << std::format("detail: {}\n", record.detail);
	out << std::format("thread: {}\n", record.thread);
	out << std::format("exe: {}\n", config::executable_file().generic_display_string());
	out << std::format("{}: {}\n", binary_field, binary_identity());
	out << std::format("mode: {}\n", config::mode());
	out << std::format("log: {}\n", log::active_file().generic_display_string());
	out << std::format("module_base: 0x{:x}\n", record.module_base);
	out << "frames:\n";
	for (const std::string& frame : record.frames) {
		out << "  " << frame << '\n';
	}

	if (!out.flush()) {
		log::println(log::level::error, log::category::general, "crash report: {} was not written in full", path);
		return {};
	}
	return path;
}
