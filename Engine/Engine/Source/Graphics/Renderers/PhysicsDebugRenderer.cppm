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

auto gse::renderer::physics_debug::ensure_vertex_capacity(system::frame_data& fd, gpu::context& ctx, const std::size_t frame_index, const std::size_t required_vertex_count) -> void {
	auto& max_verts = fd.max_vertices[frame_index];
	auto& vertex_buffer = fd.vertex_buffers[frame_index];

	if (required_vertex_count <= max_verts && vertex_buffer) {
		return;
	}

	if (vertex_buffer) {
		vertex_buffer = {};
	}

	max_verts = std::max<std::size_t>(max_verts, 4096);
	while (max_verts < required_vertex_count) {
		max_verts *= 2;
	}

	vertex_buffer = gpu::create_buffer(ctx, {
		.size = max_verts * sizeof(debug_vertex),
		.usage = gpu::buffer_flag::vertex
	});
}

auto gse::renderer::physics_debug::system::initialize(const init_context& phase, resources& r, frame_data& fd, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& assets = *static_cast<asset_registry<gpu::context>*>(phase.assets_ptr);

	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Physics Debug Renderer Enabled",
		.description = "Enable or disable outlines on collision boxes & visible impulse vectors",
		.ref = &s.enabled,
		.type = typeid(bool)
	});

	r.shader_handle = assets.get<shader>("Shaders/Standard3D/physics_debug");
	assets.instantly_load(r.shader_handle);

	const auto camera_ubo = r.shader_handle->uniform_block("CameraUBO");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx, {
			.size = camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});

		r.descriptors[i] = gpu::allocate_descriptors(ctx, *r.shader_handle);

		gpu::descriptor_writer(ctx, r.shader_handle, r.descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, camera_ubo.size)
			.commit();
	}

	r.pipeline = gpu::create_graphics_pipeline(ctx, *r.shader_handle, {
		.rasterization = {
			.polygon = gpu::polygon_mode::line,
			.cull = gpu::cull_mode::none
		},
		.depth = { .test = false, .write = false },
		.blend = gpu::blend_preset::alpha,
		.depth_fmt = gpu::depth_format::none,
		.topology = gpu::topology::line_list
	});
}

auto gse::renderer::physics_debug::add_line(const vec3<position>& a, const vec3<position>& b, const vec3f& color, std::vector<debug_vertex>& out_vertices) -> void {
	out_vertices.push_back(debug_vertex{ a, color });
	out_vertices.push_back(debug_vertex{ b, color });
}

auto gse::renderer::physics_debug::build_obb_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	auto bb = coll.bounding_box;
	if (mc) {
		bb.update(mc->current_position, mc->orientation);
	}
	std::array<vec3<position>, 8> corners;

	const auto half = bb.half_extents();
	const auto axes = bb.obb().axes;
	const auto center = bb.center();

	int idx = 0;
	for (int i = 0; i < 8; ++i) {
		const auto x = (i & 1 ? 1.0f : -1.0f) * half.x();
		const auto y = (i & 2 ? 1.0f : -1.0f) * half.y();
		const auto z = (i & 4 ? 1.0f : -1.0f) * half.z();
		corners[idx++] = center + axes[0] * x + axes[1] * y + axes[2] * z;
	}

	static constexpr std::array<std::pair<int, int>, 12> edges{ {
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	} };

	constexpr vec3f color{ 0.0f, 1.0f, 0.0f };

	for (const auto& [fst, snd] : edges) {
		add_line(corners[fst], corners[snd], color, out_vertices);
	}
}

auto gse::renderer::physics_debug::build_sphere_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	auto bb = coll.bounding_box;
	if (mc) {
		bb.update(mc->current_position, mc->orientation);
	}

	const auto center = bb.center();
	const auto axes = bb.obb().axes;
	const auto radius = coll.shape_radius;

	constexpr vec3f color{ 0.0f, 1.0f, 0.0f };

	for (int ring = 0; ring < 3; ++ring) {
		constexpr int segments = 32;
		const auto& u = axes[ring];
		const auto& v = axes[(ring + 1) % 3];

		for (int i = 0; i < segments; ++i) {
			const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / segments;
			const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / segments;

			const auto p0 = center + u * (radius * std::cos(a0)) + v * (radius * std::sin(a0));
			const auto p1 = center + u * (radius * std::cos(a1)) + v * (radius * std::sin(a1));

			add_line(p0, p1, color, out_vertices);
		}
	}
}

