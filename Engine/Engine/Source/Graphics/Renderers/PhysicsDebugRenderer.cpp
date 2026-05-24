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

import :physics_debug_renderer;
import :forward_renderer;
import :camera_system;
import :cloud_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :settings;
import :world_text_renderer;

namespace gse::renderer::physics_debug {
	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] body_data {
		using element = vbd::body_state;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::ssbo_readonly
	]] shape_instances {
		using element = shape_instance;
	};

	using instanced_shader_binding_types = type_pack<shaders::standard_3d::camera_ubo, body_data, shape_instances>;
	using shader_types = type_pack<shape_instance>;

	using instanced_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/physics_debug_instanced">,
		gpu::layout<"physics_debug_instanced">,
		gpu::types<shaders::common::shader_types, vbd::shader_types, shader_types>,
		gpu::bindings<instanced_shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::rasterization<gpu::polygon_mode::line, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::primitive_topology<gpu::topology::line_list>
	>;

	using lines_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/physics_debug">,
		gpu::layout<"standard_3d">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<shaders::standard_3d::shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::rasterization<gpu::polygon_mode::line, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::primitive_topology<gpu::topology::line_list>
	>;

	constexpr vec3f shape_color{ 0.0f, 1.0f, 0.0f };
	constexpr vec3f normal_color{ 0.0f, 0.7f, 1.0f };
	constexpr int sphere_segments = 32;
	constexpr int capsule_segments = 24;
	constexpr int capsule_half_segments = capsule_segments / 2;

	auto generate_unit_box() -> std::vector<vec4f>;
	auto generate_unit_sphere() -> std::vector<vec4f>;
	auto generate_unit_capsule() -> std::vector<vec4f>;

	auto add_line(
		const vec3<position>& a,
		const vec3<position>& b,
		const vec3f& color,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_contact_debug(
		const collision_information& info,
		const physics::motion_component& mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto ensure_buffer_capacity(
		gpu::device& device,
		gpu::buffer& buffer,
		std::size_t& capacity,
		std::size_t required_bytes,
		gpu::buffer_flag usage
	) -> void;
}

auto gse::renderer::physics_debug::generate_unit_box() -> std::vector<vec4f> {
	constexpr std::array<vec4f, 8> corners{
		vec4f{ -1.f, -1.f, 0.f, -1.f },
		vec4f{ +1.f, -1.f, 0.f, -1.f },
		vec4f{ -1.f, +1.f, 0.f, -1.f },
		vec4f{ +1.f, +1.f, 0.f, -1.f },
		vec4f{ -1.f, -1.f, 0.f, +1.f },
		vec4f{ +1.f, -1.f, 0.f, +1.f },
		vec4f{ -1.f, +1.f, 0.f, +1.f },
		vec4f{ +1.f, +1.f, 0.f, +1.f },
	};
	constexpr std::array<std::pair<int, int>, 12> edges{ { { 0, 1 },
														   { 1, 3 },
														   { 3, 2 },
														   { 2, 0 },
														   { 4, 5 },
														   { 5, 7 },
														   { 7, 6 },
														   { 6, 4 },
														   { 0, 4 },
														   { 1, 5 },
														   { 2, 6 },
														   { 3, 7 } } };
	std::vector<vec4f> verts;
	verts.reserve(edges.size() * 2);
	for (const auto& [a, b] : edges) {
		verts.push_back(corners[a]);
		verts.push_back(corners[b]);
	}
	return verts;
}

auto gse::renderer::physics_debug::generate_unit_sphere() -> std::vector<vec4f> {
	std::vector<vec4f> verts;
	verts.reserve(sphere_segments * 3 * 2);
	for (int ring = 0; ring < 3; ++ring) {
		for (int i = 0; i < sphere_segments; ++i) {
			const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / sphere_segments;
			const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / sphere_segments;
			const float c0 = std::cos(a0);
			const float s0 = std::sin(a0);
			const float c1 = std::cos(a1);
			const float s1 = std::sin(a1);
			vec4f p0;
			vec4f p1;
			switch (ring) {
				case 0:
					p0 = vec4f{ c0, s0, 0.f, 0.f };
					p1 = vec4f{ c1, s1, 0.f, 0.f };
					break;
				case 1:
					p0 = vec4f{ 0.f, c0, 0.f, s0 };
					p1 = vec4f{ 0.f, c1, 0.f, s1 };
					break;
				default:
					p0 = vec4f{ c0, 0.f, 0.f, s0 };
					p1 = vec4f{ c1, 0.f, 0.f, s1 };
					break;
			}
			verts.push_back(p0);
			verts.push_back(p1);
		}
	}
	return verts;
}

auto gse::renderer::physics_debug::generate_unit_capsule() -> std::vector<vec4f> {
	std::vector<vec4f> verts;
	verts.reserve(200);

	for (const float y_cyl : { +1.f, -1.f }) {
		for (int i = 0; i < capsule_segments; ++i) {
			const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / capsule_segments;
			const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / capsule_segments;
			verts.push_back(vec4f{ std::cos(a0), y_cyl, 0.f, std::sin(a0) });
			verts.push_back(vec4f{ std::cos(a1), y_cyl, 0.f, std::sin(a1) });
		}
	}

	for (int i = 0; i < 4; ++i) {
		const float angle = static_cast<float>(i) * std::numbers::pi_v<float> * 0.5f;
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		verts.push_back(vec4f{ c, +1.f, 0.f, s });
		verts.push_back(vec4f{ c, -1.f, 0.f, s });
	}

	for (int plane = 0; plane < 2; ++plane) {
		for (int i = 0; i < capsule_half_segments; ++i) {
			const float a0 = std::numbers::pi_v<float> * static_cast<float>(i) / capsule_half_segments;
			const float a1 = std::numbers::pi_v<float> * static_cast<float>(i + 1) / capsule_half_segments;
			const float c0 = std::cos(a0);
			const float s0 = std::sin(a0);
			const float c1 = std::cos(a1);
			const float s1 = std::sin(a1);
			if (plane == 0) {
				verts.push_back(vec4f{ c0, +1.f, s0, 0.f });
				verts.push_back(vec4f{ c1, +1.f, s1, 0.f });
				verts.push_back(vec4f{ c0, -1.f, -s0, 0.f });
				verts.push_back(vec4f{ c1, -1.f, -s1, 0.f });
			}
			else {
				verts.push_back(vec4f{ 0.f, +1.f, s0, c0 });
				verts.push_back(vec4f{ 0.f, +1.f, s1, c1 });
				verts.push_back(vec4f{ 0.f, -1.f, -s0, c0 });
				verts.push_back(vec4f{ 0.f, -1.f, -s1, c1 });
			}
		}
	}

	return verts;
}

auto gse::renderer::physics_debug::add_line(const vec3<position>& a, const vec3<position>& b, const vec3f& color, std::vector<debug_vertex>& out_vertices) -> void {
	out_vertices.push_back(debug_vertex{ a, color });
	out_vertices.push_back(debug_vertex{ b, color });
}

auto gse::renderer::physics_debug::build_contact_debug(const collision_information& info, const physics::motion_component& mc, std::vector<debug_vertex>& out_vertices) -> void {
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
		const length normal_len = std::max<length>(
			std::min<length>(
				penetration,
				meters(0.5f)
			),
			min_normal_len
		);
		const auto normal_end = p + n * normal_len;
		add_line(p, normal_end, normal_color, out_vertices);
	}
}

