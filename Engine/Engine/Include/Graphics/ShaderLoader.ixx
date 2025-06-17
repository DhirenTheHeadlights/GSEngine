export module gse.graphics.shader_loader;

import std;
import vulkan_hpp;

import gse.graphics.shader;
import gse.platform;

export enum class descriptor_layout : std::uint8_t {
	standard_3d		= 0,
	deferred_3d		= 1,
	forward_3d		= 2,
	forward_2d		= 3,
	post_process	= 4,
	custom			= 99
};

export namespace gse::shader_loader {
	auto load_shaders(const ::vk::raii::Device& device) -> void;
	auto get_shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> const shader&;
	auto get_shader(std::string_view name) -> const shader&;
}

namespace gse::shader_loader {
	auto compile_shaders() -> std::unordered_map<std::string, descriptor_layout>;
}