auto gse::renderer::physics_debug::build_capsule_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	auto bb = coll.bounding_box;
	if (mc) {
		bb.update(mc->current_position, mc->orientation);
	}

	const auto center = bb.center();
	const auto axes = bb.obb().axes;
	const auto radius = coll.shape_radius;
	const auto half_height = coll.shape_half_height;

	const auto& cap_axis = axes[1];
	const auto& u = axes[0];
	const auto& v = axes[2];

	const auto top = center + cap_axis * half_height;
	const auto bottom = center - cap_axis * half_height;

	constexpr vec3f color{ 0.0f, 1.0f, 0.0f };
	constexpr int segments = 24;
	constexpr int half_segments = segments / 2;

	for (const auto& ring_center : { top, bottom }) {
		for (int i = 0; i < segments; ++i) {
			const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / segments;
			const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / segments;
			const auto p0 = ring_center + u * (radius * std::cos(a0)) + v * (radius * std::sin(a0));
			const auto p1 = ring_center + u * (radius * std::cos(a1)) + v * (radius * std::sin(a1));
			add_line(p0, p1, color, out_vertices);
		}
	}

	for (int i = 0; i < 4; ++i) {
		const float angle = static_cast<float>(i) * std::numbers::pi_v<float> * 0.5f;
		const auto offset = u * (radius * std::cos(angle)) + v * (radius * std::sin(angle));
		add_line(top + offset, bottom + offset, color, out_vertices);
	}

	for (int plane = 0; plane < 2; ++plane) {
		const auto& ring_u = (plane == 0) ? u : v;

		for (int i = 0; i < half_segments; ++i) {
			const float a0 = std::numbers::pi_v<float> * static_cast<float>(i) / half_segments;
			const float a1 = std::numbers::pi_v<float> * static_cast<float>(i + 1) / half_segments;

			const auto p0 = top + ring_u * (radius * std::cos(a0)) + cap_axis * (radius * std::sin(a0));
			const auto p1 = top + ring_u * (radius * std::cos(a1)) + cap_axis * (radius * std::sin(a1));
			add_line(p0, p1, color, out_vertices);

			const auto q0 = bottom + ring_u * (radius * std::cos(a0)) - cap_axis * (radius * std::sin(a0));
			const auto q1 = bottom + ring_u * (radius * std::cos(a1)) - cap_axis * (radius * std::sin(a1));
			add_line(q0, q1, color, out_vertices);
		}
	}
}

auto gse::renderer::physics_debug::build_shape_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	switch (coll.shape) {
	case physics::shape_type::sphere:
		build_sphere_lines_for_collider(coll, mc, out_vertices);
		break;
	case physics::shape_type::capsule:
		build_capsule_lines_for_collider(coll, mc, out_vertices);
		break;
	default:
		build_obb_lines_for_collider(coll, mc, out_vertices);
		break;
	}
}

auto gse::renderer::physics_debug::build_contact_debug_for_collider(const physics::collision_component& coll, const physics::motion_component& mc, std::vector<debug_vertex>& out_vertices) -> void {
	const auto& [colliding, collision_normal, penetration, collision_points] = coll.collision_information;
	if (!colliding || mc.position_locked) {
		return;
	}

	const float t = std::clamp(penetration / meters(0.05f), 0.f, 1.f);
	const vec3f satisfaction_color{ t, 1.f - t, 0.f };

	for (auto& collision_point : collision_points) {
		const auto p = collision_point;
		const auto n = collision_normal;

		constexpr length cross_size = meters(0.05f);
		constexpr vec3f normal_color{ 0.f, 0.7f, 1.f };

		const auto px1 = p + vec3<length>{ cross_size, meters(0.f), meters(0.f) };
		const auto px2 = p - vec3<length>{ cross_size, meters(0.f), meters(0.f) };

		const auto py1 = p + vec3<length>{ meters(0.f), cross_size, meters(0.f) };
		const auto py2 = p - vec3<length>{ meters(0.f), cross_size, meters(0.f) };

		const auto pz1 = p + vec3<length>{ meters(0.f), meters(0.f), cross_size };
		const auto pz2 = p - vec3<length>{ meters(0.f), meters(0.f), cross_size };

		add_line(px1, px2, satisfaction_color, out_vertices);
		add_line(py1, py2, satisfaction_color, out_vertices);
		add_line(pz1, pz2, satisfaction_color, out_vertices);

		constexpr length min_normal_len = meters(0.15f);
		const length normal_len = std::max<length>(std::min<length>(penetration, meters(0.5f)), min_normal_len);
		const auto normal_end = p + n * normal_len;
		add_line(p, normal_end, normal_color, out_vertices);
	}
}

