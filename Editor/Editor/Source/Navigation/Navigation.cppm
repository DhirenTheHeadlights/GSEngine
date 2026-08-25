export module gse.ide.navigation:navigation;

import std;
import gse.ide.diagnostic;

export namespace gse::ide {
	enum class jump_highlight : std::uint8_t {
		caret,
		span,
	};

	struct jump_to_request {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t end_line = 0;
		std::uint32_t end_column = 0;
		jump_highlight highlight = jump_highlight::span;
	};

	struct lint_file_edits {
		std::filesystem::path path;
		std::vector<text_edit> edits;
	};

	struct apply_lint_request {
		std::vector<lint_file_edits> files;
	};
}
