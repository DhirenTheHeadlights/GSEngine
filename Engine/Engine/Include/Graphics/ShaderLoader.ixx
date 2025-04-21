export module gse.graphics.shader_loader;

import std;
import vulkan_hpp;

import gse.graphics.shader;

enum class descriptor_layout : std::uint8_t;

export namespace gse::shader_loader {
	auto load_shaders() -> void;
	auto get_shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> const shader&;
	auto get_shader(std::string_view name) -> const shader&;
	auto get_descriptor_layout(descriptor_layout layout_type) -> const vk::DescriptorSetLayout*;
}

namespace gse::shader_loader {
	auto compile_shaders() -> std::unordered_map<std::string, descriptor_layout>;
}