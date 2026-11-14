module gse.graphics:physics_debug_renderer_impl;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.physics;
import gse.save;
import std;

import :camera_latch_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :sky_renderer;
import :world_text_renderer;
import :settings;


namespace gse::renderer::physics_debug {
	struct instanced_bindings {
		shaders::ssbo_readonly<vbd::body_state> body_data;
		shaders::ssbo_readonly<shape_instance> shape_instances;
		shaders::ssbo_readonly<vec4f> shape_vertex_buffer;
	};

	struct lines_bindings {
		shaders::ssbo_readonly<debug_vertex> line_vertex_buffer;
	};

	using line_pass = gpu::specs<
		gpu::vertex_stage<>,
		gpu::fragment_stage<>,
		gpu::rasterization<gpu::polygon_mode::line, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::primitive_topology<gpu::topology::line_list>
	>;

	using instanced_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/physics_debug_instanced">,
		gpu::types<vbd::shader_types>,
		gpu::bindings<shaders::standard_3d::camera, instanced_bindings>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		line_pass
	>;

	using lines_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/physics_debug">,
		gpu::bindings<shaders::standard_3d::camera, lines_bindings>,
		line_pass
	>;

	constexpr vec3f shape_color{ 0.0f, 1.0f, 0.0f };
	constexpr vec3f non_resolving_shape_color{ 0.35f, 0.35f, 0.7f };
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

	auto ensure_vertex_capacity(
		gpu::device& device,
		gpu::buffer& buffer,
		std::size_t& capacity,
		std::size_t required_bytes,
		gpu::buffer_flag usage
	) -> void;

	auto ensure_bindless_buffer_capacity(
		gpu::device& device,
		gpu::buffer& buffer,
		std::size_t& capacity,
		std::size_t required_bytes,
		std::size_t stride,
		gpu::buffer_flag usage,
		std::string_view tag
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
		const length normal_len = std::max<length>(std::min<length>(penetration, meters(0.5f)), min_normal_len);
		const auto normal_end = p + n * normal_len;
		add_line(p, normal_end, normal_color, out_vertices);
	}
}

auto gse::renderer::physics_debug::ensure_vertex_capacity(gpu::device& device, gpu::buffer& buffer, std::size_t& capacity, const std::size_t required_bytes, const gpu::buffer_flag usage) -> void {
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
	buffer = device.create_buffer(
		{
			.size = capacity,
			.usage = usage
		}
	);
}

auto gse::renderer::physics_debug::ensure_bindless_buffer_capacity(gpu::device& device, gpu::buffer& buffer, std::size_t& capacity, const std::size_t required_bytes, const std::size_t stride, const gpu::buffer_flag usage, const std::string_view tag) -> void {
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
	buffer = device.create_buffer(
		{
			.size = capacity,
			.stride = stride,
			.usage = usage,
		},
		tag
	);
}

auto gse::renderer::physics_debug::init(const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.pipeline_instanced, instanced_entry::pod);
		batch.add(d.pipeline_lines, lines_entry::pod);
	}

	{
		const auto box_verts = generate_unit_box();
		const auto sphere_verts = generate_unit_sphere();
		const auto capsule_verts = generate_unit_capsule();

		d.unit_box_vert_count = static_cast<std::uint32_t>(box_verts.size());
		d.unit_sphere_vert_count = static_cast<std::uint32_t>(sphere_verts.size());
		d.unit_capsule_vert_count = static_cast<std::uint32_t>(capsule_verts.size());

		d.unit_box_vb = gpu_s.device->create_buffer<vec4f>(
			box_verts.size(),
			{
				.usage = gpu::buffer_flag::storage,
				.data = box_verts.data(),
			},
			"physics_debug.unit_box"
		);

		d.unit_sphere_vb = gpu_s.device->create_buffer<vec4f>(
			sphere_verts.size(),
			{
				.usage = gpu::buffer_flag::storage,
				.data = sphere_verts.data(),
			},
			"physics_debug.unit_sphere"
		);

		d.unit_capsule_vb = gpu_s.device->create_buffer<vec4f>(
			capsule_verts.size(),
			{
				.usage = gpu::buffer_flag::storage,
				.data = capsule_verts.data(),
			},
			"physics_debug.unit_capsule"
		);
	}

	return {};
}

