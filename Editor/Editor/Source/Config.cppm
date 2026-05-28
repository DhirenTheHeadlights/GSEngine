export module ide:config_system;

import std;
import gse;

export namespace ide {
	struct config_system {
		struct [[
			= gse::settings::category<"Editor">{}
		]] data {
			[[= gse::settings::describe<"Font size in pixels for code views.">{},
				= gse::settings::range<8, 32>{}]]
			int font_size = 14;

			[[= gse::settings::describe<"Spaces per indentation level.">{},
				= gse::settings::range<1, 8>{}]]
			int indent_width = 4;

			[[= gse::settings::describe<"Use spaces instead of tabs when indenting.">{}]]
			bool indent_with_spaces = false;

			[[= gse::settings::describe<"Show line numbers in the gutter.">{}]]
			bool show_line_numbers = true;

			[[= gse::settings::describe<"Wrap lines that exceed the view width.">{}]]
			bool soft_wrap = false;

			[[= gse::settings::describe<"Show a vertical ruler at this column (0 disables).">{},
				= gse::settings::range<0, 200>{}]]
			int ruler_column = 100;

			[[= gse::settings::describe<"Highlight the current line.">{}]]
			bool highlight_current_line = true;

			[[= gse::settings::describe<"Caret blink interval in milliseconds (0 disables blinking).">{},
				= gse::settings::range<0, 2000>{}]]
			int caret_blink_ms = 500;

			[[= gse::settings::describe<"Path to the clangd executable used for C++ intelligence.">{}]]
			std::string clangd_path = "clangd";
		};
	};
}
