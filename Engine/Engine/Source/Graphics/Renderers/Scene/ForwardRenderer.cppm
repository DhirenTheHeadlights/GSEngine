export module gse.graphics:forward_renderer;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.meta;
import gse.os;
import gse.physics;
import gse.save;
import gse.time;
import std;

import :atmosphere_renderer;
import :camera_latch_renderer;
import :camera_system;
import :cloud_renderer;
import :directional_light;
import :geometry_collector;
import :gi_probe_renderer;
import :light_culling_renderer;
import :lighting_source;
import :point_light;
import :shared_shaders;
import :spot_light;
import :texture;

export namespace gse::renderer::forward {
	constexpr std::size_t max_lights = 1024;

	struct light_bindings {
		shaders::ssbo_readonly<shaders::forward::light> lights_ssbo;
	};

	struct ray_traced_bindings {
		shaders::tlas scene_tlas;
	};

	struct raster_bindings {
		shaders::texture2d<vec4f> shadow_cascade_atlas;
		shaders::texture2d<vec4f> shadow_local_atlas;
		shaders::ssbo_readonly<shaders::raster_shadow::shadow_tile> shadow_tiles;
		shaders::ssbo_readonly<vec2u> light_shadow_tiles;
		shaders::texture2d<vec4f> ambient_occlusion_in;
		shaders::texture2d<vec4f> reflection_in;
	};

	struct scene_bindings {
		shaders::ssbo_readwrite<std::uint32_t> light_index_list;
		shaders::ssbo_readwrite<vec2u> tile_light_table;
		shaders::texture3d<vec4f> aerial_perspective_in;
		shaders::texture2d<vec4f> gi_atlas;
		shaders::sampler_state ap_sampler;
		shaders::sampler_state gi_atlas_sampler;
		shaders::texture2d<vec4f> cloud_shadow_in;
		shaders::sampler_state cloud_shadow_sampler;
	};

	template <typename TechniqueBindings, typename... Tail>
	using forward_bindings = gpu::bindings<
		shaders::standard_3d::camera,
		light_bindings,
		TechniqueBindings,
		scene_bindings,
		shaders::standard_3d::materials,
		atmosphere::bindings,
		cloud::shadow_bindings,
		shaders::standard_3d::sky_view,
		Tail...
	>;

	template <fixed_string TechniqueHelper, fixed_string MaterialHelper, fixed_string... Tail>
	using surface_helpers = gpu::helpers<"Standard3D/meshlet_common", "Standard3D/gi_probe_sample", "Standard3D/light_eval", "Atmosphere/atmosphere_common", "Standard3D/lighting_shared", "Standard3D/lighting_common", "Standard3D/cube_faces", TechniqueHelper, MaterialHelper, "Standard3D/forward_shading", Tail...>;

