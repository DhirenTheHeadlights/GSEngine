module;

#ifndef _WIN32
#include <unistd.h>
#endif

export module gse.os:app;

import gse.log;
import gse.win32;
import std;

export namespace gse::app {
	auto relaunch_on_exit(
		std::filesystem::path executable,
		std::filesystem::path working_dir = {},
		std::vector<std::filesystem::path> arguments = {}
	) -> void;

	auto relaunch_self_on_exit() -> void;

	auto relaunch_pending() -> bool;

	auto pin_relaunch_argument(
		std::wstring argument
	) -> void;

	auto add_relaunch_handoff(
		std::span<void* const> handles,
		std::wstring argument
	) -> void;

	auto drop_relaunch_arguments(
		std::wstring prefix
	) -> void;

	auto run_pending_relaunch() -> void;
}

namespace gse::app {
	std::mutex relaunch_mutex;
	bool relaunch_queued = false;
	bool relaunch_self = false;
	std::filesystem::path relaunch_executable;
	std::filesystem::path relaunch_working_dir;
	std::vector<std::filesystem::path> relaunch_arguments;
	std::vector<void*> relaunch_handles;
	std::vector<std::wstring> relaunch_handoff_arguments;
	std::vector<std::wstring> relaunch_dropped_prefixes;
	std::vector<std::wstring> relaunch_pinned_arguments;

#ifdef _WIN32
	constexpr int relaunch_attempts = 20;
	constexpr auto relaunch_retry_delay = std::chrono::milliseconds(250);

	auto launch_process(
		const std::filesystem::path& display,
		const wchar_t* application,
		std::vector<wchar_t>& command,
		const wchar_t* working,
		win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list
	) -> void;
#endif
}

#ifdef _WIN32
auto gse::app::launch_process(const std::filesystem::path& display, const wchar_t* application, std::vector<wchar_t>& command, const wchar_t* working, const win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list) -> void {
	using namespace gse::win32;

	STARTUPINFOEXW startup{
		.StartupInfo = {
			.cb = static_cast<DWORD>(attribute_list != nullptr ? sizeof(STARTUPINFOEXW) : sizeof(STARTUPINFOW)),
		},
		.lpAttributeList = attribute_list,
	};
	const int inherit = attribute_list != nullptr ? 1 : 0;
	const DWORD creation_flags = attribute_list != nullptr ? extended_startupinfo_present : 0u;

	DWORD error = 0;
	for (int attempt = 0; attempt < relaunch_attempts; ++attempt) {
		PROCESS_INFORMATION process{};
		if (CreateProcessW(application, command.data(), nullptr, nullptr, inherit, creation_flags, nullptr, working, &startup.StartupInfo, &process)) {
			CloseHandle(process.hProcess);
			CloseHandle(process.hThread);
			if (attempt > 0) {
				log::println(log::level::warning, log::category::general, "relaunch: started '{}' on attempt {} (last win32 error {})", display.generic_display_string(), attempt + 1, error);
			}
			return;
		}
		error = GetLastError();
		std::this_thread::sleep_for(relaunch_retry_delay);
	}

	log::println(log::level::error, log::category::general, "relaunch: could not start '{}' after {} attempts (win32 error {})", display.generic_display_string(), relaunch_attempts, error);
}
#endif

auto gse::app::relaunch_on_exit(std::filesystem::path executable, std::filesystem::path working_dir, std::vector<std::filesystem::path> arguments) -> void {
	std::lock_guard _(relaunch_mutex);
	relaunch_executable = std::move(executable);
	relaunch_working_dir = std::move(working_dir);
	relaunch_arguments = std::move(arguments);
	relaunch_handles.clear();
	relaunch_handoff_arguments.clear();
	relaunch_dropped_prefixes.clear();
	relaunch_self = false;
	relaunch_queued = true;
}

auto gse::app::relaunch_self_on_exit() -> void {
	std::lock_guard _(relaunch_mutex);
	relaunch_executable.clear();
	relaunch_working_dir.clear();
	relaunch_arguments.clear();
	relaunch_handles.clear();
	relaunch_handoff_arguments.clear();
	relaunch_dropped_prefixes.clear();
	relaunch_self = true;
	relaunch_queued = true;
}

auto gse::app::relaunch_pending() -> bool {
	std::lock_guard _(relaunch_mutex);
	return relaunch_queued;
}

auto gse::app::pin_relaunch_argument(std::wstring argument) -> void {
	std::lock_guard _(relaunch_mutex);
	if (std::ranges::find(relaunch_pinned_arguments, argument) == relaunch_pinned_arguments.end()) {
		relaunch_pinned_arguments.push_back(std::move(argument));
	}
}

