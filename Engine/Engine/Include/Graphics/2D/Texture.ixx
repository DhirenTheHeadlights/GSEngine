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
		texture(const std::filesystem::path& filepath) : identifiable(filepath.string()), m_image_data(stb::image::load(filepath)) {}

        auto load(const vulkan::config& config) -> void;
		auto load_from_memory(const vulkan::config& config, const std::vector<std::byte>& data, unitless::vec2u size, std::uint32_t channels) -> void;

        auto get_descriptor_info() const -> vk::DescriptorImageInfo;
        auto get_image_data() const -> const stb::image::data& { return m_image_data; }
		auto get_image_resource() const -> const vulkan::persistent_allocator::image_resource& { return m_texture_image; }
    private:
		vulkan::persistent_allocator::image_resource m_texture_image;
        vk::Sampler m_texture_sampler;
        stb::image::data m_image_data;
    };
}