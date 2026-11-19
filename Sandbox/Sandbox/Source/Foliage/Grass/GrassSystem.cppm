export module sandbox:grass_system;

import std;

import gse;
import gse.gpu;
import gse.gpu_record;

import :grass_field;

namespace sandbox::grass {
	using namespace gse;
	using namespace gse::physics;
}

export namespace sandbox::grass {
	constexpr std::uint32_t cell_capacity = 16;
	constexpr std::uint32_t segment_count = 4;

	struct [[= shaders::shader_constant_block]] grass_layout {
		std::uint32_t grass_segments = segment_count;
		std::uint32_t grass_nodes = segment_count + 1;
		std::uint32_t grass_side_vertices = 2 * segment_count + 1;
		std::uint32_t grass_blade_triangles = 2 * segment_count - 1;
		std::uint32_t grass_tile_blades = tile_blades;
		std::uint32_t grass_tile_blade_count = tile_blades * tile_blades;
		std::uint32_t grass_slot_count = 128;
		std::uint32_t grass_max_contacts = cell_capacity;
		std::uint32_t grass_sim_threads = 64;
		std::uint32_t grass_compact_threads = 256;
		std::uint32_t grass_segment_search_steps = 6;
		float grass_jitter_fraction = 0.35f;
		angle grass_lean_base = radians(0.08f);
		angle grass_lean_spread = radians(0.22f);
		angle grass_curl_base = radians(0.05f);
		angle grass_curl_spread = radians(0.12f);
		float grass_height_base = 0.8f;
		float grass_height_spread = 0.4f;
		float grass_reach_factor = 1.2f;
	};

	constexpr grass_layout layout{};

	static_assert(layout.grass_slot_count <= layout.grass_compact_threads);

	struct grid_pass {};
	struct simulate_pass {};

	struct [[= shaders::shader_struct]] grass_node {
		vec3<displacement> position;
		vec3<velocity> velocity;
		vec3<displacement> previous;
		vec3<displacement> plastic;
	};

	struct [[= shaders::shader_struct]] grass_tile {
		std::uint32_t slot_plus_one;
		std::uint32_t calm_steps;
		std::uint32_t wake;
		std::uint32_t sleep_ready;
	};

	struct [[= shaders::shader_struct]] grass_slot {
		std::uint32_t tile_plus_one;
		std::uint32_t fresh;
		std::uint32_t unsettled;
		std::uint32_t reserved;
	};

	struct [[= shaders::shader_struct]] grass_status {
		std::uint32_t simulate_groups_x;
		std::uint32_t simulate_groups_y;
		std::uint32_t simulate_groups_z;
		std::uint32_t awake_tiles;
		std::uint32_t overflow_total;
	};

	struct [[= shaders::shader_struct]] grass_params {
		quat field_orientation;
		vec3<position> field_position;
		vec3<acceleration> local_gravity;
		length half_extent;
		length spacing;
		length blade_height;
		length root_width;
		length tip_width;
		length cell_size;
		length contact_margin;
		length blade_reach;
		length sleep_tolerance;
		velocity sleep_speed;
		time_t<float, seconds> substep_dt;
		mass node_mass;
		stiffness stretch_stiffness;
		angular_stiffness bend_stiffness;
		stiffness contact_stiffness;
		inverse_time damping;
		angle yield_angle;
		inverse_time yield_rate;
		angular_velocity recovery_rate;
		std::uint32_t blade_count = 0;
		std::uint32_t blades_per_side = 0;
		std::uint32_t tiles_per_side = 0;
		std::uint32_t tile_count = 0;
		std::uint32_t cells_per_side = 0;
		std::uint32_t cell_capacity = 0;
		std::uint32_t substeps = 0;
		std::uint32_t sleep_steps = 0;
		std::uint32_t iterations = 0;
		std::uint32_t body_count = 0;
		std::uint32_t passthrough_words = 0;
		std::uint32_t reaction_bodies = 0;
		std::uint32_t reaction_stamp_word = 0;
		std::uint32_t reaction_stamp = 0;
		std::uint32_t route_count = 0;
	};

	using shader_types = type_pack<grass_layout, grass_params, grass_node, grass_tile, grass_slot, grass_status>;

	struct field_instance {
		id owner;
		grass_field field;
		transform_component transform;
	};

	struct field_buffers {
		grass_field field;
		transform_component transform;
		std::uint32_t blade_count = 0;
		std::uint32_t tile_count = 0;
		std::uint32_t cells_per_side = 0;
		gpu::buffer nodes;
		gpu::buffer tiles;
		gpu::buffer slots;
		gpu::buffer awake;
		gpu::buffer status;
		gpu::buffer grid;
		gpu::buffer contact;
		per_frame_resource<gpu::buffer> params;
		bool cleared = false;
	};

	using field_buffer_map = std::flat_map<id, field_buffers>;

	struct body_binding {
		gpu::bindless_slot slot;
		std::uint32_t count = 0;
	};

	struct [[= system_state<"Grass">{}, = gpu::requires_capability<^^gpu::backend_capabilities::shader_int64>{}]] data {
		gpu::shader_program grid_pipeline;
		gpu::shader_program wake_pipeline;
		gpu::shader_program compact_pipeline;
		gpu::shader_program simulate_pipeline;
		gpu::shader_program settle_pipeline;

		std::vector<field_instance> fields;
		std::vector<vbd::body_state> cpu_bodies;
		std::vector<std::uint32_t> passthrough_bits;
		std::vector<std::uint32_t> reaction_routes;
		bool solver_bodies = false;
		std::uint32_t substeps = 0;
		time_t<float, seconds> substep_dt{};

		[[= shared]] field_buffer_map buffers;
		[[= shared]] body_binding bodies;
		[[= shared]] per_frame_resource<gpu::bindless_handle> reaction_views;

		per_frame_resource<gpu::bindless_handle> solver_body_views;
		per_frame_resource<gpu::buffer> cpu_body_buffers;
		per_frame_resource<std::size_t> cpu_body_capacity;
		per_frame_resource<gpu::buffer> body_copies;
		per_frame_resource<std::size_t> body_copy_capacity;
		per_frame_resource<gpu::buffer> passthrough_buffers;
		per_frame_resource<std::size_t> passthrough_capacity;
		per_frame_resource<gpu::buffer> route_buffers;
		per_frame_resource<std::size_t> route_capacity;
	};

	auto pool_bytes() -> std::size_t;

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto collect(
		context& ctx,
		data& d,
		shared_view<physics::data> phys,
		channel_read<interpolation_state> interp_in,
		read<grass_field> fields,
		read<transform_component> transforms,
		read<grass_passthrough> passthrough,
		read<grass_reaction_target> reaction_targets
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_read<gpu_solver_frame_info> solver_in,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}
