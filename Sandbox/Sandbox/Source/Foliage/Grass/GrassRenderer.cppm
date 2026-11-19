export module sandbox:grass_renderer;

import std;

import gse;
import gse.gpu;
import gse.gpu_record;

import :grass_field;
import :grass_system;

namespace sandbox::grass::render {
	using namespace gse;
	using namespace gse::renderer;
}

export namespace sandbox::grass::render {
	struct [[= shaders::shader_constant_block]] grass_views {
		std::uint32_t grass_camera_view = 0;
		std::uint32_t grass_first_shadow_view = 1;
		std::uint32_t grass_max_views = 72;
		std::uint32_t grass_args_words = 5;
	};

	constexpr grass_views views{};

	static_assert(raster_shadow::max_tiles + views.grass_first_shadow_view <= views.grass_max_views);

	struct [[= shaders::shader_constant_block]] grass_rt_layout {
		std::uint32_t grass_tlas_instance_bytes = sizeof(gpu::acceleration_structure_instance);
		std::uint32_t grass_tlas_mask = 0xFF;
		std::uint32_t grass_tlas_cull_disable = 1;
	};

	constexpr std::uint32_t rt_static_batch = 64;
	constexpr std::uint32_t rt_dynamic_growth = 16;
	constexpr gpu::device_size rt_static_budget = gpu::device_size{ 2 } * 1024 * 1024 * 1024;

	struct cull_pass {};
	struct depth_pass {};
	struct shadow_cull_pass {};
	struct cascade_pass {};
	struct local_pass {};
	struct forward_pass {};
	struct rt_mesh_pass {};
	struct rt_instances_pass {};

	struct draw_bindings {
		shaders::ssbo_readonly<grass_params> grass_params_in;
		shaders::ssbo_readonly<grass_node> grass_pool;
		shaders::ssbo_readonly<grass_tile> grass_tiles;
		shaders::ssbo_readonly<std::uint32_t> grass_visible;
	};

	struct cull_bindings {
		shaders::ssbo_readonly<grass_params> grass_params_in;
		shaders::ssbo_readwrite<std::uint32_t> cull_visible_out;
		shaders::ssbo_readwrite<std::uint32_t> cull_args_out;
		shaders::ssbo_readonly<shaders::raster_shadow::shadow_tile> cull_shadow_tiles;
	};

	using shader_types = type_pack_concat<grass::shader_types, type_pack<grass_views>>::type;

	struct [[= shaders::shader_struct]] cull_push_constants {
		view_projection_matrix camera_view_projection;
		std::uint32_t first_view;
		std::uint32_t shadow_views;
	};

	struct rt_occupied_slot {
		std::uint32_t slot = 0;
		std::uint32_t tile_plus_one = 0;
	};

	struct rt_field {
		grass_field field;
		std::uint32_t tile_count = 0;
		bool over_budget = false;
		std::vector<gpu::blas> static_blas;
		std::uint32_t static_built = 0;
		per_frame_resource<std::uint32_t> static_published;
		per_frame_resource<gpu::buffer> static_addresses;
		per_frame_resource<gpu::buffer> static_vertices;
		per_frame_resource<std::vector<gpu::blas>> dynamic_blas;
		per_frame_resource<std::vector<std::uint32_t>> dynamic_built_tile;
		per_frame_resource<gpu::buffer> dynamic_vertices;
		per_frame_resource<std::uint32_t> dynamic_capacity;
		per_frame_resource<gpu::buffer> dynamic_addresses;
		gpu::readback_channel slot_readback;
		std::uint64_t slot_generation = 0;
		std::uint64_t slot_applied = 0;
		std::vector<rt_occupied_slot> occupied;
	};

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

	struct [[= system_state<"GrassRenderer">{}, = gpu::requires_capability<^^gpu::backend_capabilities::shader_int64>{}, = gpu::requires_capability<^^gpu::backend_capabilities::shader_target>{}]] data {
		gpu::shader_program cull_pipeline;
		gpu::shader_program depth_pipeline;
		gpu::shader_program shadow_pipeline;
		gpu::shader_program forward_pipeline;
		gpu::shader_program rt_mesh_pipeline;
		gpu::shader_program rt_instance_pipeline;
		bool ray_traced = false;

		gpu::buffer index_upload;
		gpu::buffer indices;
		bool indices_uploaded = false;
		std::flat_map<id, field_render> fields;

		std::uint32_t reserved_instances = 0;
		gpu::device_size rt_tile_blas_bytes = 0;
		gpu::device_size rt_tile_scratch = 0;
		per_frame_resource<gpu::buffer> rt_scratch;
		std::flat_map<id, rt_field> rt_fields;
	};

	constexpr auto index_count() -> std::uint32_t;

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<forward::data> forward_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<grass::data> grass_s,
		shared_view<camera::data> cam_state,
		shared_view<camera_latch::data> latch_s,
		shared_view<forward::data> forward_s,
		shared_view<geometry_collector::data> gc_s,
		std::optional<shared_view<raster_shadow::data>> shadow_s,
		std::optional<shared_view<scene_acceleration::data>> rt_s,
		channel_write<gpu::render_pass_request> pass_out,
		channel_write<scene_acceleration::instance_reservation, geometry_collector::material_reservation> reservations_out
	) -> async::task<>;
}

constexpr auto sandbox::grass::render::index_count() -> std::uint32_t {
	return layout.grass_tile_blade_count * layout.grass_blade_triangles * 3;
}
