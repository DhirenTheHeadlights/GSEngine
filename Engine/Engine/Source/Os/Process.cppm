export module gse.os:process;

import std;

import gse.core;
import gse.math;
import gse.win32;

#ifdef _WIN32
export namespace gse::os {
	class unique_handle : non_copyable {
	public:
		unique_handle() = default;

		explicit unique_handle(
			win32::HANDLE handle
		);

		unique_handle(
			unique_handle&& other
		) noexcept;

		auto operator=(
			unique_handle&& other
		) noexcept -> unique_handle&;

		~unique_handle();

		auto valid() const -> bool;

		auto handle() const -> win32::HANDLE;

		auto release() -> win32::HANDLE;

	private:
		win32::HANDLE m_handle = nullptr;
	};

	struct spawn_request {
		std::wstring command_line;
		std::filesystem::path working_dir;
		win32::HANDLE std_input = nullptr;
		win32::HANDLE std_output = nullptr;
		std::span<const wchar_t> environment;
	};

	struct spawned_process {
		unique_handle process;
		unique_handle job;
		std::uint32_t pid = 0;
	};

	auto spawn_in_job(
		const spawn_request& request
	) -> std::optional<spawned_process>;

	auto command_line_of(
		const std::filesystem::path& executable,
		std::span<const std::string> arguments
	) -> std::wstring;

	auto exit_code_of(
		const spawned_process& spawned
	) -> std::optional<std::uint32_t>;

	auto terminate(
		const spawned_process& spawned
	) -> void;

	auto open_inheritable(
		const std::filesystem::path& path,
		win32::DWORD access,
		win32::DWORD disposition
	) -> unique_handle;

	auto create_output(
		const std::filesystem::path& path
	) -> unique_handle;

	auto try_lock_file(
		const std::filesystem::path& path
	) -> std::optional<unique_handle>;

	auto available_commit() -> byte_count;

	auto current_executable() -> std::filesystem::path;
}

gse::os::unique_handle::unique_handle(const win32::HANDLE handle) : m_handle(handle) {}

gse::os::unique_handle::unique_handle(unique_handle&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

auto gse::os::unique_handle::operator=(unique_handle&& other) noexcept -> unique_handle& {
	if (this != &other) {
		if (win32::valid_handle(m_handle)) {
			win32::CloseHandle(m_handle);
		}
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}

gse::os::unique_handle::~unique_handle() {
	if (win32::valid_handle(m_handle)) {
		win32::CloseHandle(m_handle);
	}
}

auto gse::os::unique_handle::valid() const -> bool {
	return win32::valid_handle(m_handle);
}

auto gse::os::unique_handle::handle() const -> win32::HANDLE {
	return m_handle;
}

auto gse::os::unique_handle::release() -> win32::HANDLE {
	return std::exchange(m_handle, nullptr);
}

auto gse::os::spawn_in_job(const spawn_request& request) -> std::optional<spawned_process> {
	std::vector<win32::HANDLE> inherited;
	for (const win32::HANDLE handle : { request.std_input, request.std_output }) {
		if (win32::valid_handle(handle) && std::ranges::find(inherited, handle) == inherited.end()) {
			inherited.push_back(handle);
		}
	}

	std::vector<std::byte> attribute_memory;
	win32::LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = nullptr;
	if (!inherited.empty()) {
		win32::SIZE_T attribute_size = 0;
		win32::InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
		if (attribute_size == 0) {
			return std::nullopt;
		}
		attribute_memory.resize(attribute_size);
		attribute_list = reinterpret_cast<win32::LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_memory.data());
		if (!win32::InitializeProcThreadAttributeList(attribute_list, 1, 0, &attribute_size)) {
			return std::nullopt;
		}
	}
	const auto _ = make_scope_exit([attribute_list] {
		if (attribute_list != nullptr) {
			win32::DeleteProcThreadAttributeList(attribute_list);
		}
	});
	if (attribute_list != nullptr && !win32::UpdateProcThreadAttribute(attribute_list, 0, win32::proc_thread_attribute_handle_list, inherited.data(), inherited.size() * sizeof(win32::HANDLE), nullptr, nullptr)) {
		return std::nullopt;
	}

	unique_handle job(win32::CreateJobObjectW(nullptr, nullptr));
	if (!job.valid()) {
		return std::nullopt;
	}
	win32::JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{
		.BasicLimitInformation = {
			.LimitFlags = win32::job_object_limit_kill_on_job_close,
		},
	};
	win32::SetInformationJobObject(job.handle(), win32::job_object_extended_limit_information, &limits, sizeof(limits));

	std::vector<wchar_t> command(request.command_line.begin(), request.command_line.end());
	command.push_back(0);
	const std::wstring working_dir = request.working_dir.wstring();

	const bool redirected = request.std_input != nullptr || request.std_output != nullptr;
	win32::STARTUPINFOEXW startup{
		.StartupInfo = {
			.cb = sizeof(win32::STARTUPINFOEXW),
			.dwFlags = redirected ? win32::startf_use_std_handles : 0u,
			.hStdInput = request.std_input,
			.hStdOutput = request.std_output,
			.hStdError = request.std_output,
		},
		.lpAttributeList = attribute_list,
	};

	win32::DWORD flags = win32::create_no_window | win32::create_suspended;
	if (attribute_list != nullptr) {
		flags |= win32::extended_startupinfo_present;
	}
	if (!request.environment.empty()) {
		flags |= win32::create_unicode_environment;
	}

	win32::PROCESS_INFORMATION info{};
	const win32::BOOL created = win32::CreateProcessW(
		nullptr,
		command.data(),
		nullptr,
		nullptr,
		inherited.empty() ? 0 : 1,
		flags,
		request.environment.empty() ? nullptr : const_cast<wchar_t*>(request.environment.data()),
		working_dir.empty() ? nullptr : working_dir.c_str(),
		&startup.StartupInfo,
		&info
	);
	if (!created) {
		return std::nullopt;
	}
	unique_handle process(info.hProcess);
	const unique_handle main_thread(info.hThread);

	if (!win32::AssignProcessToJobObject(job.handle(), process.handle())) {
		win32::TerminateProcess(process.handle(), 1);
		return std::nullopt;
	}
	if (win32::ResumeThread(main_thread.handle()) == std::numeric_limits<win32::DWORD>::max()) {
		win32::TerminateJobObject(job.handle(), 1);
		return std::nullopt;
	}

	return spawned_process{
		.process = std::move(process),
		.job = std::move(job),
		.pid = static_cast<std::uint32_t>(info.dwProcessId),
	};
}

auto gse::os::command_line_of(const std::filesystem::path& executable, const std::span<const std::string> arguments) -> std::wstring {
	const auto append_quoted = [](std::wstring& out, const std::wstring_view word) {
		if (!word.empty() && word.find_first_of(L" \t\"") == std::wstring_view::npos) {
			out += word;
			return;
		}
		out += L'"';
		std::size_t backslashes = 0;
		for (const wchar_t c : word) {
			if (c == L'\\') {
				++backslashes;
				continue;
			}
			out.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
			backslashes = 0;
			out += c;
		}
		out.append(backslashes * 2, L'\\');
		out += L'"';
	};

	std::wstring line;
	append_quoted(line, executable.wstring());
	for (const std::string& argument : arguments) {
		line += L' ';
		append_quoted(line, std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(argument.data()), argument.size())).wstring());
	}
	return line;
}

