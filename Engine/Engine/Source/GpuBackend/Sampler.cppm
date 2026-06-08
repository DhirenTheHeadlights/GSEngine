export module gse.gpu_backend:sampler;

import std;

import :enums;

export namespace gse::gpu {
	struct sampler {};

	enum class sampler_filter : std::uint8_t {
		nearest,
		linear,
	};

	enum class sampler_address_mode : std::uint8_t {
		repeat,
		clamp_to_edge,
		clamp_to_border,
		mirrored_repeat,
	};

	enum class border_color : std::uint8_t {
		float_opaque_white,
		float_opaque_black,
		float_transparent_black,
	};

	struct sampler_desc {
		sampler_filter min = sampler_filter::linear;
		sampler_filter mag = sampler_filter::linear;
		sampler_address_mode address_u = sampler_address_mode::repeat;
		sampler_address_mode address_v = sampler_address_mode::repeat;
		sampler_address_mode address_w = sampler_address_mode::repeat;
		bool compare_enable = false;
		compare_op compare = compare_op::always;
		border_color border = border_color::float_opaque_white;
		float max_anisotropy = 0.0f;
		float min_lod = 0.0f;
		float max_lod = 0.0f;
	};
}
