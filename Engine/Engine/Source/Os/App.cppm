module;

#ifdef _WIN32
#include <x86intrin.h>
#else
#include <unistd.h>
#endif

export module gse.os:app;

import std;

import gse.win32;

export namespace gse::app {
	auto restart() -> void;
}

auto gse::app::restart() -> void {
#ifdef _WIN32
	using namespace gse::win32;

	wchar_t path[max_path];
	GetModuleFileNameW(nullptr, path, max_path);

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};

	if (CreateProcessW(path, GetCommandLineW(), nullptr, nullptr, 0, 0, nullptr, nullptr, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		ExitProcess(0);
	}
#else
	char path[4096];
	const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (len != -1) {
		path[len] = '\0';
		char* args[] = { path, nullptr };
		execv(path, args);
	}
#endif
}
