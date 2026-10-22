module gse.runtime:bootstrap_platform;

import std;

import :bootstrap;

auto gse::enable_dpi_awareness() -> void {
}

auto gse::open_editor_pipe(std::string_view) -> std::expected<void*, std::string> {
	return std::unexpected(std::string("no editor pipe in this configuration"));
}

auto gse::command_line_arguments() -> std::vector<std::wstring> {
	return {};
}

auto gse::close_os_handle(void*) -> void {
}
