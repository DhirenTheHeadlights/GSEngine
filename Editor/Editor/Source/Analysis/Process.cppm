export module gse.ide.analysis:process;

import std;
import gse.win32;

export namespace gse::ide::analysis::process {
	struct run_result {
		int exit_code = -1;
		bool launched = false;
		bool timed_out = false;
	};

	auto run_capture_stderr(
		const char* command_line,
		const char* working_dir,
		const char* out_path
	) -> run_result;

	auto open_url(const char* url) -> void;
}

auto gse::ide::analysis::process::open_url(const char* url) -> void {
	wchar_t cmd[2200] = {};
	const wchar_t prefix[] = L"explorer \"";
	int k = 0;
	for (const wchar_t* p = prefix; *p; ++p) {
		cmd[k++] = *p;
	}
	const int n = gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, url, -1, cmd + k, 2000);
	if (n <= 0) {
		return;
	}
	k += n - 1;
	cmd[k++] = L'"';
	cmd[k] = 0;

	gse::win32::STARTUPINFOW si{ .cb = sizeof(gse::win32::STARTUPINFOW) };
	gse::win32::PROCESS_INFORMATION pi{};
	if (gse::win32::CreateProcessW(nullptr, cmd, nullptr, nullptr, 0, 0, nullptr, nullptr, &si, &pi)) {
		gse::win32::CloseHandle(pi.hProcess);
		gse::win32::CloseHandle(pi.hThread);
	}
}

auto gse::ide::analysis::process::run_capture_stderr(const char* command_line, const char* working_dir, const char* out_path) -> run_result {
	wchar_t w_cwd[1024] = {};
	wchar_t w_out[1024] = {};
	gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, working_dir, -1, w_cwd, 1024);
	gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, out_path, -1, w_out, 1024);

	const int cmd_len = gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, command_line, -1, nullptr, 0);
	if (cmd_len <= 0) {
		return {};
	}
	wchar_t* w_cmd = new wchar_t[cmd_len];
	gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, command_line, -1, w_cmd, cmd_len);

	const wchar_t* command_start = w_cmd;
	while (*command_start == L' ') {
		++command_start;
	}
	const bool quoted_command = *command_start == L'"';
	if (quoted_command) {
		++command_start;
	}
	const wchar_t* command_stop = command_start;
	while (*command_stop && (quoted_command ? *command_stop != L'"' : *command_stop != L' ')) {
		++command_stop;
	}
	const wchar_t* directory_stop = command_start;
	for (const wchar_t* cursor = command_start; cursor < command_stop; ++cursor) {
		if (*cursor == L'\\' || *cursor == L'/') {
			directory_stop = cursor;
		}
	}
	std::vector<wchar_t> environment = gse::win32::environment_with_path_prefix(std::wstring_view(
		command_start,
		static_cast<std::size_t>(directory_stop - command_start)
	));

	gse::win32::SECURITY_ATTRIBUTES sa{
		.nLength = sizeof(gse::win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	const gse::win32::HANDLE h_out = gse::win32::CreateFileW(w_out, gse::win32::generic_write, gse::win32::file_share_read, &sa, gse::win32::create_always, gse::win32::file_attribute_normal, nullptr);
	const gse::win32::HANDLE h_nul = gse::win32::CreateFileW(L"NUL", gse::win32::generic_read, gse::win32::file_share_read | gse::win32::file_share_write, &sa, gse::win32::open_existing, 0, nullptr);

	if (!gse::win32::valid_handle(h_out)) {
		delete[] w_cmd;
		if (gse::win32::valid_handle(h_nul)) {
			gse::win32::CloseHandle(h_nul);
		}
		return {};
	}

	gse::win32::HANDLE inherit_list[2] = { h_out, h_nul };
	gse::win32::SIZE_T attr_size = 0;
	gse::win32::InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
	char* attr_mem = new char[attr_size];
	const gse::win32::LPPROC_THREAD_ATTRIBUTE_LIST attr_list = reinterpret_cast<gse::win32::LPPROC_THREAD_ATTRIBUTE_LIST>(attr_mem);
	gse::win32::InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size);
	gse::win32::UpdateProcThreadAttribute(attr_list, 0, gse::win32::proc_thread_attribute_handle_list, inherit_list, sizeof(inherit_list), nullptr, nullptr);

	gse::win32::STARTUPINFOEXW six{
		.StartupInfo = {
			.cb = sizeof(gse::win32::STARTUPINFOEXW),
			.dwFlags = gse::win32::startf_use_std_handles,
			.hStdInput = h_nul,
			.hStdOutput = h_out,
			.hStdError = h_out,
		},
		.lpAttributeList = attr_list,
	};

	gse::win32::PROCESS_INFORMATION pi{};
	const gse::win32::BOOL ok = gse::win32::CreateProcessW(
		nullptr,
		w_cmd,
		nullptr,
		nullptr,
		1,
		gse::win32::extended_startupinfo_present | gse::win32::create_no_window | gse::win32::create_unicode_environment,
		environment.empty() ? nullptr : environment.data(),
		w_cwd,
		&six.StartupInfo,
		&pi
	);
	delete[] w_cmd;
	gse::win32::DeleteProcThreadAttributeList(attr_list);
	delete[] attr_mem;

	run_result result;
	if (ok) {
		result.launched = true;
		if (gse::win32::WaitForSingleObject(pi.hProcess, 60000) == gse::win32::wait_timeout) {
			result.timed_out = true;
			gse::win32::TerminateProcess(pi.hProcess, 1);
			gse::win32::WaitForSingleObject(pi.hProcess, 5000);
		}
		gse::win32::DWORD code = 0;
		gse::win32::GetExitCodeProcess(pi.hProcess, &code);
		result.exit_code = static_cast<int>(code);
		gse::win32::CloseHandle(pi.hProcess);
		gse::win32::CloseHandle(pi.hThread);
	}

	gse::win32::CloseHandle(h_out);
	if (gse::win32::valid_handle(h_nul)) {
		gse::win32::CloseHandle(h_nul);
	}
	return result;
}