	struct [[= shaders::shader_struct]] meshlet_push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
		std::int32_t num_lights;
		vec2u screen_size;
		std::int32_t shadow_quality;
		std::int32_t ao_quality;
		std::int32_t reflection_quality;
		vec3<position> gi_origin;
		length gi_spacing;
		vec3u gi_grid_dim;
		float gi_intensity;
		vec2f gi_atlas_size;
		std::int32_t gi_enabled;
		vec3f sun_color;
		irradiance sun_ambient;
		irradiance shadow_cutoff;
		std::uint32_t frame_counter;
	};

	struct surface_frame {
		gpu::bindless_slot camera_ubo;
		gpu::bindless_slot lights;
		gpu::bindless_slot light_index_list;
		gpu::bindless_slot tile_light_table;
		gpu::bindless_slot aerial_perspective;
		gpu::bindless_slot gi_atlas;
		gpu::bindless_slot ap_sampler;
		gpu::bindless_slot gi_atlas_sampler;
		gpu::bindless_slot cloud_shadow;
		gpu::bindless_slot cloud_shadow_sampler;
		gpu::bindless_slot material_palette;
		gpu::bindless_slot atmosphere_ubo;
		gpu::bindless_slot cloud_shadow_ubo;
		gpu::bindless_slot sky_view;
		gpu::bindless_slot sky_view_sampler;
		gpu::acceleration_structure_arg scene_tlas{};
		gpu::bindless_slot shadow_cascade_atlas;
		gpu::bindless_slot shadow_local_atlas;
		gpu::bindless_slot shadow_tiles;
		gpu::bindless_slot light_shadow_tiles;
		gpu::bindless_slot ambient_occlusion;
		gpu::bindless_slot reflection;
		meshlet_push_constants push_constants{};
	};

	template <typename Bindings>
	auto bind_surface(
		Bindings& bindings,
		const surface_frame& surface
	) -> void;

	enum class shadow_quality_level : int {
		off = 0,
		hard = 1,
		low = 2,
		medium = 3,
		high = 4
	};

	enum class ao_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
		ultra = 4
	};

	enum class reflection_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3
	};

	struct [[= system_state<"Forward">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Shadow map resolution and filtering quality. Off disables shadow rendering "
									  "entirely.">{}
		]]
		[[= shared]] shadow_quality_level shadow_quality = shadow_quality_level::medium;

		[[
			= settings::describe<"Trace a shadow ray only for lights whose attenuated irradiance at the surface "
									  "reaches this. Dimmer lights still light the surface, they are just left "
									  "unshadowed, so the error is bounded by their own contribution. The trace runs "
									  "once per light per pixel and dominates scenes with many lights. Zero shadows "
									  "every light, which is the behaviour this setting replaced.">{},
			= settings::range<watts_per_square_meter(0.f), watts_per_square_meter(1.f)>{}
		]]
		irradiance shadow_cutoff = watts_per_square_meter(0.1f);

		[[
			= settings::describe<"Screen-space ambient occlusion sample count and blur quality.">{}
		]]
		[[= shared]] ao_quality_level ao_quality = ao_quality_level::medium;

		[[
			= settings::
				describe<"Screen-space and ray-traced reflection quality. Higher levels trace more rays "
						 "per pixel.">{}
		]]
		[[= shared]] reflection_quality_level reflection_quality = reflection_quality_level::medium;

		gpu::shader_program pipeline;
		bool vertex_pulled = false;
		[[= shared]] bool ray_traced = false;
		[[= shared]] std::optional<surface_frame> surface;

		per_frame_resource<gpu::buffer> light_buffers;

		std::optional<lighting_source::ray_traced_scene> ray_traced_scene;
		std::optional<lighting_source::raster_shadows> raster_shadows;
		std::optional<lighting_source::screen_ambient_occlusion> ambient_occlusion;
		std::optional<lighting_source::screen_reflections> reflections;

		gpu::bindless_handle gi_sampler;
		gpu::bindless_handle material_sampler;
		gpu::image material_fallback;

		linear_vector<std::byte> light_staging;
		std::uint32_t frame_counter = 0;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<lighting::data> lighting_s,
		shared_view<light_culling::data> lc_r,
		shared_view<atmosphere::data> atm_state,
		shared_view<geometry_collector::data> gc_state,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<camera::data> cam_state,
		shared_view<camera_latch::data> latch_s,
		shared_view<geometry_collector::data> gc_r,
		shared_view<light_culling::data> lc_r,
		shared_view<atmosphere::data> atm_state,
		shared_view<cloud::data> cloud_state,
		std::optional<shared_view<gi_probe::data>> gi_s,
		read<directional_light_component> dir_lights,
		read<spot_light_component> spot_lights,
		read<point_light_component> point_lights,
		read<physics::transform_component> transforms,
		channel_read<camera::temporal_reset_request> reset_in,
		shared_view<lighting::data> lighting_s,
		channel_read<lighting_source::ray_traced_scene, lighting_source::raster_shadows, lighting_source::screen_ambient_occlusion, lighting_source::screen_reflections> lighting_in
	) -> async::task<>;
}

template <typename Bindings>
auto gse::renderer::forward::bind_surface(Bindings& bindings, const surface_frame& surface) -> void {
	bindings.camera_ubo = surface.camera_ubo;
	bindings.lights_ssbo = surface.lights;
	bindings.light_index_list = surface.light_index_list;
	bindings.tile_light_table = surface.tile_light_table;
	bindings.aerial_perspective_in = surface.aerial_perspective;
	bindings.gi_atlas = surface.gi_atlas;
	bindings.ap_sampler = surface.ap_sampler;
	bindings.gi_atlas_sampler = surface.gi_atlas_sampler;
	bindings.cloud_shadow_in = surface.cloud_shadow;
	bindings.cloud_shadow_sampler = surface.cloud_shadow_sampler;
	bindings.material_palette = surface.material_palette;
	bindings.atmosphere_ubo = surface.atmosphere_ubo;
	bindings.cloud_shadow_ubo = surface.cloud_shadow_ubo;
	bindings.sky_view_in = surface.sky_view;
	bindings.sky_view_sampler = surface.sky_view_sampler;
	if constexpr (requires { bindings.scene_tlas; }) {
		bindings.scene_tlas = surface.scene_tlas;
	}
	else {
		bindings.shadow_cascade_atlas = surface.shadow_cascade_atlas;
		bindings.shadow_local_atlas = surface.shadow_local_atlas;
		bindings.shadow_tiles = surface.shadow_tiles;
		bindings.light_shadow_tiles = surface.light_shadow_tiles;
		bindings.ambient_occlusion_in = surface.ambient_occlusion;
		bindings.reflection_in = surface.reflection;
	}
}