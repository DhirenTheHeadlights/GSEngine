module;

export module gse.graphics.texture_loader;

import std;
import gse.graphics.texture;
import gse.core.id;
import gse.platform;

export namespace gse::texture_loader {
    auto get_texture(const std::filesystem::path& texture_path) -> texture&;
    auto get_texture_id(const std::filesystem::path& texture_path) -> id;
    auto get_texture(const id& texture_id) -> texture&;
    auto get_all_textures() -> const std::unordered_map<std::filesystem::path, std::unique_ptr<texture>>&;

    auto load_queued_textures(const vulkan::config& config) -> void;
}

namespace gse::texture_loader {
    std::unordered_map<std::filesystem::path, std::unique_ptr<texture>> g_textures_by_path;
    std::unordered_map<id, texture*> g_textures_by_id;
    std::unordered_set<std::filesystem::path> g_queued_paths;

    auto find_or_create_texture_entry(const std::filesystem::path& texture_path) -> texture&;
}

auto gse::texture_loader::get_texture(const std::filesystem::path& texture_path) -> texture& {
    return find_or_create_texture_entry(texture_path);
}

auto gse::texture_loader::get_texture_id(const std::filesystem::path& texture_path) -> id {
    return find_or_create_texture_entry(texture_path).get_id();
}

auto gse::texture_loader::get_texture(const id& texture_id) -> texture& {
    assert(
        g_textures_by_id.contains(texture_id),
		std::format("Texture with ID {} not found.", texture_id.number())
    );
    return *g_textures_by_id.at(texture_id);
}

auto gse::texture_loader::load_queued_textures(const vulkan::config& config) -> void {
    for (const auto& path : g_queued_paths) {
        const auto tex = g_textures_by_path.at(path).get();
        tex->load(config);
        g_textures_by_id[tex->get_id()] = tex;
    }
    g_queued_paths.clear();
}

auto gse::texture_loader::get_all_textures() -> const std::unordered_map<std::filesystem::path, std::unique_ptr<texture>>& {
    return g_textures_by_path;
}

auto gse::texture_loader::find_or_create_texture_entry(const std::filesystem::path& texture_path) -> texture& {
    if (const auto it = g_textures_by_path.find(texture_path); it != g_textures_by_path.end()) {
        return *it->second;
    }

    auto tex_ptr = std::make_unique<texture>(texture_path);
    g_queued_paths.insert(texture_path);
    auto& tex = *g_textures_by_path.emplace(texture_path, std::move(tex_ptr)).first->second;

    return tex;
}
