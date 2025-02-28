module;

export module gse.graphics.texture_loader;

import std;
import gse.graphics.texture;
import gse.core.id;
import gse.platform.assert;

export namespace gse::texture_loader {
    auto get_texture(const std::filesystem::path& texture_path) -> const texture&;
	auto get_texture_id(const std::filesystem::path& texture_path) -> uuid;
    auto get_texture(const id* texture_id) -> texture&;
}

std::unordered_map<std::string, gse::texture> g_textures_by_path;
std::unordered_map<gse::uuid, gse::texture*> g_textures_by_id;

auto gse::texture_loader::get_texture(const std::filesystem::path& texture_path) -> const texture& {
    if (g_textures_by_path.contains(texture_path.string())) {
        return g_textures_by_path.at(texture_path.string());
    }

    auto& texture = g_textures_by_path.emplace(texture_path.string(), texture_path).first->second;
    g_textures_by_id[texture.get_id()->number()] = &texture;
    return texture;
}

auto gse::texture_loader::get_texture_id(const std::filesystem::path& texture_path) -> uuid {
	if (const auto it = g_textures_by_path.find(texture_path.string()); it != g_textures_by_path.end()) {
		return it->second.get_id()->number();
	}
	return -1;
}

auto gse::texture_loader::get_texture(const id* texture_id) -> texture& {
    perma_assert(g_textures_by_id.contains(texture_id->number()), "Trying to access a non-existent texture by ID.");
    return *g_textures_by_id.at(texture_id->number());
}
