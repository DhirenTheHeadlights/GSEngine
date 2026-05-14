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

import :camera_system;
import gse.log;

namespace gse::renderer::physics_debug {
	struct debug_vertex {
		vec3<position> position;
		vec3f color;
	};
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

	struct system {
		struct data {
			static constexpr std::string_view category = "Graphics";

			[[=gse::settings::describe<"Draw collision shapes, contact points, and joint anchors over the scene.">{}]]
			bool enabled = true;

			debug_stats latest_stats;

			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
			per_frame_resource<gpu::buffer> camera_ubo_buffers;

			per_frame_resource<gpu::buffer> vertex_buffers;
			per_frame_resource<std::size_t> max_vertices;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const asset::data& assets_s,
			data& d,
			const physics::system::data& ps
		) -> async::task<>;

		static auto frame(
			const frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<camera::system> cam_state
		) -> async::task<>;

	private:
		static auto add_line(
			const vec3<position>& a,
			const vec3<position>& b,
			const vec3f& color,
			std::vector<debug_vertex>& out_vertices
		) -> void;

		static auto build_obb_lines_for_collider(
			const physics::transform_component& tc,
			const physics::box_shape& shape,
			std::vector<debug_vertex>& out_vertices
		) -> void;

		static auto build_sphere_lines_for_collider(
			const physics::transform_component& tc,
			const physics::sphere_shape& shape,
			std::vector<debug_vertex>& out_vertices
		) -> void;

		static auto build_capsule_lines_for_collider(
			const physics::transform_component& tc,
			const physics::capsule_shape& shape,
			std::vector<debug_vertex>& out_vertices
		) -> void;

		static auto build_shape_lines_for_collider(
			const physics::collision_component& coll,
			const physics::transform_component* tc,
			std::vector<debug_vertex>& out_vertices
		) -> void;

		static auto build_contact_debug_for_collider(
			const collision_information& info,
			const physics::motion_component& mc,
			std::vector<debug_vertex>& out_vertices
		) -> void;

		static auto ensure_vertex_capacity(
			data& d,
			gpu::device& device,
			std::size_t frame_index,
			std::size_t required_vertex_count
		) -> void;
	};
}
