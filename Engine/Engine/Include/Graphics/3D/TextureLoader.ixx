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

std::unordered_map<std::filesystem::path, std::unique_ptr<gse::texture>> g_textures_by_path;
std::unordered_map<gse::id, gse::texture*> g_textures_by_id;
std::vector<std::filesystem::path> g_queued_paths;

auto gse::texture_loader::get_texture(const std::filesystem::path& texture_path) -> texture& {
    if (g_textures_by_path.contains(texture_path)) {
        return *g_textures_by_path.at(texture_path);
    }

    auto tex_ptr = std::make_unique<texture>(texture_path);
    auto& tex = *g_textures_by_path.emplace(texture_path, std::move(tex_ptr)).first->second;
    g_queued_paths.push_back(texture_path);

    return tex;
}

auto gse::texture_loader::get_texture_id(const std::filesystem::path& texture_path) -> id {
    if (const auto it = g_textures_by_path.find(texture_path); it != g_textures_by_path.end()) {
        return it->second->get_id();
    }

    auto tex_ptr = std::make_unique<texture>(texture_path);
    const auto& tex = g_textures_by_path.emplace(texture_path, std::move(tex_ptr)).first->second;
    g_queued_paths.push_back(texture_path);

    return tex->get_id();
}

auto gse::texture_loader::get_texture(const id& texture_id) -> texture& {
    assert(g_textures_by_id.contains(texture_id), "Trying to access a non-existent texture by ID.");
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