auto gse::app::add_relaunch_handoff(const std::span<void* const> handles, std::wstring argument) -> void {
	std::lock_guard _(relaunch_mutex);
	if (!relaunch_queued) {
		return;
	}
	relaunch_handles.insert(relaunch_handles.end(), handles.begin(), handles.end());
	relaunch_handoff_arguments.push_back(std::move(argument));
}

auto gse::app::drop_relaunch_arguments(std::wstring prefix) -> void {
	std::lock_guard _(relaunch_mutex);
	if (!relaunch_queued || prefix.empty()) {
		return;
	}
	if (std::ranges::find(relaunch_dropped_prefixes, prefix) == relaunch_dropped_prefixes.end()) {
		relaunch_dropped_prefixes.push_back(std::move(prefix));
	}
}

auto gse::app::run_pending_relaunch() -> void {
	std::filesystem::path executable;
	std::filesystem::path working_dir;
	std::vector<std::filesystem::path> arguments;
	std::vector<void*> handles;
	std::vector<std::wstring> handoff_arguments;
	std::vector<std::wstring> dropped_prefixes;
	std::vector<std::wstring> pinned_arguments;
	bool self = false;
	{
		std::lock_guard _(relaunch_mutex);
		if (!relaunch_queued) {
			return;
		}
		relaunch_queued = false;
		self = std::exchange(relaunch_self, false);
		executable = std::move(relaunch_executable);
		working_dir = std::move(relaunch_working_dir);
		arguments = std::move(relaunch_arguments);
		handles = std::move(relaunch_handles);
		handoff_arguments = std::move(relaunch_handoff_arguments);
		dropped_prefixes = std::move(relaunch_dropped_prefixes);
		pinned_arguments = relaunch_pinned_arguments;
	}

#ifdef _WIN32
	using namespace gse::win32;

	std::vector<std::byte> attribute_memory;
	LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = nullptr;
	if (!handles.empty()) {
		for (void* handle : handles) {
			SetHandleInformation(handle, handle_flag_inherit, handle_flag_inherit);
		}

		SIZE_T attribute_size = 0;
		InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
		if (attribute_size != 0) {
			attribute_memory.resize(attribute_size);
			attribute_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_memory.data());
			const bool prepared = InitializeProcThreadAttributeList(attribute_list, 1, 0, &attribute_size)
				&& UpdateProcThreadAttribute(attribute_list, 0, proc_thread_attribute_handle_list, handles.data(), handles.size() * sizeof(void*), nullptr, nullptr);
			if (!prepared) {
				attribute_list = nullptr;
			}
		}
	}

	if (self) {
		wchar_t path[max_path]{};
		const DWORD length = GetModuleFileNameW(nullptr, path, max_path);
		if (length == 0 || length >= max_path) {
			return;
		}

		std::wstring line(GetCommandLineW());
		for (const std::wstring& prefix : dropped_prefixes) {
			for (std::size_t at = line.find(prefix); at != std::wstring::npos; at = line.find(prefix, at)) {
				const std::size_t end = line.find_first_of(L" \t", at);
				const std::size_t begin = at > 0 && (line[at - 1] == L' ' || line[at - 1] == L'\t') ? at - 1 : at;
				line.erase(begin, end == std::wstring::npos ? std::wstring::npos : end - begin);
				at = begin;
			}
		}
		for (const std::wstring& argument : pinned_arguments) {
			if (line.find(argument) == std::wstring::npos) {
				line += L" " + argument;
			}
		}
		for (const std::wstring& argument : handoff_arguments) {
			line += L" " + argument;
		}
		std::vector<wchar_t> self_command(line.begin(), line.end());
		self_command.push_back(0);

		launch_process(path, path, self_command, nullptr, attribute_list);
		if (attribute_list != nullptr) {
			DeleteProcThreadAttributeList(attribute_list);
		}
		return;
	}

	std::wstring command = L"\"" + executable.wstring() + L"\"";
	for (const std::filesystem::path& argument : arguments) {
		command += L" \"" + argument.wstring() + L"\"";
	}
	if (arguments.empty()) {
		for (const std::wstring& argument : pinned_arguments) {
			command += L" " + argument;
		}
	}
	for (const std::wstring& argument : handoff_arguments) {
		command += L" " + argument;
	}
	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(0);

	const std::wstring working = working_dir.wstring();

	launch_process(executable, nullptr, command_buffer, working.empty() ? nullptr : working.c_str(), attribute_list);
	if (attribute_list != nullptr) {
		DeleteProcThreadAttributeList(attribute_list);
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