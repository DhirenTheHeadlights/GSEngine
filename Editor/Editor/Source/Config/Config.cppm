export module gse.ide.config:config_system;

import std;
import gse;

export namespace gse::ide::config_system {
	struct [[
		= system_state<"EditorConfig">{},
		= settings::category<"Editor">{}
	]] data {
		[[= settings::describe<"Font size in pixels for code views.">{},
			= settings::range<8, 32>{}]]
		int font_size = 14;

		[[= settings::describe<"Spaces per indentation level.">{},
			= settings::range<1, 8>{},
			= settings::project_scope{},
			= shared]]
		int indent_width = 4;

		[[= settings::describe<"Use spaces instead of tabs when indenting.">{},
			= settings::project_scope{},
			= shared]]
		bool indent_with_spaces = false;

		[[= settings::describe<"Show line numbers in the gutter.">{},
			= shared]]
		bool show_line_numbers = true;

		[[= settings::describe<"Reformat the file before every save.">{},
			= shared]]
		bool format_on_save = true;

		[[= settings::describe<"Wrap lines that exceed the view width.">{}]]
		bool soft_wrap = false;

		[[= settings::describe<"Show a vertical ruler at this column (0 disables).">{},
			= settings::range<0, 200>{},
			= settings::project_scope{}]]
		int ruler_column = 100;

		[[= settings::describe<"Highlight the current line.">{}]]
		bool highlight_current_line = true;

		[[= settings::describe<"Caret blink interval (0 disables blinking).">{},
			= shared]]
		time caret_blink = milliseconds(500);
	};
}