export module gse.graphics.texture;

import std;
import vulkan_hpp;

import gse.core.id;
import gse.physics.math;
import gse.platform.assert;
import gse.platform.context;

export namespace gse {
    class texture : public identifiable {
    public:
        texture() : identifiable("Unnamed Texture") {}
        texture(const std::filesystem::path& filepath, const vk::DescriptorSetLayout& layout);
        ~texture();

        auto load_from_file(const std::filesystem::path& filepath, const vk::DescriptorSetLayout& layout) -> void;
		auto load_from_memory(const std::span<std::uint8_t>& data, int width, int height, int channels) -> void;
        auto get_descriptor_info() const -> vk::DescriptorImageInfo;
        auto get_descriptor_set() const -> vk::DescriptorSet;
        auto get_dimensions() const -> unitless::vec2i { return m_size; }
    private:
        auto create_texture_image(const unsigned char* data, int width, int height, int channels) -> void;
        auto create_texture_image_view() -> void;
        auto create_texture_sampler() -> void;
		auto create_descriptor_set(const vk::DescriptorSetLayout& descriptor_set_layout) -> void;

        vk::Image m_texture_image;
        vk::DeviceMemory m_texture_image_memory;
        vk::ImageView m_texture_image_view;
        vk::Sampler m_texture_sampler;
        vk::DescriptorSet m_descriptor_set;

        unitless::vec2i m_size = { 0, 0 };
		std::filesystem::path m_filepath;
        int m_channels = 0;
    };
}