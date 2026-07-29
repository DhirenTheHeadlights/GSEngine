export module gse.ide.config:config_system;

import std;
import gse;

export namespace gse::ide::config_system {
	struct [[
		= gse::system_state<"EditorConfig">{},
		= gse::settings::category<"Editor">{}
	]] data {
		[[= gse::settings::describe<"Font size in pixels for code views.">{},
			= gse::settings::range<8, 32>{}]]
		int font_size = 14;

		[[= gse::settings::describe<"Spaces per indentation level.">{},
			= gse::settings::range<1, 8>{},
			= gse::settings::project_scope{},
			= gse::shared]]
		int indent_width = 4;

		[[= gse::settings::describe<"Use spaces instead of tabs when indenting.">{},
			= gse::settings::project_scope{},
			= gse::shared]]
		bool indent_with_spaces = false;

		[[= gse::settings::describe<"Show line numbers in the gutter.">{},
			= gse::shared]]
		bool show_line_numbers = true;

		[[= gse::settings::describe<"Reformat the file before every save.">{},
			= gse::shared]]
		bool format_on_save = true;

		[[= gse::settings::describe<"Wrap lines that exceed the view width.">{}]]
		bool soft_wrap = false;

		[[= gse::settings::describe<"Show a vertical ruler at this column (0 disables).">{},
			= gse::settings::range<0, 200>{},
			= gse::settings::project_scope{}]]
		int ruler_column = 100;

		[[= gse::settings::describe<"Highlight the current line.">{}]]
		bool highlight_current_line = true;

		[[= gse::settings::describe<"Caret blink interval (0 disables blinking).">{},
			= gse::shared]]
		gse::time caret_blink = gse::milliseconds(500);
	};
}
