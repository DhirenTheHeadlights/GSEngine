export module gse.ide.build:spawn;

import std;
import gse;
import gse.win32;
import gse.win32.environment;

export namespace gse::ide::spawn {
	struct output_stream {
		std::mutex mutex;
		std::vector<std::string> lines;
		std::atomic<bool> running = false;
		std::atomic<bool> terminated = false;
		void* process = nullptr;
		void* job = nullptr;
	};

	auto emit(output_stream& stream, std::string text) -> void;

	auto attach_process(output_stream& stream, void* process, void* job) -> void;

	auto close_process(output_stream& stream) -> void;

	auto terminate_process(output_stream& stream) -> void;

	auto run_capture(
		output_stream& stream,
		const std::wstring& command_line,
		const std::wstring& working_dir,
		const std::filesystem::path& path_prefix
	) -> int;
}

auto gse::ide::spawn::emit(output_stream& stream, std::string text) -> void {
	std::lock_guard lock(stream.mutex);
	stream.lines.push_back(std::move(text));
}

auto gse::ide::spawn::attach_process(output_stream& stream, void* process, void* job) -> void {
	std::lock_guard lock(stream.mutex);
	stream.process = process;
	if (win32::valid_handle(stream.job)) {
		win32::CloseHandle(stream.job);
	}
	stream.job = job;
}

auto gse::ide::spawn::close_process(output_stream& stream) -> void {
	std::lock_guard lock(stream.mutex);
	stream.process = nullptr;
	if (win32::valid_handle(stream.job)) {
		win32::CloseHandle(stream.job);
		stream.job = nullptr;
	}
	stream.running.store(false, std::memory_order_release);
}

auto gse::ide::spawn::terminate_process(output_stream& stream) -> void {
	std::lock_guard lock(stream.mutex);
	stream.terminated.store(true, std::memory_order_release);
	if (win32::valid_handle(stream.job)) {
		win32::TerminateJobObject(stream.job, 1);
	}
	else if (win32::valid_handle(stream.process)) {
		win32::TerminateProcess(stream.process, 1);
	}
}

auto gse::ide::spawn::run_capture(
	output_stream& stream,
	const std::wstring& command_line,
	const std::wstring& working_dir,
	const std::filesystem::path& path_prefix
) -> int {
	stream.terminated.store(false, std::memory_order_release);

	win32::SECURITY_ATTRIBUTES attributes{
		.nLength = sizeof(win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	void* read_end = nullptr;
	void* write_end = nullptr;
	if (!win32::CreatePipe(&read_end, &write_end, &attributes, 0)) {
		emit(stream, "failed to create output pipe");
		return -1;
	}
	const auto close_read = make_scope_exit([read_end] {
		win32::CloseHandle(read_end);
	});
	win32::SetHandleInformation(read_end, win32::handle_flag_inherit, 0);

	win32::SIZE_T attribute_size = 0;
	win32::InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
	if (attribute_size == 0) {
		win32::CloseHandle(write_end);
		emit(stream, "failed to prepare process attributes");
		return -1;
	}
	std::vector<std::byte> attribute_memory(attribute_size);
	const win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = reinterpret_cast<win32::LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_memory.data());
	if (!win32::InitializeProcThreadAttributeList(attribute_list, 1, 0, &attribute_size)) {
		win32::CloseHandle(write_end);
		emit(stream, "failed to prepare process attributes");
		return -1;
	}
	const auto delete_attribute_list = make_scope_exit([attribute_list] {
		win32::DeleteProcThreadAttributeList(attribute_list);
	});
	if (!win32::UpdateProcThreadAttribute(attribute_list, 0, win32::proc_thread_attribute_handle_list, &write_end, sizeof(write_end), nullptr, nullptr)) {
		win32::CloseHandle(write_end);
		emit(stream, "failed to prepare process attributes");
		return -1;
	}

	void* job = win32::CreateJobObjectW(nullptr, nullptr);
	if (!win32::valid_handle(job)) {
		win32::CloseHandle(write_end);
		emit(stream, "failed to create job object");
		return -1;
	}
	win32::JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{
		.BasicLimitInformation = {
			.LimitFlags = win32::job_object_limit_kill_on_job_close,
		},
	};
	win32::SetInformationJobObject(job, win32::job_object_extended_limit_information, &limits, sizeof(limits));

	std::vector<wchar_t> command_buffer(command_line.begin(), command_line.end());
	command_buffer.push_back(0);
	std::vector<wchar_t> environment = win32::environment_with_path_prefix(path_prefix.wstring());

	win32::STARTUPINFOEXW startup{
		.StartupInfo = {
			.cb = sizeof(win32::STARTUPINFOEXW),
			.dwFlags = win32::startf_use_std_handles,
			.hStdOutput = write_end,
			.hStdError = write_end,
		},
		.lpAttributeList = attribute_list,
	};

	win32::PROCESS_INFORMATION process{};
	const int spawned = win32::CreateProcessW(
		nullptr,
		command_buffer.data(),
		nullptr,
		nullptr,
		1,
		win32::extended_startupinfo_present | win32::create_no_window | win32::create_unicode_environment | win32::create_suspended,
		environment.empty() ? nullptr : environment.data(),
		working_dir.empty() ? nullptr : working_dir.c_str(),
		&startup.StartupInfo,
		&process
	);

	win32::CloseHandle(write_end);

	if (!spawned) {
		win32::CloseHandle(job);
		emit(stream, "failed to launch process");
		return -1;
	}

	if (!win32::AssignProcessToJobObject(job, process.hProcess)) {
		win32::TerminateProcess(process.hProcess, 1);
	}
	win32::ResumeThread(process.hThread);
	win32::CloseHandle(process.hThread);

	attach_process(stream, process.hProcess, job);
	if (stream.terminated.load(std::memory_order_acquire)) {
		win32::TerminateJobObject(job, 1);
	}

	std::string pending;
	std::array<char, 4096> chunk{};
	win32::DWORD received = 0;
	while (win32::ReadFile(read_end, chunk.data(), static_cast<win32::DWORD>(chunk.size()), &received, nullptr) && received > 0) {
		pending.append(chunk.data(), received);
		for (std::size_t newline = pending.find('\n'); newline != std::string::npos; newline = pending.find('\n')) {
			std::string text = pending.substr(0, newline);
			if (!text.empty() && text.back() == '\r') {
				text.pop_back();
			}
			emit(stream, std::move(text));
			pending.erase(0, newline + 1);
		}
	}
	if (!pending.empty()) {
		emit(stream, std::move(pending));
	}

	win32::WaitForSingleObject(process.hProcess, win32::infinite);
	win32::DWORD code = 0;
	win32::GetExitCodeProcess(process.hProcess, &code);
	{
		std::lock_guard lock(stream.mutex);
		stream.process = nullptr;
	}
	win32::CloseHandle(process.hProcess);
	return static_cast<int>(code);
}
