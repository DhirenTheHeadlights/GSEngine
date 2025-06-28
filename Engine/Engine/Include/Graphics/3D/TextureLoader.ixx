export module gse.graphics:texture_loader;

import std;

import :texture;

import gse.assert;
import gse.utility;
import gse.platform;

export namespace gse::texture_loader {
    auto texture(const std::filesystem::path& texture_path) -> texture&;
    auto get_texture_id(const std::filesystem::path& texture_path) -> id;
    auto texture(const id& texture_id) -> gse::texture&;
    auto blank() -> gse::texture&;

    auto load_queued_textures(const vulkan::config& config) -> void;
}

namespace gse::texture_loader {
    auto g_blank_texture = gse::texture({ 1, 1, 1, 1 });
    bool g_blank_texture_loaded = false;

    std::unordered_map<std::filesystem::path, std::unique_ptr<gse::texture>> g_textures_by_path;
    std::unordered_map<id, gse::texture*> g_textures_by_id;
    std::unordered_set<std::filesystem::path> g_queued_paths;

    auto find_or_create_texture_entry(const std::filesystem::path& texture_path) -> gse::texture&;
}

auto gse::texture_loader::texture(const std::filesystem::path& texture_path) -> gse::texture& {
    return find_or_create_texture_entry(texture_path);
}

auto gse::texture_loader::get_texture_id(const std::filesystem::path& texture_path) -> id {
    return find_or_create_texture_entry(texture_path).id();
}

auto gse::texture_loader::texture(const id& texture_id) -> gse::texture& {
    assert(
        g_textures_by_id.contains(texture_id),
		std::format("Texture with ID {} not found.", texture_id.number())
    );
    return *g_textures_by_id.at(texture_id);
}

auto gse::texture_loader::blank() -> gse::texture& {
    return g_blank_texture;
}

auto gse::texture_loader::load_queued_textures(const vulkan::config& config) -> void {
    if (!g_blank_texture_loaded) {
        g_blank_texture.load(config);
        g_blank_texture_loaded = true;
	}

    for (const auto& path : g_queued_paths) {
        const auto tex = g_textures_by_path.at(path).get();
        tex->load(config);
        g_textures_by_id[tex->id()] = tex;
    }
    g_queued_paths.clear();
}

auto gse::texture_loader::find_or_create_texture_entry(const std::filesystem::path& texture_path) -> gse::texture& {
    if (const auto it = g_textures_by_path.find(texture_path); it != g_textures_by_path.end()) {
        return *it->second;
    }

    auto tex_ptr = std::make_unique<gse::texture>(texture_path);
    g_queued_paths.insert(texture_path);
    auto& tex = *g_textures_by_path.emplace(texture_path, std::move(tex_ptr)).first->second;

    return tex;
}
