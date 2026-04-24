export module gse.graphics:texture;

import std;

import gse.core;
import gse.math;
import gse.os;
import gse.gpu;

export namespace gse {
    class texture : public identifiable {
    public:
        enum struct profile : std::uint8_t {
            generic_repeat,
            generic_clamp_to_edge,
            msdf,
            pixel_art
        };

        texture(
            const std::filesystem::path& filepath
        );

        texture(
            std::string_view name,
            const vec4f& color,
            vec2u size = { 1, 1 }
        );

        texture(
            std::string_view name,
            const std::vector<std::byte>& data,
            vec2u size,
            std::uint32_t channels,
            profile texture_profile = profile::generic_repeat
        );

        auto load(
            const gpu::context& context
        ) -> void;

        auto unload(
        ) -> void;

        auto gpu_image(
        ) const -> const gpu::image&;

        auto gpu_sampler(
        ) const -> const gpu::sampler&;

        auto image_data(
        ) const -> const image::data&;

        [[nodiscard]] auto bindless_slot(
        ) const -> gpu::bindless_texture_slot;

    private:
        auto create_vulkan_resources(
            gpu::context& context,
            profile texture_profile
        ) -> void;

        gpu::image m_image;
        gpu::sampler m_sampler;
        gpu::bindless_texture_slot m_bindless_slot;
        image::data m_image_data;
        profile m_profile = profile::generic_repeat;
    };
}