auto gse::renderer::physics_debug::ensure_buffer_capacity(gpu::device& device, gpu::buffer& buffer, std::size_t& capacity, const std::size_t required_bytes, const gpu::buffer_flag usage) -> void {
	if (required_bytes <= capacity && buffer.valid()) {
		return;
	}
	if (buffer.valid()) {
		buffer = {};
	}
	capacity = std::max<std::size_t>(capacity, 4096);
	while (capacity < required_bytes) {
		capacity *= 2;
	}
	buffer = gpu::buffer::create(
		device.allocator(),
		{
			.size = capacity,
			.usage = usage
		}
	);
}

auto gse::renderer::physics_debug::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d, const physics::system::data& ps) -> async::task<> {
	d.pipeline_instanced = gpu::build_graphics_program(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_textures,
		instanced_entry::pod
	);

	d.pipeline_lines =
		gpu::build_graphics_program(
			*gpu_s.device,
			*gpu_s.shader_registry,
			*gpu_s.bindless_textures,
			lines_entry::pod
		);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu::buffer::create(
			gpu_s.device->allocator(),
			{
				.size = camera_ubo_size,
				.usage = gpu::buffer_flag::uniform
			}
		);

		d.descriptors_box[i] =
			gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				instanced_entry::pod
			);
		d.descriptors_sphere[i] =
			gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				instanced_entry::pod
			);
		d.descriptors_capsule[i] =
			gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				instanced_entry::pod
			);

		d.descriptors_lines[i] =
			gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				lines_entry::pod
			);

		gpu::descriptor_writer(
			gpu_s.device->handle(),
			d.descriptors_lines[i]
		)
			.buffer<shaders::standard_3d::camera_ubo>(
				d.camera_ubo_buffers[i],
				0,
				camera_ubo_size
			)
			.commit();
	}

	{
		const auto box_verts = generate_unit_box();
		const auto sphere_verts = generate_unit_sphere();
		const auto capsule_verts = generate_unit_capsule();

		d.unit_box_vert_count = static_cast<std::uint32_t>(box_verts.size());
		d.unit_sphere_vert_count = static_cast<std::uint32_t>(sphere_verts.size());
		d.unit_capsule_vert_count = static_cast<std::uint32_t>(capsule_verts.size());

		d.unit_box_vb = gpu::buffer::create(
			gpu_s.device->allocator(),
			{
				.size = box_verts.size() * sizeof(vec4f),
				.usage = gpu::buffer_flag::vertex,
				.data = box_verts.data(),
			}
		);

		d.unit_sphere_vb = gpu::buffer::create(
			gpu_s.device->allocator(),
			{
				.size = sphere_verts.size() * sizeof(vec4f),
				.usage = gpu::buffer_flag::vertex,
				.data = sphere_verts.data(),
			}
		);

		d.unit_capsule_vb = gpu::buffer::create(
			gpu_s.device->allocator(),
			{
				.size = capsule_verts.size() * sizeof(vec4f),
				.usage = gpu::buffer_flag::vertex,
				.data = capsule_verts.data(),
			}
		);
	}

	while (true) {
		if (!d.enabled) {
			d.box_instances.clear();
			d.sphere_instances.clear();
			d.capsule_instances.clear();
			d.line_vertices.clear();
			d.cpu_body_staging.clear();
			co_await ctx.next_tick();
			continue;
		}

		{
			auto [transforms, motions, collisions, results] = co_await ctx.acquire_with(
				read_v<physics::transform_component>,
				read_v<physics::motion_component>,
				read_v<physics::collision_component>,
				read_v<physics::collision_result_component>
			);

			d.box_instances.clear();
			d.sphere_instances.clear();
			d.capsule_instances.clear();
			d.line_vertices.clear();
			d.cpu_body_staging.clear();

			std::unordered_map<id, std::uint32_t> body_index_map;
			const vbd::body_state* snapshot_states = nullptr;
			std::uint32_t snapshot_body_count = 0;
			const bool use_snapshot =
				ps.use_gpu_solver && ps.gpu_solver.buffers_created() && ps.gpu_solver.body_count() > 0;

			if (use_snapshot) {
				const auto safe_slot = 1u - ps.gpu_solver.latest_snapshot_slot();
				const auto bytes = ps.gpu_solver.snapshot_buffer(safe_slot).host_read();
				if (!bytes.empty()) {
					snapshot_states = reinterpret_cast<const vbd::body_state*>(bytes.data());
					snapshot_body_count = ps.gpu_solver.body_count();
					d.cpu_body_staging.resize(snapshot_body_count);
					for (std::uint32_t i = 0; i < snapshot_body_count; ++i) {
						d.cpu_body_staging[i].position = snapshot_states[i].position;
						d.cpu_body_staging[i].orientation = snapshot_states[i].orientation;
					}
					for (const auto& [eid, idx] : ps.id_to_body_index) {
						body_index_map[eid] = idx;
					}
				}
			}

			if (d.cpu_body_staging.empty()) {
				d.cpu_body_staging.resize(motions.size());
				const auto motion_ids = motions.owner_ids();
				const bool order_matches =
					transforms.size() == motions.size() && std::ranges::equal(
															   motion_ids,
															   transforms.owner_ids()
														   );
				for (std::size_t i = 0; i < motions.size(); ++i) {
					const auto eid = motion_ids[i];
					const auto* tc = order_matches ? std::addressof(transforms[i]) : transforms.find(eid);
					if (!tc) {
						continue;
					}
					auto& bs = d.cpu_body_staging[i];
					bs.position = tc->position;
					bs.orientation = tc->orientation;
					body_index_map[eid] = static_cast<std::uint32_t>(i);
				}
			}

			const auto collision_ids = collisions.owner_ids();
			for (std::size_t i = 0; i < collisions.size(); ++i) {
				const auto& coll = collisions[i];
				if (!coll.resolve_collisions) {
					continue;
				}

				const auto eid = collision_ids[i];

				const auto idx_it = body_index_map.find(eid);
				if (idx_it == body_index_map.end()) {
					continue;
				}
				const std::uint32_t body_index = idx_it->second;

				gse::match(coll.shape)
					.if_is([&](const physics::box_shape& s) {
						d.box_instances.push_back(
							shape_instance{
								.body_index = body_index,
								.shape_scale = vec3<length>{ s.size.x() * 0.5f, s.size.y() * 0.5f, s.size.z() * 0.5f },
								.color = shape_color,
							}
						);
					})
					.else_if_is([&](const physics::sphere_shape& s) {
						d.sphere_instances.push_back(
							shape_instance{
								.body_index = body_index,
								.shape_scale = vec3<length>{ s.radius, s.radius, s.radius },
								.color = shape_color,
							}
						);
					})
					.else_if_is([&](const physics::capsule_shape& s) {
						d.capsule_instances.push_back(
							shape_instance{
								.body_index = body_index,
								.shape_scale = vec3<length>{ s.radius, s.half_height, s.radius },
								.color = shape_color,
							}
						);
					});

				if (const auto* res = results.find(eid); res != nullptr) {
					if (const auto* mc = motions.find(eid); mc != nullptr) {
						build_contact_debug(*res, *mc, d.line_vertices);
					}
				}
			}
		}

		co_await ctx.next_tick();
	}
}

