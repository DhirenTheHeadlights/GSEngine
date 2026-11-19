export module gse.ide.navigation:navigation;

import std;
import gse.concurrency;
import gse.ide.diagnostic;

export namespace gse::ide {
	constexpr std::string_view source_control_panel_name = "Source Control";

	enum class jump_highlight : std::uint8_t {
		caret,
		span,
	};

	struct [[= reveals<"Code">{}]] jump_to_request {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t end_line = 0;
		std::uint32_t end_column = 0;
		jump_highlight highlight = jump_highlight::span;
	};

	struct link_hit {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
	};

	auto path_link_at(
		std::string_view row,
		std::uint32_t column,
		const std::filesystem::path& base
	) -> std::optional<link_hit>;

	auto resolve_link(
		std::string_view token,
		const std::filesystem::path& base
	) -> std::optional<link_hit>;

	struct lint_file_edits {
		std::filesystem::path path;
		std::vector<text_edit> edits;
	};

	struct [[= reveals<"Lints">{}]] apply_lint_request {
		std::vector<lint_file_edits> files;
	};
}
