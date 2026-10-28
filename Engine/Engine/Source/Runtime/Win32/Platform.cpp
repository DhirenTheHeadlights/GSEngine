module gse.runtime:bootstrap_platform;

import std;

import :bootstrap;

import gse.win32;

auto gse::enable_dpi_awareness() -> void {
	win32::enable_per_monitor_dpi_awareness();
}

auto gse::open_editor_pipe(const std::string_view name) -> std::expected<void*, std::string> {
	const std::wstring pipe(name.begin(), name.end());
	constexpr int connect_attempts = 100;
	constexpr auto connect_retry_delay = std::chrono::milliseconds(50);
	void* handle = win32::CreateFileW(pipe.c_str(), win32::generic_write | win32::generic_read, 0, nullptr, win32::open_existing, 0, nullptr);
	for (int attempt = 1; attempt < connect_attempts && !win32::valid_handle(handle); ++attempt) {
		const win32::DWORD error = win32::GetLastError();
		if (error != win32::error_pipe_busy && error != win32::error_file_not_found) {
			break;
		}
		std::this_thread::sleep_for(connect_retry_delay);
		handle = win32::CreateFileW(pipe.c_str(), win32::generic_write | win32::generic_read, 0, nullptr, win32::open_existing, 0, nullptr);
	}
	if (!win32::valid_handle(handle)) {
		return std::unexpected(std::format("error {}", win32::GetLastError()));
	}
	return handle;
}

auto gse::command_line_arguments() -> std::vector<std::wstring> {
	int count = 0;
	wchar_t** arguments = win32::CommandLineToArgvW(win32::GetCommandLineW(), &count);
	if (arguments == nullptr) {
		return {};
	}

	std::vector<std::wstring> out;
	out.reserve(static_cast<std::size_t>(count));
	for (int index = 0; index < count; ++index) {
		out.emplace_back(arguments[index]);
	}

	win32::LocalFree(arguments);
	return out;
}

auto gse::close_os_handle(void* const handle) -> void {
	if (win32::valid_handle(handle)) {
		win32::CloseHandle(handle);
	}
}
