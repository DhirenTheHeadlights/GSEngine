module gse.server:console_impl;

import gse.win32;

import :server;

auto gse::server::prepare_dashboard_console() -> void {
	const auto out = win32::GetStdHandle(win32::std_output_handle);
	win32::DWORD mode = 0;
	if (win32::GetConsoleMode(out, &mode)) {
		win32::SetConsoleMode(out, mode | win32::enable_virtual_terminal_processing);
	}
	const auto in = win32::GetStdHandle(win32::std_input_handle);
	win32::DWORD in_mode = 0;
	if (win32::GetConsoleMode(in, &in_mode)) {
		win32::SetConsoleMode(in, (in_mode & ~win32::enable_quick_edit_mode) | win32::enable_extended_flags);
	}
}
