export module gse.graphics:cube_map;

import std;

import gse.math;
import gse.platform;

export namespace gse {
    class cube_map {
    public:
        auto create(
            gpu::context& ctx,
            const std::array<std::filesystem::path, 6>& face_paths
        ) -> void;

        auto create(
            gpu::context& ctx,
            int resolution,
            bool depth_only = false
        ) -> void;

        auto gpu_image(
        ) const -> const gpu::image&;

        auto gpu_sampler(
        ) const -> const gpu::sampler&;

        auto face_view(
            std::size_t face
        ) const -> gpu::image_view;

        auto resolution(
        ) const -> int;
    private:
        gpu::image m_image;
        gpu::sampler m_sampler;
        int m_resolution = 0;
    };
}

auto gse::cube_map::create(gpu::context& ctx, const std::array<std::filesystem::path, 6>& face_paths) -> void {
    const auto faces = image::load_cube_faces(face_paths);

    m_image = gpu::create_image(ctx.device_ref(), {
        .size = faces[0].size,
        .format = gpu::image_format::r8g8b8a8_srgb,
        .view = gpu::image_view_type::cube,
        .usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst
    });

    gpu::upload_image_layers(
        ctx.device_ref(),
        m_image,
        reinterpret_cast<const std::vector<const void*>&>(faces),
        faces[0].size_bytes()
    );

    m_sampler = gpu::create_sampler(ctx.device_ref(), {
        .address_u = gpu::sampler_address_mode::clamp_to_edge,
        .address_v = gpu::sampler_address_mode::clamp_to_edge,
        .address_w = gpu::sampler_address_mode::clamp_to_edge
    });
}

auto gse::cube_map::create(gpu::context& ctx, const int resolution, const bool depth_only) -> void {
    m_resolution = resolution;

    const gpu::image_format gpu_format = depth_only ? gpu::image_format::d32_sfloat : gpu::image_format::r16g16b16a16_sfloat;
    const gpu::image_usage usage = depth_only
        ? (gpu::image_flag::depth_attachment | gpu::image_flag::sampled)
        : (gpu::image_flag::color_attachment | gpu::image_flag::sampled);

    const vec2u size{ static_cast<std::uint32_t>(resolution), static_cast<std::uint32_t>(resolution) };

    m_image = gpu::create_image(ctx.device_ref(), {
        .size = size,
        .format = gpu_format,
        .view = gpu::image_view_type::cube,
        .usage = usage,
        .ready_layout = depth_only ? gpu::image_layout::general : gpu::image_layout::undefined
    });

    m_sampler = gpu::create_sampler(ctx.device_ref(), {
        .address_u = gpu::sampler_address_mode::clamp_to_edge,
        .address_v = gpu::sampler_address_mode::clamp_to_edge,
        .address_w = gpu::sampler_address_mode::clamp_to_edge
    });
}

auto gse::cube_map::gpu_image() const -> const gpu::image& {
    return m_image;
}

auto gse::cube_map::gpu_sampler() const -> const gpu::sampler& {
    return m_sampler;
}

auto gse::cube_map::face_view(const std::size_t face) const -> gpu::image_view {
    return m_image.layer_view(static_cast<std::uint32_t>(face));
}

auto gse::cube_map::resolution() const -> int {
    return m_resolution;
}
