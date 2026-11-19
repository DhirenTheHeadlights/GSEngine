module gse.os:app_impl;

import std;

import gse.log;
import gse.win32;

import :app;

namespace gse::app {
	constexpr int relaunch_attempts = 20;
	constexpr auto relaunch_retry_delay = std::chrono::milliseconds(250);

	auto launch_process(
		const std::filesystem::path& display,
		const wchar_t* application,
		std::vector<wchar_t>& command,
		const wchar_t* working,
		win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list,
		const std::array<void*, 3>& std_handles
	) -> void;
}

auto gse::app::launch_process(const std::filesystem::path& display, const wchar_t* application, std::vector<wchar_t>& command, const wchar_t* working, const win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list, const std::array<void*, 3>& std_handles) -> void {
	using namespace gse::win32;

	const bool pass_std_handles = attribute_list != nullptr && std::ranges::any_of(std_handles, [](void* handle) {
		return valid_handle(handle);
	});
	STARTUPINFOEXW startup{
		.StartupInfo = {
			.cb = static_cast<DWORD>(attribute_list != nullptr ? sizeof(STARTUPINFOEXW) : sizeof(STARTUPINFOW)),
			.dwFlags = pass_std_handles ? startf_use_std_handles : 0u,
			.hStdInput = pass_std_handles ? std_handles[0] : nullptr,
			.hStdOutput = pass_std_handles ? std_handles[1] : nullptr,
			.hStdError = pass_std_handles ? std_handles[2] : nullptr,
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

auto gse::app::start_relaunch(const relaunch_plan& plan) -> void {
	using namespace gse::win32;

	const std::array<void*, 3> std_handles{
		GetStdHandle(std_input_handle),
		GetStdHandle(std_output_handle),
		GetStdHandle(std_error_handle),
	};
	std::vector<void*> handles = plan.handles;
	for (void* handle : std_handles) {
		if (valid_handle(handle) && std::ranges::find(handles, handle) == handles.end()) {
			handles.push_back(handle);
		}
	}

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

	if (plan.self) {
		wchar_t path[max_path]{};
		const DWORD length = GetModuleFileNameW(nullptr, path, max_path);
		if (length == 0 || length >= max_path) {
			return;
		}

		std::wstring line(GetCommandLineW());
		for (const std::wstring& prefix : plan.dropped_prefixes) {
			for (std::size_t at = line.find(prefix); at != std::wstring::npos; at = line.find(prefix, at)) {
				const std::size_t end = line.find_first_of(L" \t", at);
				const std::size_t begin = at > 0 && (line[at - 1] == L' ' || line[at - 1] == L'\t') ? at - 1 : at;
				line.erase(begin, end == std::wstring::npos ? std::wstring::npos : end - begin);
				at = begin;
			}
		}
		for (const std::wstring& argument : plan.pinned_arguments) {
			if (line.find(argument) == std::wstring::npos) {
				line += L" " + argument;
			}
		}
		for (const std::wstring& argument : plan.handoff_arguments) {
			line += L" " + argument;
		}
		std::vector<wchar_t> self_command(line.begin(), line.end());
		self_command.push_back(0);

		launch_process(path, path, self_command, nullptr, attribute_list, std_handles);
		if (attribute_list != nullptr) {
			DeleteProcThreadAttributeList(attribute_list);
		}
		return;
	}

	std::wstring command = L"\"" + plan.executable.wstring() + L"\"";
	for (const std::filesystem::path& argument : plan.arguments) {
		command += L" \"" + argument.wstring() + L"\"";
	}
	if (plan.arguments.empty()) {
		for (const std::wstring& argument : plan.pinned_arguments) {
			command += L" " + argument;
		}
	}
	for (const std::wstring& argument : plan.handoff_arguments) {
		command += L" " + argument;
	}
	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(0);

	const std::wstring working = plan.working_dir.wstring();

	launch_process(plan.executable, nullptr, command_buffer, working.empty() ? nullptr : working.c_str(), attribute_list, std_handles);
	if (attribute_list != nullptr) {
		DeleteProcThreadAttributeList(attribute_list);
	}
}
