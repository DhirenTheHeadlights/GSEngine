export module gse.ide.analysis:process;

import std;
import gse.core;
import gse.win32;
import gse.win32.environment;

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
	auto temporary_path(
		std::string_view tag,
		std::string_view extension
	) -> std::filesystem::path;
}

auto gse::ide::analysis::process::temporary_path(const std::string_view tag, const std::string_view extension) -> std::filesystem::path {
	std::random_device random;
	const std::filesystem::path directory = std::filesystem::temp_directory_path();
	for (;;) {
		const std::filesystem::path candidate = directory / std::format(
			"gseditor_{}_{:08x}{:08x}{:08x}{:08x}.{}",
			tag, random(), random(), random(), random(), extension
		);
		std::error_code ec;
		if (!std::filesystem::exists(candidate, ec)) {
			return candidate;
		}
	}
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

	gse::win32::STARTUPINFOW si{
		.cb = sizeof(gse::win32::STARTUPINFOW),
	};
	gse::win32::PROCESS_INFORMATION pi{};
	if (gse::win32::CreateProcessW(nullptr, cmd, nullptr, nullptr, 0, 0, nullptr, nullptr, &si, &pi)) {
		gse::win32::CloseHandle(pi.hProcess);
		gse::win32::CloseHandle(pi.hThread);
	}
}

auto gse::ide::analysis::process::run_capture_stderr(const char* command_line, const char* working_dir, const char* out_path) -> run_result {
	wchar_t w_cwd[1024] = {};
	wchar_t w_out[1024] = {};
	const int cwd_length = gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, working_dir, -1, w_cwd, 1024);
	const int out_length = gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, out_path, -1, w_out, 1024);
	if (cwd_length <= 0 || out_length <= 0) {
		return {};
	}

	const int cmd_len = gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, command_line, -1, nullptr, 0);
	if (cmd_len <= 0) {
		return {};
	}
	std::vector<wchar_t> w_cmd(static_cast<std::size_t>(cmd_len));
	if (gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, command_line, -1, w_cmd.data(), cmd_len) <= 0) {
		return {};
	}

	const wchar_t* command_start = w_cmd.data();
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

	const gse::win32::HANDLE out = gse::win32::CreateFileW(w_out, gse::win32::generic_write, gse::win32::file_share_read, &sa, gse::win32::create_always, gse::win32::file_attribute_normal, nullptr);
	const auto close_out = gse::make_scope_exit([out] {
		if (gse::win32::valid_handle(out)) {
			gse::win32::CloseHandle(out);
		}
	});
	const gse::win32::HANDLE nul = gse::win32::CreateFileW(L"NUL", gse::win32::generic_read, gse::win32::file_share_read | gse::win32::file_share_write, &sa, gse::win32::open_existing, 0, nullptr);
	const auto close_nul = gse::make_scope_exit([nul] {
		if (gse::win32::valid_handle(nul)) {
			gse::win32::CloseHandle(nul);
		}
	});

	if (!gse::win32::valid_handle(out) || !gse::win32::valid_handle(nul)) {
		return {};
	}

	gse::win32::HANDLE inherit_list[2] = { out, nul };
	gse::win32::SIZE_T attr_size = 0;
	gse::win32::InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
	if (attr_size == 0) {
		return {};
	}
	std::vector<std::byte> attr_memory(attr_size);
	const gse::win32::LPPROC_THREAD_ATTRIBUTE_LIST attr_list = reinterpret_cast<gse::win32::LPPROC_THREAD_ATTRIBUTE_LIST>(attr_memory.data());
	if (!gse::win32::InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size)) {
		return {};
	}
	const auto delete_attr_list = gse::make_scope_exit([attr_list] {
		gse::win32::DeleteProcThreadAttributeList(attr_list);
	});
	if (!gse::win32::UpdateProcThreadAttribute(attr_list, 0, gse::win32::proc_thread_attribute_handle_list, inherit_list, sizeof(inherit_list), nullptr, nullptr)) {
		return {};
	}

	const gse::win32::HANDLE job = gse::win32::CreateJobObjectW(nullptr, nullptr);
	const auto close_job = gse::make_scope_exit([job] {
		if (gse::win32::valid_handle(job)) {
			gse::win32::CloseHandle(job);
		}
	});
	if (!gse::win32::valid_handle(job)) {
		return {};
	}

	gse::win32::STARTUPINFOEXW six{
		.StartupInfo = {
			.cb = sizeof(gse::win32::STARTUPINFOEXW),
			.dwFlags = gse::win32::startf_use_std_handles,
			.hStdInput = nul,
			.hStdOutput = out,
			.hStdError = out,
		},
		.lpAttributeList = attr_list,
	};

	gse::win32::PROCESS_INFORMATION pi{};
	const gse::win32::BOOL ok = gse::win32::CreateProcessW(
		nullptr,
		w_cmd.data(),
		nullptr,
		nullptr,
		1,
		gse::win32::extended_startupinfo_present | gse::win32::create_no_window | gse::win32::create_unicode_environment | gse::win32::create_suspended,
		environment.empty() ? nullptr : environment.data(),
		w_cwd,
		&six.StartupInfo,
		&pi
	);
	if (!ok) {
		return {};
	}
	const auto close_process = gse::make_scope_exit([process = pi.hProcess] {
		gse::win32::CloseHandle(process);
	});
	const auto close_thread = gse::make_scope_exit([thread = pi.hThread] {
		gse::win32::CloseHandle(thread);
	});

	if (!gse::win32::AssignProcessToJobObject(job, pi.hProcess)) {
		gse::win32::TerminateProcess(pi.hProcess, 1);
		gse::win32::WaitForSingleObject(pi.hProcess, gse::win32::infinite);
		return {};
	}
	if (gse::win32::ResumeThread(pi.hThread) == std::numeric_limits<gse::win32::DWORD>::max()) {
		gse::win32::TerminateJobObject(job, 1);
		gse::win32::WaitForSingleObject(pi.hProcess, gse::win32::infinite);
		return {};
	}

	run_result result{
		.launched = true,
	};
	if (gse::win32::WaitForSingleObject(pi.hProcess, 60000) == gse::win32::wait_timeout) {
		result.timed_out = true;
		gse::win32::TerminateJobObject(job, 1);
		gse::win32::WaitForSingleObject(pi.hProcess, gse::win32::infinite);
	}
	gse::win32::DWORD code = 0;
	gse::win32::GetExitCodeProcess(pi.hProcess, &code);
	result.exit_code = static_cast<int>(code);
	return result;
}
