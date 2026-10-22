export module gse.os:process;

import std;

import gse.core;
import gse.math;

export namespace gse::os {
	class unique_handle : non_copyable {
	public:
		unique_handle() = default;

		explicit unique_handle(
			void* handle
		);

		unique_handle(
			unique_handle&& other
		) noexcept;

		auto operator=(
			unique_handle&& other
		) noexcept -> unique_handle&;

		~unique_handle();

		auto valid() const -> bool;

		auto handle() const -> void*;

		auto release() -> void*;

	private:
		void* m_handle = nullptr;
	};

	struct spawn_request {
		std::wstring command_line;
		std::filesystem::path working_dir;
		void* std_input = nullptr;
		void* std_output = nullptr;
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

	struct watched_process {
		unique_handle process;
		std::uint64_t created = 0;
	};

	auto watch_process(
		std::uint32_t pid
	) -> std::optional<watched_process>;

	auto exit_code_of(
		const unique_handle& process
	) -> std::optional<std::uint32_t>;

	auto exit_code_of(
		const spawned_process& spawned
	) -> std::optional<std::uint32_t>;

	auto terminate(
		const spawned_process& spawned
	) -> void;

	auto open_inheritable(
		const std::filesystem::path& path,
		std::uint32_t access,
		std::uint32_t disposition
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
