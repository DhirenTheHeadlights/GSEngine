export module gse.ide.analysis:process;

import std;
import gse.core;
import gse.math;
import gse.win32;
import gse.win32.environment;

export namespace gse::ide::analysis::process {
	enum class run_error {
		launch_failed,
		timed_out,
		cancelled,
	};

	using run_outcome = std::expected<int, run_error>;

	auto run_capture_stderr(
		std::string_view command_line,
		const std::filesystem::path& working_dir,
		const std::filesystem::path& out_path,
		std::stop_token cancel = {}
	) -> run_outcome;

	auto open_url(
		std::string_view url
	) -> void;

	auto temporary_path(
		std::string_view tag,
		std::string_view extension
	) -> std::filesystem::path;
}

namespace gse::ide::analysis::process {
	class unique_handle : non_copyable {
	public:
		unique_handle() = default;

		explicit unique_handle(
			win32::HANDLE handle
		);

		~unique_handle();

		auto valid() const -> bool;

		auto handle() const -> win32::HANDLE;

	private:
		win32::HANDLE m_handle = nullptr;
	};

	auto to_wide(
		std::string_view text
	) -> std::wstring;

	auto command_directory(
		std::wstring_view command_line
	) -> std::wstring_view;
}

gse::ide::analysis::process::unique_handle::unique_handle(const win32::HANDLE handle) : m_handle(handle) {}

gse::ide::analysis::process::unique_handle::~unique_handle() {
	if (win32::valid_handle(m_handle)) {
		win32::CloseHandle(m_handle);
	}
}

auto gse::ide::analysis::process::unique_handle::valid() const -> bool {
	return win32::valid_handle(m_handle);
}

auto gse::ide::analysis::process::unique_handle::handle() const -> win32::HANDLE {
	return m_handle;
}

auto gse::ide::analysis::process::to_wide(const std::string_view text) -> std::wstring {
	if (text.empty()) {
		return {};
	}
	const int length = win32::MultiByteToWideChar(win32::cp_utf8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) {
		return {};
	}
	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	if (win32::MultiByteToWideChar(win32::cp_utf8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length) <= 0) {
		return {};
	}
	return wide;
}