auto gse::renderer::physics_debug::build(context& ctx, data& d, const shared_view<physics::data> ps, read<physics::transform_component> transforms, read<physics::motion_component> motions, read<physics::collision_component> collisions, read<physics::collision_result_component> results) -> async::task<> {
	d.line_vertices.clear();

	if (!d.enabled) {
		return {};
	}

	d.draw_solver_bodies = physics::gpu_solver_active(ps);
	const auto motion_ids = motions.owner_ids();
	if (!d.draw_solver_bodies) {
		d.cpu_body_staging.resize(motions.size());
		const bool order_matches =
			transforms.size() == motions.size() && std::ranges::equal(motion_ids, transforms.owner_ids());
		for (std::size_t i = 0; i < motions.size(); ++i) {
			const auto* tc = order_matches ? std::addressof(transforms[i]) : transforms.find(motion_ids[i]);
			if (!tc) {
				continue;
			}
			auto& bs = d.cpu_body_staging[i];
			bs.position = tc->position;
			bs.orientation = tc->orientation;
		}
	}

	const auto collision_ids = collisions.owner_ids();
	const bool body_indices_current = d.draw_solver_bodies
		? d.instance_body_index_generation == ps.body_index_generation
		: std::ranges::equal(d.instance_motion_owners, motion_ids);
	const bool instances_current =
		d.instance_solver_indices == d.draw_solver_bodies &&
		d.instance_shape_generation == ps.shape_generation &&
		body_indices_current &&
		std::ranges::equal(d.instance_owners, collision_ids);

	if (!instances_current) {
		d.box_instances.clear();
		d.sphere_instances.clear();
		d.capsule_instances.clear();
		d.instance_body_span = 0;

		std::flat_map<id, std::uint32_t> motion_index;
		if (!d.draw_solver_bodies) {
			std::vector<std::pair<id, std::uint32_t>> entries;
			entries.reserve(motion_ids.size());
			for (std::size_t i = 0; i < motion_ids.size(); ++i) {
				if (transforms.find(motion_ids[i]) != nullptr) {
					entries.emplace_back(motion_ids[i], static_cast<std::uint32_t>(i));
				}
			}
			motion_index.insert(entries.begin(), entries.end());
		}
		const auto& body_index_of = d.draw_solver_bodies ? ps.id_to_body_index : motion_index;

		for (std::size_t i = 0; i < collisions.size(); ++i) {
			const auto idx_it = body_index_of.find(collision_ids[i]);
			if (idx_it == body_index_of.end()) {
				continue;
			}
			const std::uint32_t body_index = idx_it->second;
			d.instance_body_span = std::max(d.instance_body_span, body_index + 1);

			const auto& coll = collisions[i];
			const auto shape_tint = coll.resolve_collisions ? shape_color : non_resolving_shape_color;

			match(coll.shape)
				.if_is([&](const physics::box_shape& s) {
					d.box_instances.push_back(
						shape_instance{
							.body_index = body_index,
							.shape_scale = vec3<length>{ s.size.x() * 0.5f, s.size.y() * 0.5f, s.size.z() * 0.5f },
							.color = shape_tint,
						}
					);
				})
				.else_if_is([&](const physics::sphere_shape& s) {
					d.sphere_instances.push_back(
						shape_instance{
							.body_index = body_index,
							.shape_scale = vec3<length>{ s.radius, s.radius, s.radius },
							.color = shape_tint,
						}
					);
				})
				.else_if_is([&](const physics::capsule_shape& s) {
					d.capsule_instances.push_back(
						shape_instance{
							.body_index = body_index,
							.shape_scale = vec3<length>{ s.radius, s.half_height, s.radius },
							.color = shape_tint,
						}
					);
				});
		}

		d.instance_owners.assign(collision_ids.begin(), collision_ids.end());
		d.instance_motion_owners.assign(motion_ids.begin(), motion_ids.end());
		d.instance_solver_indices = d.draw_solver_bodies;
		d.instance_shape_generation = ps.shape_generation;
		d.instance_body_index_generation = ps.body_index_generation;
		++d.instances_version;
	}

	const auto result_ids = results.owner_ids();
	const bool motion_order_matches =
		motions.size() == results.size() && std::ranges::equal(result_ids, motions.owner_ids());
	for (std::size_t i = 0; i < results.size(); ++i) {
		const auto& res = results[i];
		if (!res.colliding) {
			continue;
		}
		const auto* mc = motion_order_matches ? std::addressof(motions[i]) : motions.find(result_ids[i]);
		if (mc) {
			build_contact_debug(res, *mc, d.line_vertices);
		}
	}

	return {};
}