auto gse::renderer::physics_debug::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state) -> async::task<> {
	if (!d.enabled) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	const bool has_shape_instances =
		!d.box_instances.empty() || !d.sphere_instances.empty() || !d.capsule_instances.empty();
	const bool has_lines = !d.line_vertices.empty();

	if (!has_shape_instances && !has_lines) {
		co_return;
	}

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;

	const shaders::common::camera_data camera{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
		.inv_view_proj = (proj * view).inverse(),
		.prev_view = cam_state.prev_view_matrix,
		.prev_proj = cam_state.prev_projection_matrix,
		.jitter_ndc = cam_state.jitter_ndc,
		.prev_jitter_ndc = cam_state.prev_jitter_ndc,
	};
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const auto ext = gpu_s.render_graph->extent();
	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	const gpu::buffer* body_buffer = nullptr;
	std::size_t body_count_bytes = 0;

	if (has_shape_instances && !d.cpu_body_staging.empty()) {
		const auto body_count = static_cast<std::uint32_t>(d.cpu_body_staging.size());
		body_count_bytes = body_count * sizeof(vbd::body_state);
		ensure_buffer_capacity(
			*gpu_s.device,
			d.cpu_body_buffers[frame_index],
			d.cpu_body_capacity[frame_index],
			body_count_bytes,
			gpu::buffer_flag::storage
		);
		d.cpu_body_buffers[frame_index].host_write(d.cpu_body_staging);
		body_buffer = &d.cpu_body_buffers[frame_index];

		const auto upload_instances =
			[&](std::vector<shape_instance>& instances, gpu::buffer& instance_buffer, std::size_t& capacity) {
				if (instances.empty()) {
					return;
				}
				const auto bytes = instances.size() * sizeof(shape_instance);
				ensure_buffer_capacity(
					*gpu_s.device,
					instance_buffer,
					capacity,
					bytes,
					gpu::buffer_flag::storage
				);
				instance_buffer.host_write(instances);
			};

		upload_instances(
			d.box_instances,
			d.box_instance_buffers[frame_index],
			d.box_instance_capacity[frame_index]
		);
		upload_instances(
			d.sphere_instances,
			d.sphere_instance_buffers[frame_index],
			d.sphere_instance_capacity[frame_index]
		);
		upload_instances(
			d.capsule_instances,
			d.capsule_instance_buffers[frame_index],
			d.capsule_instance_capacity[frame_index]
		);
	}

	if (has_lines) {
		const auto bytes = d.line_vertices.size() * sizeof(debug_vertex);
		ensure_buffer_capacity(
			*gpu_s.device,
			d.line_vertex_buffers[frame_index],
			d.line_vertex_capacity[frame_index],
			bytes,
			gpu::buffer_flag::vertex
		);
		d.line_vertex_buffers[frame_index].host_write(d.line_vertices);
	}

	auto rec = co_await gpu::pass<system>(ctx)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.after<forward::system, sdf_grid::system, world_text::system, cloud::cloud_composite_pass>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (body_buffer != nullptr) {
		rec.bind(d.pipeline_instanced);

		const auto draw_shape = [&](const std::vector<shape_instance>& instances,
									const gpu::buffer& instance_buffer,
									const gpu::buffer& unit_vb,
									std::uint32_t unit_vert_count,
									gpu::descriptor_region& descriptor) {
			if (instances.empty()) {
				return;
			}
			const auto count = static_cast<std::uint32_t>(instances.size());

			gpu::descriptor_writer(
				gpu_s.device->handle(),
				descriptor
			)
				.buffer<shaders::standard_3d::camera_ubo>(
					d.camera_ubo_buffers[frame_index],
					0,
					camera_ubo_size
				)
				.buffer<body_data>(
					*body_buffer,
					0,
					body_count_bytes
				)
				.buffer<shape_instances>(
					instance_buffer,
					0,
					count * sizeof(shape_instance)
				)
				.commit();

			rec.bind_descriptors(d.pipeline_instanced, descriptor);
			rec.bind_vertex(unit_vb);
			rec.draw(unit_vert_count, count);
		};

		draw_shape(
			d.box_instances,
			d.box_instance_buffers[frame_index],
			d.unit_box_vb,
			d.unit_box_vert_count,
			d.descriptors_box[frame_index]
		);
		draw_shape(
			d.sphere_instances,
			d.sphere_instance_buffers[frame_index],
			d.unit_sphere_vb,
			d.unit_sphere_vert_count,
			d.descriptors_sphere[frame_index]
		);
		draw_shape(
			d.capsule_instances,
			d.capsule_instance_buffers[frame_index],
			d.unit_capsule_vb,
			d.unit_capsule_vert_count,
			d.descriptors_capsule[frame_index]
		);
	}

	if (has_lines) {
		rec.bind(d.pipeline_lines);
		rec.bind_descriptors(d.pipeline_lines, d.descriptors_lines[frame_index]);
		rec.bind_vertex(d.line_vertex_buffers[frame_index]);
		rec.draw(static_cast<std::uint32_t>(d.line_vertices.size()));
	}
}
