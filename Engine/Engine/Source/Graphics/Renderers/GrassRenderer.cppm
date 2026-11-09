export module gse.graphics:grass_renderer;

import std;

import :camera_system;
import :lighting_source;
import :shared_shaders;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.physics;

export namespace gse::renderer::grass {
	struct [[= shaders::shader_constant_block]] grass_views {
		std::uint32_t grass_camera_view = 0;
		std::uint32_t grass_first_shadow_view = 1;
		std::uint32_t grass_max_views = 72;
		std::uint32_t grass_args_words = 5;
	};

	constexpr grass_views views{};

	struct cull_pass {};

	struct draw_bindings {
		shaders::ssbo_readonly<physics::grass::grass_params> grass_params_in;
		shaders::ssbo_readonly<physics::grass::grass_node> grass_pool;
		shaders::ssbo_readonly<physics::grass::grass_tile> grass_tiles;
		shaders::ssbo_readonly<std::uint32_t> grass_visible;
	};

	struct cull_bindings {
		shaders::ssbo_readonly<physics::grass::grass_params> grass_params_in;
		shaders::ssbo_readwrite<std::uint32_t> cull_visible_out;
		shaders::ssbo_readwrite<std::uint32_t> cull_args_out;
		shaders::ssbo_readonly<shaders::raster_shadow::shadow_tile> cull_shadow_tiles;
	};

	using shader_types = type_pack_concat<physics::grass::shader_types, type_pack<grass_views>>::type;

	struct [[= shaders::shader_struct]] cull_push_constants {
		view_projection_matrix camera_view_projection;
		std::uint32_t first_view;
		std::uint32_t shadow_views;
	};

	using cull_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_cull">,
		gpu::types<shader_types, shaders::raster_shadow::shader_types>,
		gpu::bindings<cull_bindings>,
		gpu::helpers<"Grass/grass_shape">,
		gpu::threads<64>,
		gpu::push_constant<cull_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	struct field_render {
		std::uint32_t tile_count = 0;
		bool culled = false;
		gpu::bindless_slot pool;
		gpu::bindless_slot tiles;
		per_frame_resource<gpu::bindless_slot> params;
		gpu::buffer visible;
		gpu::buffer args;
		per_frame_resource<gpu::buffer> instance;
		std::optional<spatial_matrix> previous_model;
	};

	struct [[= system_state<"GrassRenderer">{}]] data {
		gpu::shader_program cull_pipeline;
		gpu::buffer index_upload;
		[[= shared]] gpu::buffer indices;
		bool indices_uploaded = false;
		[[= shared]] std::flat_map<id, field_render> fields;
	};

	constexpr auto index_count() -> std::uint32_t;

	auto args_offset(
		std::uint32_t view
	) -> std::size_t;

	auto record_cull(
		gpu::recording_context& rec,
		const field_render& field,
		std::uint32_t frame_index,
		const cull_push_constants& pc,
		std::uint32_t view_count,
		gpu::bindless_slot shadow_tiles
	) -> void;

	auto record_draw(
		gpu::recording_context& rec,
		const gpu::buffer& indices,
		const field_render& field,
		std::uint32_t view
	) -> void;

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<physics::grass::data> grass_s,
		shared_view<camera::data> cam_state,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}

constexpr auto gse::renderer::grass::index_count() -> std::uint32_t {
	return physics::grass::layout.grass_tile_blade_count * physics::grass::layout.grass_blade_triangles * 3;
}
