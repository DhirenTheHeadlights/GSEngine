export module gse.ide.build:spawn;

import std;
import gse;
import gse.win32;
import gse.win32.environment;

export namespace gse::ide::spawn {
	struct output_stream {
		std::mutex mutex;
		std::vector<std::string> lines;
		std::vector<std::string> transcript;
		bool recording = false;
		std::atomic<bool> running = false;
		std::atomic<bool> terminated = false;
		void* process = nullptr;
		void* job = nullptr;
	};

	struct launched {
		void* process = nullptr;
		void* output = nullptr;
		void* input = nullptr;
		void* job = nullptr;
		std::uint32_t pid = 0;
	};

	auto emit(output_stream& stream, std::string text) -> void;

	auto begin_transcript(output_stream& stream) -> void;

	auto take_transcript(output_stream& stream) -> std::vector<std::string>;

	auto attach_process(output_stream& stream, void* process, void* job) -> void;

	auto close_process(output_stream& stream) -> void;

	auto terminate_process(output_stream& stream) -> void;

	auto run_capture(
		output_stream& stream,
		const std::wstring& command_line,
		const std::wstring& working_dir,
		const std::filesystem::path& path_prefix
	) -> int;

	auto launch_streamed(
		const std::wstring& command_line,
		const std::wstring& working_dir,
		std::span<const wchar_t> environment = {}
	) -> launched;

	auto read_lines(
		void* read_end,
		std::string& pending,
		std::vector<std::string>& out
	) -> bool;

	auto pump_output(
		output_stream& stream,
		void* read_end,
		std::string& pending
	) -> bool;
}

namespace gse::ide::spawn {
	auto strip_escapes(
		std::string_view text
	) -> std::string;

	auto split_lines(
		std::string& pending,
		std::vector<std::string>& out
	) -> void;

	auto flush_lines(
		output_stream& stream,
		std::string& pending
	) -> void;
}

auto gse::ide::spawn::strip_escapes(const std::string_view text) -> std::string {
	if (text.find('\x1b') == std::string_view::npos) {
		return std::string(text);
	}
	std::string out;
	out.reserve(text.size());
	for (std::size_t i = 0; i < text.size(); ++i) {
		if (text[i] != '\x1b') {
			out.push_back(text[i]);
			continue;
		}
		if (i + 1 < text.size() && text[i + 1] == '[') {
			i += 2;
			while (i < text.size() && (text[i] < '@' || text[i] > '~')) {
				++i;
			}
			continue;
		}
		++i;
	}
	return out;
}

auto gse::ide::spawn::split_lines(std::string& pending, std::vector<std::string>& out) -> void {
	for (std::size_t newline = pending.find('\n'); newline != std::string::npos; newline = pending.find('\n')) {
		std::string_view text = std::string_view(pending).substr(0, newline);
		if (!text.empty() && text.back() == '\r') {
			text.remove_suffix(1);
		}
		out.emplace_back(text);
		pending.erase(0, newline + 1);
	}
}

auto gse::ide::spawn::flush_lines(output_stream& stream, std::string& pending) -> void {
	std::vector<std::string> lines;
	split_lines(pending, lines);
	for (const std::string& line : lines) {
		emit(stream, strip_escapes(line));
	}
}

auto gse::ide::spawn::emit(output_stream& stream, std::string text) -> void {
	std::lock_guard lock(stream.mutex);
	if (stream.recording) {
		stream.transcript.push_back(text);
	}
	stream.lines.push_back(std::move(text));
}

auto gse::ide::spawn::begin_transcript(output_stream& stream) -> void {
	std::lock_guard lock(stream.mutex);
	stream.transcript.clear();
	stream.recording = true;
}

