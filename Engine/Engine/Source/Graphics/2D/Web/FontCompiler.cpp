module gse.graphics:font_compiler_impl;

import std;

import gse.log;

import :font;
import :font_compiler;

auto gse::bake(const std::filesystem::path& src, font::baked&) -> bool {
	log::println(
		log::level::error,
		log::category::assets,
		"font bake [{}]: baking is unavailable on this platform",
		src.filename().generic_display_string()
	);
	return false;
}