auto gse::renderer::physics_debug::system::update(update_context& ctx, const resources& r, state& s) -> async::task<> {
	if (!s.enabled) {
		co_return;
	}

	constexpr std::size_t max_shape_debug_vertices = 256;
	auto [motions, collisions] = co_await ctx.acquire<
		read<physics::motion_component>,
		read<physics::collision_component>
	>();

	std::vector<debug_vertex> vertices;
	vertices.reserve(collisions.size() * max_shape_debug_vertices);
	debug_stats stats;

	for (const auto& mc : motions) {
		stats.body_count++;
		if (mc.sleeping) {
			stats.sleeping_count++;
		}

		const auto lin = magnitude(mc.current_velocity);
		const auto ang = magnitude(mc.angular_velocity);
		if (lin > stats.max_linear_speed) {
			stats.max_linear_speed = lin;
		}
		if (ang > stats.max_angular_speed) {
			stats.max_angular_speed = ang;
		}
	}

	for (const auto& coll : collisions) {
		if (!coll.resolve_collisions) {
			continue;
		}

		const auto* mc = motions.find(coll.owner_id());
		build_shape_lines_for_collider(coll, mc, vertices);

		if (mc) {
			build_contact_debug_for_collider(coll, *mc, vertices);
		}

		if (coll.collision_information.colliding) {
			stats.colliding_pairs++;
			if (coll.collision_information.penetration > stats.max_penetration) {
				stats.max_penetration = coll.collision_information.penetration;
			}
		}
	}

	if (const auto* ps = ctx.try_state_of<physics::state>()) {
		if (ps->use_gpu_solver && ps->gpu_stats.active) {
			stats.gpu_solver_active = true;
			stats.contact_count = ps->gpu_stats.contact_count;
			stats.motor_count = ps->gpu_stats.motor_count;
			stats.solve_time = ps->gpu_stats.solve_time;
		}
	}

	s.latest_stats = stats;
	ctx.channels.push(render_data{ std::move(vertices), stats });

	co_return;
}

auto gse::renderer::physics_debug::system::frame(const frame_context& ctx, const resources& r, frame_data& fd, const state& s) -> async::task<> {
	auto& gpu = ctx.get<gpu::context>();

	if (!s.enabled) {
		co_return;
	}

	if (!gpu.graph().frame_in_progress()) {
		co_return;
	}

	const auto& render_items = ctx.read_channel<render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& verts = render_items[0].vertices;
	if (verts.empty()) {
		co_return;
	}

	const auto frame_index = gpu.graph().current_frame();
	ensure_vertex_capacity(fd, gpu, frame_index, verts.size());

	auto& vertex_buffer = fd.vertex_buffers[frame_index];
	if (auto* dst = vertex_buffer.mapped()) {
		gse::memcpy(dst, verts);
	}

	const auto* cam_state = ctx.try_state_of<camera::state>();
	const auto view_matrix = cam_state ? cam_state->view_matrix : gse::view_matrix{};
	const auto proj_matrix = cam_state ? cam_state->projection_matrix : projection_matrix{};

	r.shader_handle->set_uniform("CameraUBO.view", view_matrix, r.ubo_allocations.at("CameraUBO")[frame_index]);
	r.shader_handle->set_uniform("CameraUBO.proj", proj_matrix, r.ubo_allocations.at("CameraUBO")[frame_index]);

	const auto ext = gpu.graph().extent();
	const auto vertex_count = static_cast<std::uint32_t>(verts.size());

	auto pass = gpu.graph().add_pass<state>();
	pass.track(r.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.color_output_load();

	auto& rec = co_await pass.record();
	rec.bind(r.pipeline);
	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
	rec.bind_vertex(vertex_buffer);
	rec.draw(vertex_count);
}