auto gse::renderer::physics_debug::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<physics::gpu_solver_frame_info> solver_in, shared_view<camera_latch::data> latch_s) -> async::task<> {
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

	const auto ext = gpu_s.render_graph->extent();

	std::uint32_t body_count = 0;
	if (d.draw_solver_bodies) {
		if (const auto& solver_infos = solver_in.of<physics::gpu_solver_frame_info>(); !solver_infos.empty() && solver_infos[0].snapshot) {
			body_count = solver_infos[0].body_count;
		}
	}
	else {
		body_count = static_cast<std::uint32_t>(d.cpu_body_staging.size());
	}

	const bool draw_shapes = has_shape_instances && body_count > 0 && body_count >= d.instance_body_span;

	if (!draw_shapes && !has_lines) {
		co_return;
	}

	if (draw_shapes) {
		if (d.draw_solver_bodies) {
			const auto& info = solver_in.of<physics::gpu_solver_frame_info>()[0];
			if (!d.solver_body_views[frame_index].valid()) {
				d.solver_body_views[frame_index] = gpu_s.device->allocate_buffer_slot();
			}
			gpu_s.device->write_storage_buffer(d.solver_body_views[frame_index].slot(), info.snapshot, info.body_count * info.body_stride);
		}
		else {
			ensure_bindless_buffer_capacity(
				*gpu_s.device,
				d.cpu_body_buffers[frame_index],
				d.cpu_body_capacity[frame_index],
				body_count * sizeof(vbd::body_state),
				sizeof(vbd::body_state),
				gpu::buffer_flag::storage,
				"physics_debug.cpu_body"
			);
			d.cpu_body_buffers[frame_index].host_write(d.cpu_body_staging);
		}
	}

	if (draw_shapes && d.uploaded_instances_version[frame_index] != d.instances_version) {
		d.uploaded_instances_version[frame_index] = d.instances_version;

		const auto upload_instances =
			[&](
				std::vector<shape_instance>& instances,
				gpu::buffer& instance_buffer,
				std::size_t& capacity,
				const std::string_view tag
		) {
				if (instances.empty()) {
					return;
				}
				const auto bytes = instances.size() * sizeof(shape_instance);
				ensure_bindless_buffer_capacity(
					*gpu_s.device,
					instance_buffer,
					capacity,
					bytes,
					sizeof(shape_instance),
					gpu::buffer_flag::storage,
					tag
				);
				instance_buffer.host_write(instances);
			};

		upload_instances(
			d.box_instances,
			d.box_instance_buffers[frame_index],
			d.box_instance_capacity[frame_index],
			"physics_debug.box_instances"
		);
		upload_instances(
			d.sphere_instances,
			d.sphere_instance_buffers[frame_index],
			d.sphere_instance_capacity[frame_index],
			"physics_debug.sphere_instances"
		);
		upload_instances(
			d.capsule_instances,
			d.capsule_instance_buffers[frame_index],
			d.capsule_instance_capacity[frame_index],
			"physics_debug.capsule_instances"
		);
	}

	if (has_lines) {
		const auto bytes = d.line_vertices.size() * sizeof(debug_vertex);
		ensure_bindless_buffer_capacity(
			*gpu_s.device,
			d.line_vertex_buffers[frame_index],
			d.line_vertex_capacity[frame_index],
			bytes,
			sizeof(debug_vertex),
			gpu::buffer_flag::storage,
			"physics_debug.lines"
		);
		d.line_vertex_buffers[frame_index].host_write(d.line_vertices);
	}

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.after<^^camera_latch::frame, ^^forward::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^sky::frame, ^^vbd::vbd_render_mirror_stage>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (draw_shapes) {
		rec.bind(d.pipeline_instanced);

		const auto body_slot = d.draw_solver_bodies ? d.solver_body_views[frame_index].slot() : d.cpu_body_buffers[frame_index].slot();

		const auto draw_shape = [&](
			const std::vector<shape_instance>& instances,
			const gpu::buffer& instance_buffer,
			const gpu::buffer& unit_vb,
			std::uint32_t unit_vert_count
		) {
			if (instances.empty()) {
				return;
			}
			const auto count = static_cast<std::uint32_t>(instances.size());
			rec.push_bindings<instanced_entry>({
				.camera_ubo = latch_s.camera_buffers[frame_index],
				.body_data = body_slot,
				.shape_instances = instance_buffer,
				.shape_vertex_buffer = unit_vb,
			});
			rec.draw(unit_vert_count, count);
		};

		draw_shape(d.box_instances, d.box_instance_buffers[frame_index], d.unit_box_vb, d.unit_box_vert_count);
		draw_shape(d.sphere_instances,
			d.sphere_instance_buffers[frame_index],
			d.unit_sphere_vb,
			d.unit_sphere_vert_count);
		draw_shape(
			d.capsule_instances,
			d.capsule_instance_buffers[frame_index],
			d.unit_capsule_vb,
			d.unit_capsule_vert_count
		);
	}

	if (has_lines) {
		rec.bind(d.pipeline_lines);
		rec.push_bindings<lines_entry>({
			.camera_ubo = latch_s.camera_buffers[frame_index],
			.line_vertex_buffer = d.line_vertex_buffers[frame_index],
		});
		rec.draw(static_cast<std::uint32_t>(d.line_vertices.size()));
	}
}