export module gse.graphics.texture;

import std;
import vulkan_hpp;

import gse.core.id;
import gse.physics.math;
import gse.platform;

export namespace gse {
    class texture : public identifiable {
    public:
        texture() : identifiable("Unnamed Texture") {}
        texture(const std::filesystem::path& filepath);
        ~texture();

        auto load_from_file(const std::filesystem::path& filepath) -> void;
		auto load_from_memory(const std::span<std::uint8_t>& data, int width, int height, int channels) -> void;

        auto get_descriptor_info() const -> vk::DescriptorImageInfo;
        auto get_dimensions() const -> unitless::vec2i { return m_size; }
    private:
		vulkan::persistent_allocator::image_resource m_texture_image;
        vk::Sampler m_texture_sampler;

        unitless::vec2i m_size = { 0, 0 };
		std::filesystem::path m_filepath;
        int m_channels = 0;
    };
}