auto gse::os::exit_code_of(const spawned_process& spawned) -> std::optional<std::uint32_t> {
	if (win32::WaitForSingleObject(spawned.process.handle(), 0) != win32::wait_object_0) {
		return std::nullopt;
	}
	win32::DWORD code = 0;
	win32::GetExitCodeProcess(spawned.process.handle(), &code);
	return static_cast<std::uint32_t>(code);
}

auto gse::os::terminate(const spawned_process& spawned) -> void {
	win32::TerminateJobObject(spawned.job.handle(), 1);
}

auto gse::os::open_inheritable(const std::filesystem::path& path, const win32::DWORD access, const win32::DWORD disposition) -> unique_handle {
	win32::SECURITY_ATTRIBUTES inheritable{
		.nLength = sizeof(win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};
	return unique_handle(win32::CreateFileW(path.wstring().c_str(), access, win32::file_share_read | win32::file_share_write, &inheritable, disposition, win32::file_attribute_normal, nullptr));
}

auto gse::os::create_output(const std::filesystem::path& path) -> unique_handle {
	return open_inheritable(path, win32::generic_write, win32::create_always);
}

auto gse::os::try_lock_file(const std::filesystem::path& path) -> std::optional<unique_handle> {
	unique_handle lock(win32::CreateFileW(path.wstring().c_str(), win32::generic_write, 0, nullptr, win32::open_always, win32::file_attribute_normal, nullptr));
	if (!lock.valid()) {
		return std::nullopt;
	}
	return lock;
}

auto gse::os::available_commit() -> byte_count {
	win32::MEMORYSTATUSEX status{
		.dwLength = sizeof(win32::MEMORYSTATUSEX),
	};
	win32::GlobalMemoryStatusEx(&status);
	return bytes(static_cast<double>(status.ullAvailPageFile));
}

auto gse::os::current_executable() -> std::filesystem::path {
	std::wstring buffer(static_cast<std::size_t>(win32::max_path), L'\0');
	for (;;) {
		const win32::DWORD length = win32::GetModuleFileNameW(nullptr, buffer.data(), static_cast<win32::DWORD>(buffer.size()));
		if (length == 0) {
			return {};
		}
		if (length < buffer.size()) {
			buffer.resize(length);
			return std::filesystem::path(buffer);
		}
		buffer.resize(buffer.size() * 2);
	}
}
#endif
