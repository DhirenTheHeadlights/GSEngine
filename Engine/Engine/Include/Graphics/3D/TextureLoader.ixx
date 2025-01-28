export module gse.graphics.texture_loader;

import std;

import gse.core.id;

export namespace gse::texture_loader {
	auto load_texture(const std::string& path, bool gamma_correction) -> std::uint32_t;
	auto get_texture_by_path(std::string_view texture_path) -> std::uint32_t;
	auto get_texture_by_id(id* model_id) -> std::uint32_t;
	auto use_texture
}