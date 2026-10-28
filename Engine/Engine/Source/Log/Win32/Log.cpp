module gse.log;

import gse.win32;
import gse.win32.environment;
import std;

auto gse::log::process_id_text() -> std::string {
	return std::format("{}", win32::GetCurrentProcessId());
}

auto gse::log::describe_os_thread() -> void {
	win32::SetThreadDescription(win32::GetCurrentThread(), win32::widen(thread_display()).c_str());
}

auto gse::log::console_output() -> bool {
	static const bool console = win32::GetFileType(win32::GetStdHandle(win32::std_output_handle)) == win32::file_type_char;
	return console;
}
