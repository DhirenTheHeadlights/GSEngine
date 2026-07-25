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

	auto relaunch_on_exit(
		std::filesystem::path executable,
		std::filesystem::path working_dir = {},
		std::vector<std::filesystem::path> arguments = {}
	) -> void;

	auto run_pending_relaunch() -> void;
}

namespace gse::app {
	std::mutex relaunch_mutex;
	bool relaunch_pending = false;
	std::filesystem::path relaunch_executable;
	std::filesystem::path relaunch_working_dir;
	std::vector<std::filesystem::path> relaunch_arguments;
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

auto gse::app::relaunch_on_exit(std::filesystem::path executable, std::filesystem::path working_dir, std::vector<std::filesystem::path> arguments) -> void {
	std::lock_guard lock(relaunch_mutex);
	relaunch_executable = std::move(executable);
	relaunch_working_dir = std::move(working_dir);
	relaunch_arguments = std::move(arguments);
	relaunch_pending = true;
}

auto gse::app::run_pending_relaunch() -> void {
	std::filesystem::path executable;
	std::filesystem::path working_dir;
	std::vector<std::filesystem::path> arguments;
	{
		std::lock_guard lock(relaunch_mutex);
		if (!relaunch_pending) {
			return;
		}
		relaunch_pending = false;
		executable = std::move(relaunch_executable);
		working_dir = std::move(relaunch_working_dir);
		arguments = std::move(relaunch_arguments);
	}

#ifdef _WIN32
	using namespace gse::win32;

	std::wstring command = L"\"" + executable.wstring() + L"\"";
	for (const std::filesystem::path& argument : arguments) {
		command += L" \"" + argument.wstring() + L"\"";
	}
	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(0);

	const std::wstring working = working_dir.wstring();

	STARTUPINFOW startup{
		.cb = sizeof(STARTUPINFOW),
	};
	PROCESS_INFORMATION process{};
	if (CreateProcessW(nullptr, command_buffer.data(), nullptr, nullptr, 0, 0, nullptr, working.empty() ? nullptr : working.c_str(), &startup, &process)) {
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);
	}
#else
	std::vector<std::string> storage;
	storage.push_back(executable.string());
	for (const std::filesystem::path& argument : arguments) {
		storage.push_back(argument.string());
	}

	std::vector<char*> argv;
	argv.reserve(storage.size() + 1);
	for (std::string& entry : storage) {
		argv.push_back(entry.data());
	}
	argv.push_back(nullptr);

	execv(storage.front().c_str(), argv.data());
#endif
}
