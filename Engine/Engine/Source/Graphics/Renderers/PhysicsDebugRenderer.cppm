export module gse.graphics:physics_debug_renderer;

import std;

import gse.physics;
import gse.math;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.save;
import gse.meta;
import gse.gpu;
import gse.assets;
import gse.gpu_record;

import :camera_system;

namespace gse::renderer::physics_debug {
	struct [[= shaders::shader_struct]] debug_vertex {
		vec3<position> position;
		vec3f color;
	};
}

export namespace gse::renderer::physics_debug {
	struct [[= shaders::shader_struct]] shape_instance {
		std::uint32_t body_index;
		vec3<length> shape_scale;
		vec3f color;
	};

	struct [[= system_state<"PhysicsDebug">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Draw collision shapes, contact points, and joint anchors over the scene.">{}
		]]
		bool enabled = true;

		gpu::shader_program pipeline_instanced;
		gpu::shader_program pipeline_lines;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;

		gpu::buffer unit_box_vb;
		gpu::buffer unit_sphere_vb;
		gpu::buffer unit_capsule_vb;
		std::uint32_t unit_box_vert_count = 0;
		std::uint32_t unit_sphere_vert_count = 0;
		std::uint32_t unit_capsule_vert_count = 0;

		per_frame_resource<gpu::buffer> cpu_body_buffers;
		per_frame_resource<std::size_t> cpu_body_capacity;
		std::vector<vbd::body_state> cpu_body_staging;

		per_frame_resource<gpu::buffer> box_instance_buffers;
		per_frame_resource<gpu::buffer> sphere_instance_buffers;
		per_frame_resource<gpu::buffer> capsule_instance_buffers;
		per_frame_resource<std::size_t> box_instance_capacity;
		per_frame_resource<std::size_t> sphere_instance_capacity;
		per_frame_resource<std::size_t> capsule_instance_capacity;
		std::vector<shape_instance> box_instances;
		std::vector<shape_instance> sphere_instances;
		std::vector<shape_instance> capsule_instances;

		per_frame_resource<gpu::buffer> line_vertex_buffers;
		per_frame_resource<std::size_t> line_vertex_capacity;
		std::vector<debug_vertex> line_vertices;

		std::unordered_map<id, std::uint32_t> body_index_map;
	};

	[[= system_init{}]]
	auto init(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto prepare(
		context& ctx,
		data& d,
		shared_view<physics::data> ps
	) -> async::task<>;

	[[= system_run<1>{}]]
	auto build(
		context& ctx,
		data& d,
		read<physics::transform_component> transforms,
		read<physics::motion_component> motions,
		read<physics::collision_component> collisions,
		read<physics::collision_result_component> results
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<camera::data> cam_state
	) -> async::task<>;
}