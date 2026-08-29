export module gse.gpu_backend:image;

import std;

import :core;
import :enums;

import gse.core;
import gse.math;

export namespace gse::gpu {
	enum class color_space : std::uint8_t {
		srgb_nonlinear,
	};

	enum class image_format : std::uint8_t {
		d32_sfloat,
		r8g8b8a8_srgb,
		r8g8b8a8_unorm,
		b8g8r8a8_srgb,
		b8g8r8a8_unorm,
		r8g8b8_srgb,
		r8g8b8_unorm,
		r8_unorm,
		b10g11r11_ufloat,
		r8g8_snorm,
		r8g8_unorm,
		r16g16b16a16_sfloat,
		r16g16_sfloat,
		undefined,
	};

	enum class image_view_type : std::uint8_t {
		e2d,
		e3d,
		cube,
	};

	enum class image_flag : std::uint8_t {
		sampled = 1 << 0,
		depth_attachment = 1 << 1,
		color_attachment = 1 << 2,
		transfer_dst = 1 << 3,
		storage = 1 << 4,
		transfer_src = 1 << 5,
		host_transfer = 1 << 6,
	};

	using image_usage = flags<image_flag>;

	enum class image_aspect_flag : std::uint32_t {
		color = 1u << 0,
		depth = 1u << 1,
		stencil = 1u << 2,
		plane_0 = 1u << 3,
		plane_1 = 1u << 4,
	};

	using image_aspect_flags = flags<image_aspect_flag>;

	enum class image_create_flag : std::uint8_t {
		cube_compatible = 1 << 0,
		exportable = 1 << 1,
	};

	using image_create_flags = flags<image_create_flag>;

	enum class image_type : std::uint8_t {
		e1d,
		e2d,
		e3d,
	};

	struct surface_format {
		image_format format = image_format::r8g8b8a8_srgb;
		color_space color_space = color_space::srgb_nonlinear;
	};

	struct surface_capabilities {
		std::uint32_t min_image_count = 0;
		std::uint32_t max_image_count = 0;
		vec2u current_extent;
		vec2u min_image_extent;
		vec2u max_image_extent;
		std::uint32_t max_image_array_layers = 0;
	};

	struct image_desc {
		vec2u size = { 1, 1 };
		std::uint32_t depth = 1;
		image_format format = image_format::d32_sfloat;
		image_view_type view = image_view_type::e2d;
		image_usage usage{ image_flag::sampled, image_flag::depth_attachment };
		bool bindless = false;
	};

	struct image_create_info {
		image_create_flags flags;
		image_type type = image_type::e2d;
		image_format format = image_format::r8g8b8a8_unorm;
		vec3u extent;
		std::uint32_t mip_levels = 1;
		std::uint32_t array_layers = 1;
		sample_count samples = sample_count::e1;
		image_usage usage;
	};

	struct image_view_create_info {
		image_format format = image_format::r8g8b8a8_unorm;
		image_view_type view_type = image_view_type::e2d;
		image_aspect_flags aspects;
		std::uint32_t base_mip_level = 0;
		std::uint32_t level_count = 1;
		std::uint32_t base_array_layer = 0;
		std::uint32_t layer_count = 1;
	};

	struct image_subresource_layers {
		image_aspect_flags aspects;
		std::uint32_t mip_level = 0;
		std::uint32_t base_array_layer = 0;
		std::uint32_t layer_count = 1;
	};

	struct buffer_image_copy_region {
		device_size buffer_offset = 0;
		std::uint32_t buffer_row_length = 0;
		std::uint32_t buffer_image_height = 0;
		image_subresource_layers image_subresource;
		vec3i image_offset;
		vec3u image_extent;
	};

	struct image_copy_region {
		image_subresource_layers src_subresource;
		vec3i src_offset;
		image_subresource_layers dst_subresource;
		vec3i dst_offset;
		vec3u extent;
	};

