module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

export module gse.ide.analysis:process;

export namespace gse::ide::analysis::process {
	auto run_capture_stderr(
		const char* command_line,
		const char* working_dir,
		const char* out_path
	) -> int;

	auto open_url(const char* url) -> void;
}

auto gse::ide::analysis::process::open_url(const char* url) -> void {
	wchar_t cmd[2200] = {};
	const wchar_t prefix[] = L"explorer \"";
	int k = 0;
	for (const wchar_t* p = prefix; *p; ++p) {
		cmd[k++] = *p;
	}
	const int n = MultiByteToWideChar(CP_UTF8, 0, url, -1, cmd + k, 2000);
	if (n <= 0) {
		return;
	}
	k += n - 1;
	cmd[k++] = L'"';
	cmd[k] = 0;

	STARTUPINFOW si{ .cb = sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION pi{};
	if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

auto gse::ide::analysis::process::run_capture_stderr(const char* command_line, const char* working_dir, const char* out_path) -> int {
	wchar_t w_cwd[1024] = {};
	wchar_t w_out[1024] = {};
	MultiByteToWideChar(CP_UTF8, 0, working_dir, -1, w_cwd, 1024);
	MultiByteToWideChar(CP_UTF8, 0, out_path, -1, w_out, 1024);

	const int cmd_len = MultiByteToWideChar(CP_UTF8, 0, command_line, -1, nullptr, 0);
	if (cmd_len <= 0) {
		return -1;
	}
	wchar_t* w_cmd = new wchar_t[cmd_len];
	MultiByteToWideChar(CP_UTF8, 0, command_line, -1, w_cmd, cmd_len);

	static LONG compiler_path_added = 0;
	if (InterlockedCompareExchange(&compiler_path_added, 1, 0) == 0) {
		const wchar_t* start = w_cmd;
		while (*start == L' ') {
			++start;
		}
		const wchar_t* stop = start;
		while (*stop && *stop != L' ') {
			++stop;
		}
		const wchar_t* sep = start;
		for (const wchar_t* q = start; q < stop; ++q) {
			if (*q == L'\\' || *q == L'/') {
				sep = q;
			}
		}
		if (sep > start) {
			wchar_t old_path[32768] = {};
			GetEnvironmentVariableW(L"PATH", old_path, 32768);
			wchar_t new_path[33200] = {};
			size_t k = 0;
			for (const wchar_t* q = start; q < sep; ++q) {
				new_path[k++] = *q;
			}
			new_path[k++] = L';';
			for (size_t j = 0; old_path[j] != 0 && k < 33199; ++j) {
				new_path[k++] = old_path[j];
			}
			new_path[k] = 0;
			SetEnvironmentVariableW(L"PATH", new_path);
		}
	}

	SECURITY_ATTRIBUTES sa{
		.nLength = sizeof(SECURITY_ATTRIBUTES),
		.bInheritHandle = TRUE,
	};

	const HANDLE h_out = CreateFileW(w_out, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	const HANDLE h_nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

	if (h_out == INVALID_HANDLE_VALUE) {
		delete[] w_cmd;
		if (h_nul != INVALID_HANDLE_VALUE) {
			CloseHandle(h_nul);
		}
		return -1;
	}

	HANDLE inherit_list[2] = { h_out, h_nul };
	SIZE_T attr_size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
	char* attr_mem = new char[attr_size];
	const LPPROC_THREAD_ATTRIBUTE_LIST attr_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_mem);
	InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size);
	UpdateProcThreadAttribute(attr_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit_list, sizeof(inherit_list), nullptr, nullptr);

	STARTUPINFOEXW six{
		.StartupInfo = {
			.cb = sizeof(STARTUPINFOEXW),
			.dwFlags = STARTF_USESTDHANDLES,
			.hStdInput = h_nul,
			.hStdOutput = h_out,
			.hStdError = h_out,
		},
		.lpAttributeList = attr_list,
	};

	PROCESS_INFORMATION pi{};
	const BOOL ok = CreateProcessW(nullptr, w_cmd, nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, w_cwd, &six.StartupInfo, &pi);
	delete[] w_cmd;
	DeleteProcThreadAttributeList(attr_list);
	delete[] attr_mem;

	int result = -1;
	if (ok) {
		if (WaitForSingleObject(pi.hProcess, 60000) == WAIT_TIMEOUT) {
			TerminateProcess(pi.hProcess, 1);
			WaitForSingleObject(pi.hProcess, 5000);
		}
		DWORD code = 0;
		GetExitCodeProcess(pi.hProcess, &code);
		result = static_cast<int>(code);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	CloseHandle(h_out);
	if (h_nul != INVALID_HANDLE_VALUE) {
		CloseHandle(h_nul);
	}
	return result;
}