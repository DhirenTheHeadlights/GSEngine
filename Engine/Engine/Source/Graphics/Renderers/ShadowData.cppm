export module gse.graphics:shadow_data;

import std;

import :cube_map;

import gse.platform;
import gse.math;
import gse.utility;

export namespace gse::renderer {
	struct shadow_light_entry {
		view_matrix view;
		projection_matrix proj;
		std::vector<id> ignore_list_ids;
		int shadow_index = -1;
	};

	struct point_shadow_light_entry {
		vec3<length> world_position;
		std::array<view_projection_matrix, 6> face_view_proj;
		length near_plane;
		length far_plane;
		std::vector<id> ignore_list_ids;
		int point_shadow_index = -1;
	};

	constexpr std::size_t max_shadow_lights = 10;
	constexpr std::size_t max_point_shadow_lights = 4;

	auto ensure_non_collinear_up(
		const vec3f& direction,
		const vec3f& up
	) -> vec3f;
}

export namespace gse::renderer::shadow {
	struct render_data {
		std::vector<shadow_light_entry> lights;
		std::vector<point_shadow_light_entry> point_lights;
	};

	struct state {
		gpu::pipeline pipeline;

		resource::handle<shader> shader_handle;

		vec2u shadow_extent = { 1024, 1024 };
		vec2u point_shadow_extent = { 512, 512 };

		std::array<gpu::image, max_shadow_lights> shadow_maps;
		std::array<cube_map, max_point_shadow_lights> point_shadow_cubemaps;

		state() = default;

		auto shadow_map_view(
			std::size_t index
		) const -> vk::ImageView;

		auto point_shadow_cube_view(
			std::size_t index
		) const -> vk::ImageView;

		auto point_shadow_face_view(
			std::size_t index,
			std::size_t face
		) const -> vk::ImageView;

		auto point_shadow_sampler(
			std::size_t index
		) const -> vk::Sampler;

		auto shadow_texel_size(
		) const -> vec2f;
	};
}
