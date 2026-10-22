export module gse.graphics:font_compiler;

import std;

import :font;

export namespace gse {

	auto bake(
		const std::filesystem::path& src,
		font::baked& out
	) -> bool;
}
