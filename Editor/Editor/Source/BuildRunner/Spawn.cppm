export module gse.ide.build:spawn;

import gse;
import gse.win32;
import gse.win32.environment;
import std;

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

	auto terminate(
		void* process,
		void* job
	) -> void;

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
	{
		std::lock_guard _(stream.mutex);
		if (stream.recording) {
			stream.transcript.push_back(text);
		}
		stream.lines.push_back(std::move(text));
	}
	frame_demand::request_redraw();
}

auto gse::ide::spawn::begin_transcript(output_stream& stream) -> void {
	std::lock_guard _(stream.mutex);
	stream.transcript.clear();
	stream.recording = true;
}

auto gse::ide::spawn::take_transcript(output_stream& stream) -> std::vector<std::string> {
	std::lock_guard _(stream.mutex);
	stream.recording = false;
	return std::move(stream.transcript);
}

auto gse::ide::spawn::attach_process(output_stream& stream, void* process, void* job) -> void {
	std::lock_guard _(stream.mutex);
	stream.process = process;
	if (win32::valid_handle(stream.job)) {
		win32::CloseHandle(stream.job);
	}
	stream.job = job;
}

auto gse::ide::spawn::close_process(output_stream& stream) -> void {
	std::lock_guard _(stream.mutex);
	stream.process = nullptr;
	if (win32::valid_handle(stream.job)) {
		win32::CloseHandle(stream.job);
		stream.job = nullptr;
	}
	stream.running.store(false, std::memory_order_release);
}

auto gse::ide::spawn::terminate_process(output_stream& stream) -> void {
	std::lock_guard _(stream.mutex);
	stream.terminated.store(true, std::memory_order_release);
	terminate(stream.process, stream.job);
}

auto gse::ide::spawn::terminate(void* process, void* job) -> void {
	if (win32::valid_handle(job)) {
		win32::TerminateJobObject(job, 1);
	}
	else if (win32::valid_handle(process)) {
		win32::TerminateProcess(process, 1);
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
	const auto _ = make_scope_exit([read_end] {
		win32::CloseHandle(read_end);
	});
	win32::SetHandleInformation(read_end, win32::handle_flag_inherit, 0);

	const std::vector<wchar_t> environment = win32::environment_with_path_prefix(path_prefix.wstring());
	std::optional<os::spawned_process> spawned = os::spawn_in_job({
		.command_line = command_line,
		.working_dir = working_dir,
		.std_output = write_end,
		.environment = environment,
	});

	win32::CloseHandle(write_end);

	if (!spawned) {
		emit(stream, "failed to launch process");
		return -1;
	}

	void* job = spawned->job.release();
	attach_process(stream, spawned->process.handle(), job);
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

	win32::WaitForSingleObject(spawned->process.handle(), win32::infinite);
	const std::uint32_t code = os::exit_code_of(*spawned).value_or(0);
	{
		std::lock_guard _(stream.mutex);
		stream.process = nullptr;
	}
	return static_cast<int>(code);
}

auto gse::ide::spawn::launch_streamed(const std::wstring& command_line, const std::wstring& working_dir, const std::span<const wchar_t> environment) -> launched {
	constexpr win32::DWORD output_buffer_bytes = 4u * 1024u * 1024u;

	win32::SECURITY_ATTRIBUTES attributes{
		.nLength = sizeof(win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	void* read_end = nullptr;
	void* write_end = nullptr;
	if (!win32::CreatePipe(&read_end, &write_end, &attributes, output_buffer_bytes)) {
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
	const auto _ = make_scope_exit([input_read] {
		win32::CloseHandle(input_read);
	});
	const auto _ = make_scope_exit([&adopted, input_write] {
		if (!adopted) {
			win32::CloseHandle(input_write);
		}
	});

	std::optional<os::spawned_process> spawned = os::spawn_in_job({
		.command_line = command_line,
		.working_dir = working_dir,
		.std_input = input_read,
		.std_output = write_end,
		.environment = environment,
	});

	win32::CloseHandle(write_end);

	if (!spawned) {
		win32::CloseHandle(read_end);
		return {};
	}

	adopted = true;
	return {
		.process = spawned->process.release(),
		.output = read_end,
		.input = input_write,
		.job = spawned->job.release(),
		.pid = spawned->pid,
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