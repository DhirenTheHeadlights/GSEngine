module gse.graphics;

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
import gse.log;

import :physics_debug_renderer;
import :forward_renderer;
import :camera_system;
import :settings;

namespace gse::renderer::physics_debug {
	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/physics_debug">,
		gpu::layout<"standard_3d">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<shaders::standard_3d::shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::rasterization<gpu::polygon_mode::line, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::primitive_topology<gpu::topology::line_list>
	>;
}

auto gse::renderer::physics_debug::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state) -> async::task<> {
	if (!d.enabled) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
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

	const auto frame_index = gpu_s.render_graph->current_frame();
	ensure_vertex_capacity(d, *gpu_s.device, frame_index, verts.size());

	auto& vertex_buffer = d.vertex_buffers[frame_index];
	vertex_buffer.host_write(verts);

	const auto view_matrix = cam_state.view_matrix;
	const auto proj_matrix = cam_state.projection_matrix;

	const shaders::common::camera_data camera{
		.view = view_matrix,
		.proj = proj_matrix,
		.inv_view = mat4f(1.0f),
	};
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const auto ext = gpu_s.render_graph->extent();
	const auto vertex_count = static_cast<std::uint32_t>(verts.size());

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::load_color())
		.after<forward::system>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.bind_vertex(vertex_buffer);
	rec.draw(vertex_count);
}

auto gse::renderer::physics_debug::system::ensure_vertex_capacity(data& d, gpu::device& device, const std::size_t frame_index, const std::size_t required_vertex_count) -> void {
	auto& max_verts = d.max_vertices[frame_index];
	auto& vertex_buffer = d.vertex_buffers[frame_index];

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

	vertex_buffer = gpu::buffer::create(device.allocator(), {
		.size = max_verts * sizeof(debug_vertex),
		.usage = gpu::buffer_flag::vertex
	});
}

auto gse::renderer::physics_debug::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d, const physics::system::data& ps) -> async::task<> {
	d.pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = camera_ubo_size,
			.usage = gpu::buffer_flag::uniform
		});

		d.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);

		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.descriptors[i])
			.buffer<shaders::standard_3d::camera_ubo>(d.camera_ubo_buffers[i], 0, camera_ubo_size)
			.commit();
	}

	while (true) {
		if (!d.enabled) {
			co_await ctx.next_tick();
			continue;
		}

		{
			constexpr std::size_t max_shape_debug_vertices = 256;
			auto [transforms, motions, statuses, collisions, results] = co_await ctx.acquire<
				read<physics::transform_component>,
				read<physics::motion_component>,
				read<physics::motion_status_component>,
				read<physics::collision_component>,
				read<physics::collision_result_component>
			>();

			std::vector<debug_vertex> vertices;
			vertices.reserve(collisions.size() * max_shape_debug_vertices);
			debug_stats stats;

			for (const auto& mc : motions) {
				stats.body_count++;
				const auto lin = magnitude(mc.current_velocity);
				const auto ang = magnitude(mc.angular_velocity);
				if (lin > stats.max_linear_speed) {
					stats.max_linear_speed = lin;
				}
				if (ang > stats.max_angular_speed) {
					stats.max_angular_speed = ang;
				}
			}

			for (const auto& st : statuses) {
				if (st.sleeping) {
					stats.sleeping_count++;
				}
			}

			const auto collision_ids = collisions.owner_ids();
			for (std::size_t i = 0; i < collisions.size(); ++i) {
				const auto& coll = collisions[i];
				if (!coll.resolve_collisions) {
					continue;
				}

				const auto eid = collision_ids[i];
				const auto* tc = transforms.find(eid);
				const auto* mc = motions.find(eid);
				const auto* res = results.find(eid);
				build_shape_lines_for_collider(coll, tc, vertices);

				if (mc && res) {
					build_contact_debug_for_collider(*res, *mc, vertices);
				}

				if (res && res->colliding) {
					stats.colliding_pairs++;
					if (res->penetration > stats.max_penetration) {
						stats.max_penetration = res->penetration;
					}
				}
			}

			if (ps.use_gpu_solver && ps.gpu_stats.active) {
				stats.gpu_solver_active = true;
				stats.motor_count = ps.gpu_stats.motor_count;
			}

			d.latest_stats = stats;
			ctx.channels.push<render_data>({ std::move(vertices), stats });
		}

		co_await ctx.next_tick();
	}
}

auto gse::renderer::physics_debug::system::add_line(const vec3<position>& a, const vec3<position>& b, const vec3f& color, std::vector<debug_vertex>& out_vertices) -> void {
	out_vertices.push_back(debug_vertex{ a, color });
	out_vertices.push_back(debug_vertex{ b, color });
}

auto gse::renderer::physics_debug::system::build_obb_lines_for_collider(const physics::transform_component& tc, const physics::box_shape& shape, std::vector<debug_vertex>& out_vertices) -> void {
	const bounding_box bb(tc, shape);
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

auto gse::renderer::physics_debug::system::build_sphere_lines_for_collider(const physics::transform_component& tc, const physics::sphere_shape& shape, std::vector<debug_vertex>& out_vertices) -> void {
	const bounding_box bb(tc);

	const auto center = bb.center();
	const auto axes = bb.obb().axes;
	const auto radius = shape.radius;

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

auto gse::renderer::physics_debug::system::build_capsule_lines_for_collider(const physics::transform_component& tc, const physics::capsule_shape& shape, std::vector<debug_vertex>& out_vertices) -> void {
	const bounding_box bb(tc);

	const auto center = bb.center();
	const auto axes = bb.obb().axes;
	const auto radius = shape.radius;
	const auto half_height = shape.half_height;

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

auto gse::renderer::physics_debug::system::build_shape_lines_for_collider(const physics::collision_component& coll, const physics::transform_component* tc, std::vector<debug_vertex>& out_vertices) -> void {
	if (!tc) {
		return;
	}
	gse::match(coll.shape)
		.if_is([&](const physics::box_shape& s) {
			build_obb_lines_for_collider(*tc, s, out_vertices);
		})
		.else_if_is([&](const physics::sphere_shape& s) {
			build_sphere_lines_for_collider(*tc, s, out_vertices);
		})
		.else_if_is([&](const physics::capsule_shape& s) {
			build_capsule_lines_for_collider(*tc, s, out_vertices);
		});
}

auto gse::renderer::physics_debug::system::build_contact_debug_for_collider(const collision_information& info, const physics::motion_component& mc, std::vector<debug_vertex>& out_vertices) -> void {
	const auto& [colliding, collision_normal, penetration, collision_points] = info;
	if (!colliding || !physics::is_dynamic(mc)) {
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
