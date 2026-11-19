export module gse.graphics:skin_renderer;

import std;

import :animation_components;
import :deformed_target;
import :skinned_model;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.physics;
import gse.gpu_record;

export namespace gse::renderer::skin {
	struct palette_pass {};
	struct deform_pass {};

	struct [[= shaders::shader_struct]] rig_bone_binding {
		mat4f inverse_bind;
		mat4f world;
		std::uint32_t valid = 0;
	};

	struct [[= shaders::shader_struct]] skin_matrix {
		mat4f skin;
		std::uint32_t valid = 0;
	};

	struct skinned_instance {
		id owner;
		std::shared_ptr<const skinned_model> model;
		std::uint32_t mesh_index = 0;
		std::uint32_t bone_offset = 0;
		std::uint32_t vertex_offset = 0;
		std::uint32_t vertex_count = 0;
	};

	using deformed_target_map = std::flat_map<std::pair<id, std::uint32_t>, deformed_target>;

	using skinned_bounds_map = std::flat_map<id, aabb>;

	struct [[= system_state<"Skin">{}]] data {
		gpu::shader_program palette_pipeline;
		gpu::shader_program deform_pipeline;
		bool initialized = false;

		std::vector<skinned_instance> instances;
		std::vector<rig_bone_binding> bone_bindings;
		[[= shared]] deformed_target_map targets;
		[[= shared]] skinned_bounds_map bounds;

		per_frame_resource<gpu::buffer> bone_buffers;
		std::size_t bone_buffer_size = 0;

		per_frame_resource<gpu::buffer> palette_buffers;
		std::size_t palette_buffer_size = 0;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto collect(
		context& ctx,
		data& d,
		channel_read<physics::interpolation_state> interp_in,
		read<skeleton_instance_component> skeletons,
		read<physics::transform_component> transforms,
		read<physics::motion_component> motions,
		read<physics::collision_component> collisions
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;

	auto deformed_slots_for(
		const deformed_target_map& targets,
		id owner,
		std::uint32_t mesh_index
	) -> deformed_slots;
}