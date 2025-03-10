export module gse.graphics.shader_loader;

import std;

import gse.graphics.shader;

export namespace gse::shader_loader {
	auto load_shaders() -> void;
	auto get_shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> const shader&;
	auto get_shader(std::string_view name) -> const shader&;
}