auto gse::ide::analysis::process::command_directory(const std::wstring_view command_line) -> std::wstring_view {
	const std::size_t begin = command_line.find_first_not_of(L' ');
	if (begin == std::wstring_view::npos) {
		return {};
	}
	const bool quoted = command_line[begin] == L'"';
	const std::size_t start = quoted ? begin + 1 : begin;
	const std::size_t stop = command_line.find(quoted ? L'"' : L' ', start);
	const std::wstring_view executable = command_line.substr(start, stop == std::wstring_view::npos ? std::wstring_view::npos : stop - start);
	const std::size_t separator = executable.find_last_of(L"\\/");
	return separator == std::wstring_view::npos ? std::wstring_view{} : executable.substr(0, separator);
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

auto gse::ide::analysis::process::open_url(const std::string_view url) -> void {
	std::wstring command = L"explorer \"" + to_wide(url) + L"\"";
	win32::STARTUPINFOW startup{
		.cb = sizeof(win32::STARTUPINFOW),
	};
	win32::PROCESS_INFORMATION info{};
	if (win32::CreateProcessW(nullptr, command.data(), nullptr, nullptr, 0, 0, nullptr, nullptr, &startup, &info)) {
		const unique_handle child(info.hProcess);
		const unique_handle main_thread(info.hThread);
	}
}

auto gse::ide::analysis::process::run_capture_stderr(const std::string_view command_line, const std::filesystem::path& working_dir, const std::filesystem::path& out_path, std::stop_token cancel) -> run_outcome {
	std::wstring command = to_wide(command_line);
	if (command.empty()) {
		return std::unexpected(run_error::launch_failed);
	}

	const std::wstring directory = working_dir.wstring();
	const std::wstring output = out_path.wstring();
	std::vector<wchar_t> environment = win32::environment_with_path_prefix(command_directory(command));

	win32::SECURITY_ATTRIBUTES inheritable{
		.nLength = sizeof(win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	const unique_handle capture(win32::CreateFileW(output.c_str(), win32::generic_write, win32::file_share_read, &inheritable, win32::create_always, win32::file_attribute_normal, nullptr));
	const unique_handle null_input(win32::CreateFileW(L"NUL", win32::generic_read, win32::file_share_read | win32::file_share_write, &inheritable, win32::open_existing, 0, nullptr));
	if (!capture.valid() || !null_input.valid()) {
		return std::unexpected(run_error::launch_failed);
	}

	std::array<win32::HANDLE, 2> inherited = { capture.handle(), null_input.handle() };
	win32::SIZE_T attribute_size = 0;
	win32::InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
	if (attribute_size == 0) {
		return std::unexpected(run_error::launch_failed);
	}
	std::vector<std::byte> attribute_memory(attribute_size);
	const win32::LPPROC_THREAD_ATTRIBUTE_LIST attributes = reinterpret_cast<win32::LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_memory.data());
	if (!win32::InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)) {
		return std::unexpected(run_error::launch_failed);
	}
	const auto delete_attributes = make_scope_exit([attributes] {
		win32::DeleteProcThreadAttributeList(attributes);
	});
	if (!win32::UpdateProcThreadAttribute(attributes, 0, win32::proc_thread_attribute_handle_list, inherited.data(), inherited.size() * sizeof(win32::HANDLE), nullptr, nullptr)) {
		return std::unexpected(run_error::launch_failed);
	}

	const unique_handle job(win32::CreateJobObjectW(nullptr, nullptr));
	if (!job.valid()) {
		return std::unexpected(run_error::launch_failed);
	}

	win32::STARTUPINFOEXW startup{
		.StartupInfo = {
			.cb = sizeof(win32::STARTUPINFOEXW),
			.dwFlags = win32::startf_use_std_handles,
			.hStdInput = null_input.handle(),
			.hStdOutput = capture.handle(),
			.hStdError = capture.handle(),
		},
		.lpAttributeList = attributes,
	};

	win32::PROCESS_INFORMATION info{};
	const win32::BOOL created = win32::CreateProcessW(
		nullptr,
		command.data(),
		nullptr,
		nullptr,
		1,
		win32::extended_startupinfo_present | win32::create_no_window | win32::create_unicode_environment | win32::create_suspended,
		environment.empty() ? nullptr : environment.data(),
		directory.empty() ? nullptr : directory.c_str(),
		&startup.StartupInfo,
		&info
	);
	if (!created) {
		return std::unexpected(run_error::launch_failed);
	}
	const unique_handle child(info.hProcess);
	const unique_handle main_thread(info.hThread);

	if (!win32::AssignProcessToJobObject(job.handle(), child.handle())) {
		win32::TerminateProcess(child.handle(), 1);
		win32::WaitForSingleObject(child.handle(), win32::infinite);
		return std::unexpected(run_error::launch_failed);
	}
	if (win32::ResumeThread(main_thread.handle()) == std::numeric_limits<win32::DWORD>::max()) {
		win32::TerminateJobObject(job.handle(), 1);
		win32::WaitForSingleObject(child.handle(), win32::infinite);
		return std::unexpected(run_error::launch_failed);
	}

	const time limit = seconds(60.f);
	const time slice = milliseconds(50.f);
	for (time waited = seconds(0.f); win32::WaitForSingleObject(child.handle(), static_cast<win32::DWORD>(slice.as<milliseconds>())) == win32::wait_timeout; waited += slice) {
		const bool expired = waited + slice >= limit;
		if (!expired && !cancel.stop_requested()) {
			continue;
		}
		win32::TerminateJobObject(job.handle(), 1);
		win32::WaitForSingleObject(child.handle(), win32::infinite);
		return std::unexpected(expired ? run_error::timed_out : run_error::cancelled);
	}

	win32::DWORD code = 0;
	win32::GetExitCodeProcess(child.handle(), &code);
	return static_cast<int>(code);
}
