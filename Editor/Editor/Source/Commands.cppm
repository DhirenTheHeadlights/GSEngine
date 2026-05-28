export module ide:commands;

import std;
import gse;

import :annotations;

export namespace ide::commands {
	[[
		= ide::name<"Save File">{},
		= ide::shortcut<"Ctrl+S">{},
		= ide::description<"Write the active buffer to disk.">{},
		= ide::when<"buffer_focused">{}
	]]
	auto save_file() -> void;

	[[
		= ide::name<"Find In File">{},
		= ide::shortcut<"Ctrl+F">{},
		= ide::description<"Open the in-file search overlay.">{},
		= ide::when<"buffer_focused">{}
	]]
	auto find_in_file() -> void;

	[[
		= ide::name<"Goto Definition">{},
		= ide::shortcut<"F12">{},
		= ide::description<"Jump to the definition of the symbol under the caret.">{},
		= ide::when<"buffer_focused">{}
	]]
	auto goto_definition() -> void;

	[[
		= ide::name<"Toggle Command Palette">{},
		= ide::shortcut<"Ctrl+Shift+P">{},
		= ide::description<"Open the command palette.">{}
	]]
	auto toggle_command_palette() -> void;
}

auto ide::commands::save_file() -> void {}
auto ide::commands::find_in_file() -> void {}
auto ide::commands::goto_definition() -> void {}
auto ide::commands::toggle_command_palette() -> void {}
