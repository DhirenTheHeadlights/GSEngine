export module gse.graphics:lighting_source;

import std;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.meta;

export namespace gse::lighting {
	enum class technique : std::uint8_t {
		ray_traced,
		raster,
	};

	struct [[= system_state<"Lighting">{}, = settings::category<"Graphics">{}]]
	data {
		[[
			= settings::describe<"How shadows, ambient occlusion, reflections and probe GI are computed. Ray traced "
								 "queries the scene acceleration structure. Raster uses cascaded and local-light "
								 "shadow maps, screen-space ambient occlusion and reflections, and rasterized probe "
								 "captures, and builds no acceleration structure, so it runs on GPUs without ray "
								 "queries. The unused technique's systems are never registered, so a change takes "
								 "effect on the next launch.">{},
			= settings::restart_required{},
			= shared
		]]
		technique lighting_technique = technique::ray_traced;
	};
}

export namespace gse::shaders::raster_shadow {
	struct [[= shader_struct]]
	shadow_tile {
		view_projection_matrix view_projection;
		vec3<position> origin;
		length texel_size;
		vec2u atlas_offset;
		std::uint32_t atlas_extent;
		std::uint32_t perspective;
		float texel_slope;
		length far_distance;
		length depth_range;
		length near_plane;
	};

	using shader_types = type_pack<shadow_tile>;
}

export namespace gse::renderer::lighting_source {
	struct ray_traced_scene {
		per_frame_resource<gpu::device_address> tlas_addresses{};
		per_frame_resource<gpu::bindless_slot> tlas_slots{};
		id pass;
	};

	struct raster_shadows {
		gpu::bindless_slot cascade_atlas;
		gpu::bindless_slot local_atlas;
		gpu::bindless_slot sampler;
		per_frame_resource<gpu::bindless_slot> tiles{};
		per_frame_resource<gpu::bindless_slot> light_tiles{};
		id pass;
	};

	struct screen_ambient_occlusion {
		gpu::bindless_slot occlusion;
		id pass;
	};

	struct screen_reflections {
		gpu::bindless_slot radiance;
		id pass;
	};
}
