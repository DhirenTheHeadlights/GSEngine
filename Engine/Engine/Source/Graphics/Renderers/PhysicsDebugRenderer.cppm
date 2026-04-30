export module gse.graphics:physics_debug_renderer;

import std;

import gse.physics;
import gse.math;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.save;
import gse.gpu;
import gse.assets;

import :camera_system;
import gse.log;

namespace gse::renderer::physics_debug {
	struct debug_vertex {
		vec3<position> position;
		vec3f color;
	};

	auto add_line(
		const vec3<position>& a,
		const vec3<position>& b,
		const vec3f& color,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_obb_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_sphere_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_capsule_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_shape_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_contact_debug_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component& mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;
}

export namespace gse::renderer::physics_debug {
	struct debug_stats {
		std::uint32_t body_count = 0;
		std::uint32_t sleeping_count = 0;
		std::uint32_t contact_count = 0;
		std::uint32_t motor_count = 0;
		std::uint32_t colliding_pairs = 0;
		time_t<float, seconds> solve_time{};
		velocity max_linear_speed{};
		angular_velocity max_angular_speed{};
		length max_penetration{};
		bool gpu_solver_active = false;
	};

	struct render_data {
		std::vector<debug_vertex> vertices;
		debug_stats stats;
	};

	struct state {
		bool enabled = true;
		debug_stats latest_stats;
	};

	struct system {
		struct resources {
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
			resource::handle<shader> shader_handle;
			std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;
		};

		struct frame_data {
			per_frame_resource<gpu::buffer> vertex_buffers;
			per_frame_resource<std::size_t> max_vertices;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			frame_data& fd,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			const resources& r,
			state& s
		) -> async::task<>;

		static auto frame(const frame_context& ctx,
			const resources& r,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};
}

namespace gse::renderer::physics_debug {
	auto ensure_vertex_capacity(
		system::frame_data& fd,
		gpu::context& ctx,
		std::size_t frame_index,
		std::size_t required_vertex_count
	) -> void;
}

