module;

export module gse.graphics.texture_loader;

import std;
import gse.graphics.texture;
import gse.core.id;
import gse.platform;

export namespace gse::texture_loader {
    auto get_texture(const std::filesystem::path& texture_path) -> texture&;
    auto get_texture_id(const std::filesystem::path& texture_path) -> uuid;
    auto get_texture(const id* texture_id) -> texture&;
    auto get_texture(uuid id) -> texture&;

    auto load_queued_textures(const vulkan::config& config) -> void;
}

std::unordered_map<std::filesystem::path, gse::texture> g_textures_by_path;
std::unordered_map<gse::uuid, gse::texture*> g_textures_by_id;
std::vector<std::filesystem::path> g_queued_paths;

auto gse::texture_loader::get_texture(const std::filesystem::path& texture_path) -> texture& {
    if (g_textures_by_path.contains(texture_path)) {
        return g_textures_by_path.at(texture_path);
    }

    auto& tex = g_textures_by_path.emplace(texture_path, texture(texture_path)).first->second;
    g_queued_paths.push_back(texture_path);

    return tex;
}

auto gse::texture_loader::get_texture_id(const std::filesystem::path& texture_path) -> uuid {
    if (const auto it = g_textures_by_path.find(texture_path); it != g_textures_by_path.end()) {
        return it->second.get_id()->number();
    }
    return -1;
}

auto gse::texture_loader::get_texture(const id* texture_id) -> texture& {
    assert(g_textures_by_id.contains(texture_id->number()), "Trying to access a non-existent texture by ID.");
    return *g_textures_by_id.at(texture_id->number());
}

auto gse::texture_loader::get_texture(uuid id) -> texture& {
    assert(g_textures_by_id.contains(id), "Trying to access a non-existent texture by ID");
    return *g_textures_by_id.at(id);
}

auto gse::texture_loader::load_queued_textures(const vulkan::config& config) -> void {
    for (const auto& path : g_queued_paths) {
        auto& tex = g_textures_by_path.at(path);
        tex.load(config);

        g_textures_by_id[tex.get_id()->number()] = &tex;
    }

    g_queued_paths.clear();
}
