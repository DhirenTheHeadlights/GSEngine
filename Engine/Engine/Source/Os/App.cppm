module;

#ifndef _WIN32
#include <unistd.h>
#endif

export module gse.os:app;

import std;

import gse.win32;

export namespace gse::app {
	auto relaunch_on_exit(
		std::filesystem::path executable,
		std::filesystem::path working_dir = {},
		std::vector<std::filesystem::path> arguments = {}
	) -> void;

	auto relaunch_self_on_exit() -> void;

	auto run_pending_relaunch() -> void;
}

namespace gse::app {
	std::mutex relaunch_mutex;
	bool relaunch_pending = false;
	bool relaunch_self = false;
	std::filesystem::path relaunch_executable;
	std::filesystem::path relaunch_working_dir;
	std::vector<std::filesystem::path> relaunch_arguments;
}

auto gse::app::relaunch_on_exit(std::filesystem::path executable, std::filesystem::path working_dir, std::vector<std::filesystem::path> arguments) -> void {
	std::lock_guard lock(relaunch_mutex);
	relaunch_executable = std::move(executable);
	relaunch_working_dir = std::move(working_dir);
	relaunch_arguments = std::move(arguments);
	relaunch_self = false;
	relaunch_pending = true;
}

auto gse::app::relaunch_self_on_exit() -> void {
	std::lock_guard lock(relaunch_mutex);
	relaunch_executable.clear();
	relaunch_working_dir.clear();
	relaunch_arguments.clear();
	relaunch_self = true;
	relaunch_pending = true;
}

auto gse::app::run_pending_relaunch() -> void {
	std::filesystem::path executable;
	std::filesystem::path working_dir;
	std::vector<std::filesystem::path> arguments;
	bool self = false;
	{
		std::lock_guard lock(relaunch_mutex);
		if (!relaunch_pending) {
			return;
		}
		relaunch_pending = false;
		self = std::exchange(relaunch_self, false);
		executable = std::move(relaunch_executable);
		working_dir = std::move(relaunch_working_dir);
		arguments = std::move(relaunch_arguments);
	}

#ifdef _WIN32
	using namespace gse::win32;

	if (self) {
		wchar_t path[max_path]{};
		const DWORD length = GetModuleFileNameW(nullptr, path, max_path);
		if (length == 0 || length >= max_path) {
			return;
		}

		const std::wstring_view line(GetCommandLineW());
		std::vector<wchar_t> self_command(line.begin(), line.end());
		self_command.push_back(0);

		STARTUPINFOW self_startup{
			.cb = sizeof(STARTUPINFOW),
		};
		PROCESS_INFORMATION self_process{};
		if (CreateProcessW(path, self_command.data(), nullptr, nullptr, 0, 0, nullptr, nullptr, &self_startup, &self_process)) {
			CloseHandle(self_process.hProcess);
			CloseHandle(self_process.hThread);
		}
		return;
	}

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
	if (self) {
		char self_path[4096];
		const ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
		if (len != -1) {
			self_path[len] = '\0';
			char* self_args[] = { self_path, nullptr };
			execv(self_path, self_args);
		}
		return;
	}

	std::vector<std::string> storage;
	storage.push_back(executable.native_encoded_string());
	for (const std::filesystem::path& argument : arguments) {
		storage.push_back(argument.native_encoded_string());
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
