export module gse.runtime:bootstrap;

import gse.config;
import std;

import :engine;

export namespace gse {
	using app_setup_fn = std::function<void(engine&)>;

	auto start(
		app_setup_fn setup,
		const engine_config& config = {}
	) -> int;

	auto shutdown() -> void;
}

namespace gse {
	auto enable_dpi_awareness() -> void;

	[[nodiscard]] auto command_line_arguments() -> std::vector<std::wstring>;

	auto close_os_handle(
		void* handle
	) -> void;

	[[nodiscard]] auto open_editor_pipe(
		std::string_view name
	) -> std::expected<void*, std::string>;
}
