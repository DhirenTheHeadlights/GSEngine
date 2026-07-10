export module gse.ide.command:commands;

import std;
import gse;

import :annotations;

export namespace gse::ide::commands {
	[[
		= gse::ide::name<"Save File">{},
		= gse::ide::shortcut<"Ctrl+S">{},
		= gse::ide::description<"Write the active buffer to disk.">{},
		= gse::ide::when<"buffer_focused">{}
	]]
	auto save_file() -> void;

	[[
		= gse::ide::name<"Find In File">{},
		= gse::ide::shortcut<"Ctrl+F">{},
		= gse::ide::description<"Open the in-file search overlay.">{},
		= gse::ide::when<"buffer_focused">{}
	]]
	auto find_in_file() -> void;

	[[
		= gse::ide::name<"Goto Definition">{},
		= gse::ide::shortcut<"F12">{},
		= gse::ide::description<"Jump to the definition of the symbol under the caret.">{},
		= gse::ide::when<"buffer_focused">{}
	]]
	auto goto_definition() -> void;

	[[
		= gse::ide::name<"Toggle Command Palette">{},
		= gse::ide::shortcut<"Ctrl+Shift+P">{},
		= gse::ide::description<"Open the command palette.">{}
	]]
	auto toggle_command_palette() -> void;
}

auto gse::ide::commands::save_file() -> void {}
auto gse::ide::commands::find_in_file() -> void {}
auto gse::ide::commands::goto_definition() -> void {}
auto gse::ide::commands::toggle_command_palette() -> void {}