auto gse::ide::spawn::take_transcript(output_stream& stream) -> std::vector<std::string> {
	std::lock_guard lock(stream.mutex);
	stream.recording = false;
	return std::move(stream.transcript);
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
		flush_lines(stream, pending);
	}
	if (!pending.empty()) {
		emit(stream, strip_escapes(pending));
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

auto gse::ide::spawn::launch_streamed(const std::wstring& command_line, const std::wstring& working_dir, const std::span<const wchar_t> environment) -> launched {
	win32::SECURITY_ATTRIBUTES attributes{
		.nLength = sizeof(win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	void* read_end = nullptr;
	void* write_end = nullptr;
	if (!win32::CreatePipe(&read_end, &write_end, &attributes, 0)) {
		return {};
	}
	win32::SetHandleInformation(read_end, win32::handle_flag_inherit, 0);

	void* input_read = nullptr;
	void* input_write = nullptr;
	if (!win32::CreatePipe(&input_read, &input_write, &attributes, 0)) {
		win32::CloseHandle(read_end);
		win32::CloseHandle(write_end);
		return {};
	}
	win32::SetHandleInformation(input_write, win32::handle_flag_inherit, 0);

	bool adopted = false;
	const auto close_input_read = make_scope_exit([input_read] {
		win32::CloseHandle(input_read);
	});
	const auto close_input_write = make_scope_exit([&adopted, input_write] {
		if (!adopted) {
			win32::CloseHandle(input_write);
		}
	});

	win32::SIZE_T attribute_size = 0;
	win32::InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
	if (attribute_size == 0) {
		win32::CloseHandle(read_end);
		win32::CloseHandle(write_end);
		return {};
	}
	std::vector<std::byte> attribute_memory(attribute_size);
	const win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = reinterpret_cast<win32::LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_memory.data());
	if (!win32::InitializeProcThreadAttributeList(attribute_list, 1, 0, &attribute_size)) {
		win32::CloseHandle(read_end);
		win32::CloseHandle(write_end);
		return {};
	}
	const auto delete_attribute_list = make_scope_exit([attribute_list] {
		win32::DeleteProcThreadAttributeList(attribute_list);
	});
	void* inherited[2] = { write_end, input_read };
	if (!win32::UpdateProcThreadAttribute(attribute_list, 0, win32::proc_thread_attribute_handle_list, inherited, sizeof(inherited), nullptr, nullptr)) {
		win32::CloseHandle(read_end);
		win32::CloseHandle(write_end);
		return {};
	}

	// The child is tied to a job that kills on close, so it cannot outlive the editor
	// even if the editor crashes and never runs its shutdown path.
	void* job = win32::CreateJobObjectW(nullptr, nullptr);
	if (!win32::valid_handle(job)) {
		win32::CloseHandle(read_end);
		win32::CloseHandle(write_end);
		return {};
	}
	win32::JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{
		.BasicLimitInformation = {
			.LimitFlags = win32::job_object_limit_kill_on_job_close,
		},
	};
	win32::SetInformationJobObject(job, win32::job_object_extended_limit_information, &limits, sizeof(limits));

	std::vector<wchar_t> command_buffer(command_line.begin(), command_line.end());
	command_buffer.push_back(0);

	win32::STARTUPINFOEXW startup{
		.StartupInfo = {
			.cb = sizeof(win32::STARTUPINFOEXW),
			.dwFlags = win32::startf_use_std_handles,
			.hStdInput = input_read,
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
		win32::extended_startupinfo_present | win32::create_no_window | win32::create_suspended | (environment.empty() ? 0u : win32::create_unicode_environment),
		environment.empty() ? nullptr : const_cast<wchar_t*>(environment.data()),
		working_dir.empty() ? nullptr : working_dir.c_str(),
		&startup.StartupInfo,
		&process
	);

	win32::CloseHandle(write_end);

	if (!spawned) {
		win32::CloseHandle(read_end);
		win32::CloseHandle(job);
		return {};
	}

	if (!win32::AssignProcessToJobObject(job, process.hProcess)) {
		win32::TerminateProcess(process.hProcess, 1);
		win32::CloseHandle(process.hThread);
		win32::CloseHandle(process.hProcess);
		win32::CloseHandle(read_end);
		win32::CloseHandle(job);
		return {};
	}

	if (win32::ResumeThread(process.hThread) == std::numeric_limits<win32::DWORD>::max()) {
		win32::TerminateJobObject(job, 1);
		win32::CloseHandle(process.hThread);
		win32::CloseHandle(process.hProcess);
		win32::CloseHandle(read_end);
		win32::CloseHandle(job);
		return {};
	}

	win32::CloseHandle(process.hThread);

	adopted = true;
	return {
		.process = process.hProcess,
		.output = read_end,
		.input = input_write,
		.job = job,
		.pid = static_cast<std::uint32_t>(process.dwProcessId),
	};
}

auto gse::ide::spawn::read_lines(void* read_end, std::string& pending, std::vector<std::string>& out) -> bool {
	bool open = win32::valid_handle(read_end);
	std::array<char, 4096> chunk{};
	while (open) {
		win32::DWORD available = 0;
		if (!win32::PeekNamedPipe(read_end, nullptr, 0, nullptr, &available, nullptr)) {
			open = false;
			break;
		}
		if (available == 0) {
			break;
		}
		win32::DWORD received = 0;
		if (!win32::ReadFile(read_end, chunk.data(), std::min(available, static_cast<win32::DWORD>(chunk.size())), &received, nullptr) || received == 0) {
			open = false;
			break;
		}
		pending.append(chunk.data(), received);
	}

	split_lines(pending, out);

	if (!open && !pending.empty()) {
		out.push_back(std::exchange(pending, {}));
	}
	return open;
}

auto gse::ide::spawn::pump_output(output_stream& stream, void* read_end, std::string& pending) -> bool {
	std::vector<std::string> lines;
	const bool open = read_lines(read_end, pending, lines);
	for (const std::string& line : lines) {
		emit(stream, strip_escapes(line));
	}
	return open;
}