	struct image_blit_region {
		image_subresource_layers src_subresource;
		vec3i src_offsets[2];
		image_subresource_layers dst_subresource;
		vec3i dst_offsets[2];
	};

	class image final : public non_copyable {
	public:
		image() {}
		~image() = default;

		image(
			handle<image> image,
			handle<image_view> view,
			image_format format,
			vec3u extent,
			image_view_create_info view_info,
			bindless_slot storage_slot = {},
			bindless_slot sampled_slot = {}
		);

		image(
			image&&
		) noexcept = default;

		auto operator=(
			image&&
		) noexcept -> image& = default;

		[[nodiscard]] auto handle() const -> gpu::handle<image>;

		[[nodiscard]] auto view() const -> gpu::handle<image_view>;

		[[nodiscard]] auto format() const -> image_format;

		[[nodiscard]] auto extent() const -> vec3u;

		[[nodiscard]] auto view_create_info() const -> const image_view_create_info&;

		[[nodiscard]] auto storage_slot() const -> bindless_slot;

		[[nodiscard]] auto sampled_slot() const -> bindless_slot;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::handle<image> m_image;
		gpu::handle<image_view> m_view;
		image_format m_format = image_format::undefined;
		vec3u m_extent;
		image_view_create_info m_view_info;
		bindless_slot m_storage_slot;
		bindless_slot m_sampled_slot;
	};

	struct swap_chain_info {
		swap_chain_handle handle;
		vec2u extent;
		image_format format = image_format::d32_sfloat;
		gpu::present_mode present_mode = gpu::present_mode::fifo;
		std::vector<gpu::handle<image>> images;
		std::vector<gpu::handle<image_view>> image_views;
		bool present_timing_supported = false;
		time_t<std::uint64_t> refresh_interval{};
		time_t<std::uint64_t> refresh_duration{};
		std::uint64_t time_domain_id = 0;
	};

	struct shared_surface_desc {
		vec2u extent;
		image_format format = image_format::r8g8b8a8_unorm;
		image_usage usage{ image_flag::color_attachment, image_flag::sampled };
	};

	struct shared_surface {
		gpu::handle<gpu::image> image;
		gpu::handle<image_view> view;
		device_memory memory;
		vec2u extent;
		image_format format = image_format::r8g8b8a8_unorm;
		device_size size = 0;
		void* handle = nullptr;
	};

	[[nodiscard]]
	auto image_aspect_for(
		image_format f
	) -> image_aspect_flags;
}

auto gse::gpu::image_aspect_for(const image_format f) -> image_aspect_flags {
	if (f == image_format::d32_sfloat) {
		return image_aspect_flag::depth;
	}
	return image_aspect_flag::color;
}

gse::gpu::image::image(const gpu::handle<image> image, const gpu::handle<image_view> view, const image_format format, const vec3u extent, image_view_create_info view_info, const bindless_slot storage_slot, const bindless_slot sampled_slot)
	: m_image(image), m_view(view), m_format(format), m_extent(extent), m_view_info(view_info), m_storage_slot(storage_slot), m_sampled_slot(sampled_slot) {
}

auto gse::gpu::image::handle() const -> gpu::handle<image> {
	return m_image;
}

auto gse::gpu::image::view() const -> gpu::handle<image_view> {
	return m_view;
}

auto gse::gpu::image::format() const -> image_format {
	return m_format;
}

auto gse::gpu::image::extent() const -> vec3u {
	return m_extent;
}

auto gse::gpu::image::view_create_info() const -> const image_view_create_info& {
	return m_view_info;
}

auto gse::gpu::image::storage_slot() const -> bindless_slot {
	return m_storage_slot;
}

auto gse::gpu::image::sampled_slot() const -> bindless_slot {
	return m_sampled_slot;
}

auto gse::gpu::image::valid() const -> bool {
	return static_cast<bool>(m_image);
}