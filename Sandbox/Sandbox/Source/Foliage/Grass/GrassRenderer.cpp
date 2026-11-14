module sandbox:grass_renderer_impl;

import std;

import gse;
import gse.gpu;
import gse.gpu_record;

import :grass_field;
import :grass_renderer;
import :grass_system;

namespace sandbox::grass::render {
	using namespace gse;
	using namespace gse::renderer;

	using cull_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_cull">,
		gpu::types<shader_types, shaders::raster_shadow::shader_types>,
		gpu::bindings<cull_bindings>,
		gpu::helpers<"Grass/grass_shape">,
		gpu::threads<64>,
		gpu::push_constant<cull_push_constants>
	>;

	struct [[= shaders::shader_struct]] depth_push_constants {
		std::uint32_t view;
	};

	using depth_entry = gpu::graphics_entry<
		gpu::body_path<"Grass/grass_depth">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shader_types>,
		gpu::bindings<shaders::standard_3d::camera, shaders::meshlet::instances, draw_bindings>,
		gpu::helpers<"Standard3D/depth_velocity", "Grass/grass_shape", "Grass/grass_geometry">,
		gpu::vertex_stage<>,
		gpu::fragment_stage<>,
		gpu::push_constant<depth_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<true, true, gpu::compare_op::greater>,
		gpu::color_targets<gpu::color_format::hdr>
	>;

	struct [[= shaders::shader_struct]] shadow_push_constants {
		std::uint32_t tile_index;
	};

	using shadow_entry = gpu::graphics_entry<
		gpu::body_path<"Grass/grass_shadow">,
		gpu::types<shaders::common::shader_types, shaders::raster_shadow::shader_types, shader_types>,
		gpu::bindings<raster_shadow::tile_bindings, shaders::meshlet::instances, draw_bindings>,
		gpu::helpers<"Grass/grass_shape", "Grass/grass_geometry">,
		gpu::vertex_stage<>,
		gpu::fragment_stage<>,
		gpu::push_constant<shadow_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<true, true, gpu::compare_op::greater_or_equal>,
		gpu::color_targets<>
	>;

	template <typename TechniqueBindings, fixed_string TechniqueHelper>
	using forward_entry = gpu::graphics_entry<
		gpu::body_path<"Grass/grass_forward">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types, shader_types>,
		forward::forward_bindings<TechniqueBindings, shaders::meshlet::instances, draw_bindings>,
		forward::surface_helpers<TechniqueHelper, "Grass/grass_material", "Grass/grass_shape", "Grass/grass_geometry">,
		gpu::vertex_stage<>,
		gpu::fragment_stage<>,
		gpu::push_constant<forward::meshlet_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::greater_or_equal>
	>;

	using ray_traced_entry = forward_entry<forward::ray_traced_bindings, "Standard3D/lighting_ray_traced">;

	using raster_entry = forward_entry<forward::raster_bindings, "Standard3D/lighting_raster">;

	struct rt_mesh_bindings {
		shaders::ssbo_readonly<grass_params> grass_params_in;
		shaders::ssbo_readonly<grass_node> grass_pool;
		shaders::ssbo_readonly<grass_slot> rt_slots;
		shaders::ssbo_readwrite<vec4f> rt_vertices_out;
	};

	struct rt_instance_bindings {
		shaders::ssbo_readonly<grass_params> grass_params_in;
		shaders::ssbo_readonly<grass_tile> grass_tiles;
		shaders::ssbo_readonly<vec2u> rt_static_blas;
		shaders::ssbo_readonly<vec2u> rt_dynamic_blas;
		shaders::rw_byte_address_buffer tlas_instances;
	};

	struct [[= shaders::shader_struct]] rt_mesh_push_constants {
		std::uint32_t first_tile;
		std::uint32_t dynamic;
	};

	struct [[= shaders::shader_struct]] rt_instance_push_constants {
		std::uint32_t first_instance;
		std::uint32_t material_index;
	};

	using rt_mesh_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tile_mesh">,
		gpu::types<shader_types>,
		gpu::bindings<rt_mesh_bindings>,
		gpu::helpers<"Grass/grass_shape", "Grass/grass_geometry">,
		gpu::threads<layout.grass_sim_threads>,
		gpu::push_constant<rt_mesh_push_constants>
	>;

	using rt_instance_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tlas_instances">,
		gpu::types<shader_types, type_pack<grass_rt_layout>>,
		gpu::bindings<rt_instance_bindings>,
		gpu::helpers<"Grass/grass_shape">,
		gpu::threads<64>,
		gpu::push_constant<rt_instance_push_constants>
	>;

	constexpr std::uint32_t rt_tile_vertices = layout.grass_tile_blade_count * layout.grass_side_vertices;
	constexpr std::uint32_t rt_tile_primitives = index_count() / 3;
	constexpr std::size_t rt_tile_vertex_bytes = static_cast<std::size_t>(rt_tile_vertices) * sizeof(vec4f);
	constexpr std::size_t rt_slot_bytes = static_cast<std::size_t>(layout.grass_slot_count) * sizeof(grass_slot);
	constexpr std::size_t mebibyte = 1024 * 1024;

	constexpr gpu::build_acceleration_structure_flags rebuilt_blas_flags{ gpu::build_acceleration_structure_flag::prefer_fast_build };
	constexpr gpu::build_acceleration_structure_flags refit_blas_flags{ gpu::build_acceleration_structure_flag::prefer_fast_build, gpu::build_acceleration_structure_flag::allow_update };

	struct rt_work {
		const field_buffers* buffers = nullptr;
		rt_field* field = nullptr;
		std::uint32_t first_instance = 0;
		std::uint32_t static_first = 0;
		std::uint32_t static_rows = 0;
	};

	const id material_owner = find_or_generate_id("sandbox.grass.material");

	constexpr shaders::forward::material_data material{
		.base_color = vec3f(0.24f, 0.46f, 0.12f),
		.roughness = 0.8f,
		.metallic = 0.f,
		.diffuse_index = shaders::bindless::invalid_index,
	};

	auto build_tile_indices() -> std::vector<std::uint32_t>;

	auto create_field_render(
		gpu::device& device,
		std::uint32_t tile_count,
		field_render& fr
	) -> void;

	auto args_offset(
		std::uint32_t view
	) -> std::size_t;

	auto record_cull(
		gpu::recording_context& rec,
		const field_render& field,
		std::uint32_t frame_index,
		const cull_push_constants& pc,
		std::uint32_t view_count,
		gpu::bindless_slot shadow_tiles
	) -> void;

	auto record_draw(
		gpu::recording_context& rec,
		const gpu::buffer& indices,
		const field_render& field,
		std::uint32_t view
	) -> void;

	auto record_shadow_tiles(
		gpu::recording_context& rec,
		const data& d,
		std::uint32_t frame_index,
		std::span<const raster_shadow::tile_request> requests,
		std::uint32_t first_tile,
		gpu::bindless_slot tiles_slot
	) -> void;

	auto record_shadows(
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<raster_shadow::data> shadows,
		const view_projection_matrix& camera_view_projection,
		std::uint32_t frame_index,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;

	auto record_forward(
		shared_view<gpu::context::data> gpu_s,
		const data& d,
		forward::surface_frame surface,
		std::uint32_t frame_index,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;

	auto rt_tile_geometry(
		const gpu::buffer& vertices,
		std::uint32_t row,
		const gpu::buffer& indices
	) -> gpu::acceleration_structure_geometry;

	auto record_rt_blas(
		gpu::recording_context& rec,
		gpu::acceleration_structure dst,
		const gpu::acceleration_structure_geometry& geometry,
		gpu::build_acceleration_structure_flags flags,
		gpu::build_acceleration_structure_mode mode,
		gpu::device_address scratch
	) -> void;

	auto rt_vertex_buffer(
		gpu::device& device,
		std::uint32_t rows,
		std::string_view tag
	) -> gpu::buffer;

	auto rt_address_buffer(
		gpu::device& device,
		std::uint32_t count,
		std::string_view tag
	) -> gpu::buffer;

	auto create_rt_field(
		shared_view<gpu::context::data> gpu_s,
		data& d,
		const field_buffers& fb,
		rt_field& rf
	) -> void;

	auto prepare_rt_field(
		gpu::device& device,
		std::uint32_t frame_index,
		const gpu::buffer& indices,
		rt_field& rf
	) -> void;

	auto record_rt(
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<grass::data> grass_s,
		shared_view<scene_acceleration::data> rt,
		std::uint32_t material_index,
		std::uint32_t frame_index,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}

auto sandbox::grass::render::build_tile_indices() -> std::vector<std::uint32_t> {
	std::vector<std::uint32_t> indices;
	indices.reserve(index_count());
	for (std::uint32_t blade = 0; blade < layout.grass_tile_blade_count; ++blade) {
		const auto base = blade * layout.grass_side_vertices;
		const auto left = [base](const std::uint32_t s) {
			return base + 2 * s;
		};
		const auto right = [base](const std::uint32_t s) {
			return base + 2 * s + 1;
		};
		for (std::uint32_t s = 0; s + 1 < layout.grass_segments; ++s) {
			indices.insert(indices.end(), { left(s), right(s), right(s + 1), left(s), right(s + 1), left(s + 1) });
		}
		const auto last = layout.grass_segments - 1;
		indices.insert(indices.end(), { left(last), right(last), base + layout.grass_side_vertices - 1 });
	}
	return indices;
}

auto sandbox::grass::render::create_field_render(gpu::device& device, const std::uint32_t tile_count, field_render& fr) -> void {
	fr.tile_count = tile_count;
	fr.visible = device.create_buffer(
		{
			.size = static_cast<std::size_t>(views.grass_max_views) * tile_count * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.writable = true,
			.device_local = true,
		},
		"grass.visible"
	);
	fr.args = device.create_buffer(
		{
			.size = static_cast<std::size_t>(views.grass_max_views) * views.grass_args_words * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::indirect, gpu::buffer_flag::transfer_dst },
			.writable = true,
			.device_local = true,
		},
		"grass.draw_args"
	);
	for (auto& instance : fr.instance) {
		instance = device.create_buffer(
			{
				.size = sizeof(shaders::common::instance_data),
				.stride = sizeof(shaders::common::instance_data),
				.usage = gpu::buffer_flag::storage,
			},
			"grass.instance"
		);
	}
	fr.previous_model.reset();
	log::println(log::category::render, "Grass renderer: {} tiles, visible lists {} KiB", tile_count, fr.visible.size() / 1024);
}

auto sandbox::grass::render::args_offset(const std::uint32_t view) -> std::size_t {
	return static_cast<std::size_t>(view) * views.grass_args_words * sizeof(std::uint32_t);
}

auto sandbox::grass::render::record_cull(gpu::recording_context& rec, const field_render& field, const std::uint32_t frame_index, const cull_push_constants& pc, const std::uint32_t view_count, const gpu::bindless_slot shadow_tiles) -> void {
	rec.fill_buffer(field.args, args_offset(pc.first_view), args_offset(view_count));
	rec.dispatch<cull_entry>(
		pc,
		{
			.grass_params_in = field.params[frame_index],
			.cull_visible_out = field.visible,
			.cull_args_out = field.args,
			.cull_shadow_tiles = shadow_tiles,
		},
		vec3u{ (field.tile_count + 63) / 64, view_count, 1u }
	);
}

auto sandbox::grass::render::record_draw(gpu::recording_context& rec, const gpu::buffer& indices, const field_render& field, const std::uint32_t view) -> void {
	rec.bind_index(indices);
	rec.draw_indirect(field.args, args_offset(view), 1, views.grass_args_words * sizeof(std::uint32_t));
}

auto sandbox::grass::render::record_shadow_tiles(gpu::recording_context& rec, const data& d, const std::uint32_t frame_index, const std::span<const raster_shadow::tile_request> requests, const std::uint32_t first_tile, const gpu::bindless_slot tiles_slot) -> void {
	for (std::size_t t = 0; t < requests.size(); ++t) {
		raster_shadow::set_tile_viewport(rec, requests[t]);
		const auto tile_index = first_tile + static_cast<std::uint32_t>(t);
		for (const auto& field : d.fields | std::views::values) {
			if (!field.culled) {
				continue;
			}
			rec.push_bindings<shadow_entry>(
				{
					.tile_index = tile_index,
				},
				{
					.tiles = tiles_slot,
					.instance_data_buffer = field.instance[frame_index],
					.grass_params_in = field.params[frame_index],
					.grass_pool = field.pool,
					.grass_tiles = field.tiles,
					.grass_visible = field.visible,
				}
			);
			record_draw(rec, d.indices, field, views.grass_first_shadow_view + tile_index);
		}
	}
}

auto sandbox::grass::render::record_shadows(const shared_view<gpu::context::data> gpu_s, data& d, const shared_view<raster_shadow::data> shadows, const view_projection_matrix& camera_view_projection, const std::uint32_t frame_index, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	const auto requests = shadows.requests.span();
	const auto tile_count = static_cast<std::uint32_t>(requests.size());
	const auto tiles_slot = shadows.tile_buffers[frame_index].slot();

	{
		auto rec = co_await gpu::pass<^^shadow_cull_pass>(pass_out)
			.pipeline(d.cull_pipeline)
			.after<^^raster_shadow::setup_pass, ^^cull_pass>();
		const cull_push_constants pc{
			.camera_view_projection = camera_view_projection,
			.first_view = views.grass_first_shadow_view,
			.shadow_views = 1,
		};
		for (const auto& field : d.fields | std::views::values) {
			if (field.culled) {
				record_cull(rec, field, frame_index, pc, tile_count, tiles_slot);
			}
		}
	}

	{
		auto rec = co_await gpu::pass<^^cascade_pass>(pass_out)
			.pipeline(d.shadow_pipeline)
			.depth(gpu::load_depth(shadows.cascade_atlas))
			.extends<^^raster_shadow::cascade_pass>()
			.after<^^shadow_cull_pass>();
		record_shadow_tiles(rec, d, frame_index, requests.first(raster_shadow::cascade_count), 0, tiles_slot);
	}

	auto rec = co_await gpu::pass<^^local_pass>(pass_out)
		.pipeline(d.shadow_pipeline)
		.depth(gpu::load_depth(shadows.local_atlas))
		.extends<^^raster_shadow::frame>()
		.after<^^shadow_cull_pass>();
	record_shadow_tiles(rec, d, frame_index, requests.subspan(raster_shadow::cascade_count), raster_shadow::cascade_count, tiles_slot);
}

auto sandbox::grass::render::record_forward(const shared_view<gpu::context::data> gpu_s, const data& d, const forward::surface_frame surface, const std::uint32_t frame_index, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	const auto ext = gpu_s.render_graph->extent();
	auto rec = co_await gpu::pass<^^forward_pass>(pass_out)
		.pipeline(d.forward_pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.extends<^^forward::frame>()
		.after<^^cull_pass>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	const auto draw = [&]<typename Entry>() {
		for (const auto& field : d.fields | std::views::values) {
			if (!field.culled) {
				continue;
			}
			gpu::entry_binding_args<Entry> bindings{
				.instance_data_buffer = field.instance[frame_index],
				.grass_params_in = field.params[frame_index],
				.grass_pool = field.pool,
				.grass_tiles = field.tiles,
				.grass_visible = field.visible,
			};
			forward::bind_surface(bindings, surface);
			rec.push_bindings<Entry>(surface.push_constants, bindings);
			record_draw(rec, d.indices, field, views.grass_camera_view);
		}
	};

	if (d.ray_traced) {
		draw.template operator()<ray_traced_entry>();
	}
	else {
		draw.template operator()<raster_entry>();
	}
}

auto sandbox::grass::render::rt_tile_geometry(const gpu::buffer& vertices, const std::uint32_t row, const gpu::buffer& indices) -> gpu::acceleration_structure_geometry {
	auto geometry = gpu::make_blas_geometry({
		.vertex_buffer = &vertices,
		.vertex_count = rt_tile_vertices,
		.vertex_stride = static_cast<std::uint32_t>(sizeof(vec4f)),
		.index_buffer = &indices,
		.index_count = index_count(),
	});
	geometry.triangles.vertex_data += static_cast<gpu::device_address>(row) * rt_tile_vertex_bytes;
	return geometry;
}

auto sandbox::grass::render::record_rt_blas(gpu::recording_context& rec, const gpu::acceleration_structure dst, const gpu::acceleration_structure_geometry& geometry, const gpu::build_acceleration_structure_flags flags, const gpu::build_acceleration_structure_mode mode, const gpu::device_address scratch) -> void {
	const gpu::acceleration_structure_build_range_info range{
		.primitive_count = rt_tile_primitives,
	};
	const gpu::acceleration_structure_build_range_info* range_ptr = &range;
	rec.build_acceleration_structure(
		{
			.type = gpu::acceleration_structure_type::bottom_level,
			.flags = flags,
			.mode = mode,
			.src = mode == gpu::build_acceleration_structure_mode::update ? dst : gpu::acceleration_structure{},
			.dst = dst,
			.geometries = std::span(&geometry, 1),
			.scratch_address = scratch,
		},
		{ &range_ptr, 1 }
	);
}

auto sandbox::grass::render::rt_vertex_buffer(gpu::device& device, const std::uint32_t rows, const std::string_view tag) -> gpu::buffer {
	return device.create_buffer(
		{
			.size = static_cast<std::size_t>(rows) * rt_tile_vertex_bytes,
			.stride = sizeof(vec4f),
			.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::acceleration_structure_build_input },
			.writable = true,
			.device_local = true,
		},
		tag
	);
}

auto sandbox::grass::render::rt_address_buffer(gpu::device& device, const std::uint32_t count, const std::string_view tag) -> gpu::buffer {
	auto buffer = device.create_buffer(
		{
			.size = static_cast<std::size_t>(count) * sizeof(gpu::device_address),
			.stride = sizeof(vec2u),
			.usage = gpu::buffer_flag::storage,
		},
		tag
	);
	buffer.host_zero();
	return buffer;
}

auto sandbox::grass::render::create_rt_field(const shared_view<gpu::context::data> gpu_s, data& d, const field_buffers& fb, rt_field& rf) -> void {
	auto& device = *gpu_s.device;
	rf.field = fb.field;
	rf.tile_count = fb.tile_count;
	rf.static_built = 0;
	rf.occupied.clear();
	rf.slot_generation = 0;
	rf.slot_applied = 0;
	rf.slot_readback = gpu_s.render_graph->create_readback_channel(rt_slot_bytes, "grass.rt.slots");

	const auto batch_rows = std::min(rt_static_batch, rf.tile_count);
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		rf.static_published[i] = 0;
		rf.static_addresses[i] = rt_address_buffer(device, rf.tile_count, "grass.rt.static_addresses");
		rf.static_vertices[i] = rt_vertex_buffer(device, batch_rows, "grass.rt.static_vertices");
		rf.dynamic_blas[i].clear();
		rf.dynamic_built_tile[i].clear();
		rf.dynamic_vertices[i] = gpu::buffer{};
		rf.dynamic_capacity[i] = 0;
		rf.dynamic_addresses[i] = rt_address_buffer(device, layout.grass_slot_count, "grass.rt.dynamic_addresses");
	}

	const auto geometry = rt_tile_geometry(rf.static_vertices[0], 0, d.indices);
	const auto sizes = device.query_blas_build_sizes(geometry, rt_tile_primitives, rebuilt_blas_flags);
	const auto refit_sizes = device.query_blas_build_sizes(geometry, rt_tile_primitives, refit_blas_flags);
	d.rt_tile_blas_bytes = sizes.acceleration_structure_size;
	d.rt_tile_scratch = std::max({ sizes.build_scratch_size, refit_sizes.build_scratch_size, refit_sizes.update_scratch_size });

	const auto static_bytes = d.rt_tile_blas_bytes * rf.tile_count;
	rf.over_budget = static_bytes > rt_static_budget;
	log::println(
		log::category::render,
		"Grass RT: {} tiles x {} KiB BLAS ({} triangles each) = {} MiB static; build scratch {} KiB; batch vertices {} MiB x {}",
		rf.tile_count,
		d.rt_tile_blas_bytes / 1024,
		rt_tile_primitives,
		static_bytes / mebibyte,
		d.rt_tile_scratch / 1024,
		rf.static_vertices[0].size() / mebibyte,
		per_frame_resource<gpu::buffer>::frames_in_flight
	);

	rf.static_blas.clear();
	if (rf.over_budget) {
		log::println(log::level::warning, log::category::render, "Grass RT: static BLAS {} MiB exceeds the {} MiB budget; resting tiles cast no RT shadow", static_bytes / mebibyte, rt_static_budget / mebibyte);
		for (auto& vertices : rf.static_vertices) {
			vertices = gpu::buffer{};
		}
		return;
	}

	rf.static_blas.reserve(rf.tile_count);
	for (std::uint32_t tile = 0; tile < rf.tile_count; ++tile) {
		rf.static_blas.push_back(device.create_blas(geometry, rt_tile_primitives, rebuilt_blas_flags));
	}
}

auto sandbox::grass::render::prepare_rt_field(gpu::device& device, const std::uint32_t frame_index, const gpu::buffer& indices, rt_field& rf) -> void {
	if (const auto view = rf.slot_readback.latest(); view.generation > rf.slot_applied && view.bytes.size() >= rt_slot_bytes) {
		rf.slot_applied = view.generation;
		rf.occupied.clear();
		for (std::uint32_t slot = 0; slot < layout.grass_slot_count; ++slot) {
			grass_slot entry;
			std::memcpy(&entry, view.bytes.data() + slot * sizeof(entry), sizeof(entry));
			if (entry.tile_plus_one != 0) {
				rf.occupied.push_back({
					.slot = slot,
					.tile_plus_one = entry.tile_plus_one,
				});
			}
		}
	}

	const auto needed = rf.occupied.empty() ? 0u : rf.occupied.back().slot + 1;
	auto& capacity = rf.dynamic_capacity[frame_index];
	if (capacity < needed) {
		capacity = std::min((needed + rt_dynamic_growth - 1) / rt_dynamic_growth * rt_dynamic_growth, layout.grass_slot_count);
		rf.dynamic_vertices[frame_index] = rt_vertex_buffer(device, capacity, "grass.rt.dynamic_vertices");
		log::println(log::category::render, "Grass RT: dynamic vertices grown to {} slots ({} MiB) for frame {}", capacity, rf.dynamic_vertices[frame_index].size() / mebibyte, frame_index);
	}

	auto& blases = rf.dynamic_blas[frame_index];
	if (blases.size() < capacity) {
		blases.resize(capacity);
		rf.dynamic_built_tile[frame_index].resize(capacity, 0);
	}
	for (const auto& occupied : rf.occupied) {
		if (!blases[occupied.slot].valid()) {
			blases[occupied.slot] = device.create_blas(rt_tile_geometry(rf.dynamic_vertices[frame_index], occupied.slot, indices), rt_tile_primitives, refit_blas_flags);
		}
	}
}

auto sandbox::grass::render::record_rt(const shared_view<gpu::context::data> gpu_s, data& d, const shared_view<grass::data> grass_s, const shared_view<scene_acceleration::data> rt, const std::uint32_t material_index, const std::uint32_t frame_index, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	std::erase_if(d.rt_fields, [&](const auto& entry) {
		return !grass_s.buffers.contains(entry.first);
	});

	const auto range = rt.contributed.find(gpu::pass_id<^^rt_instances_pass>());
	if (range == rt.contributed.end()) {
		co_return;
	}

	std::vector<rt_work> work;
	std::uint32_t instances = 0;
	std::uint32_t builds = 0;
	for (const auto& [owner, fb] : grass_s.buffers) {
		const auto rendered = d.fields.find(owner);
		if (rendered == d.fields.end() || !rendered->second.culled) {
			continue;
		}
		if (instances + fb.tile_count > range->second.capacity) {
			continue;
		}

		auto found = d.rt_fields.find(owner);
		if (found == d.rt_fields.end() || found->second.tile_count != fb.tile_count || rest_shape_changed(found->second.field, fb.field)) {
			d.rt_fields.erase(owner);
			found = d.rt_fields.try_emplace(owner).first;
			create_rt_field(gpu_s, d, fb, found->second);
		}
		auto& rf = found->second;
		prepare_rt_field(*gpu_s.device, frame_index, d.indices, rf);

		const auto static_rows = rf.over_budget ? 0u : std::min(rt_static_batch, rf.tile_count - rf.static_built);
		builds += static_rows + static_cast<std::uint32_t>(rf.occupied.size());
		work.push_back({
			.buffers = &fb,
			.field = &rf,
			.first_instance = range->second.first + instances,
			.static_first = rf.static_built,
			.static_rows = static_rows,
		});
		instances += fb.tile_count;
	}

	if (work.empty()) {
		co_return;
	}

	const auto scratch_alignment = gpu_s.device->acceleration_structure_scratch_alignment();
	const auto scratch_stride = (d.rt_tile_scratch + scratch_alignment - 1) / scratch_alignment * scratch_alignment;
	if (builds != 0) {
		const auto required = scratch_stride * builds + scratch_alignment;
		if (d.rt_scratch[frame_index].size() < required) {
			d.rt_scratch[frame_index] = gpu_s.device->create_buffer(
				{
					.size = required,
					.usage = gpu::buffer_flag::acceleration_structure_scratch,
				},
				"grass.rt.scratch"
			);
			log::println(log::category::render, "Grass RT: build scratch grown to {} KiB for {} concurrent builds (frame {})", required / 1024, builds, frame_index);
		}
	}

	for (const auto& item : work) {
		auto& rf = *item.field;
		const auto built = rf.static_built + item.static_rows;
		if (rf.static_published[frame_index] < built) {
			linear_vector<gpu::device_address> addresses;
			addresses.reserve(built - rf.static_published[frame_index]);
			for (auto tile = rf.static_published[frame_index]; tile < built; ++tile) {
				addresses.push_back(rf.static_blas[tile].device_address());
			}
			rf.static_addresses[frame_index].host_write(addresses.data(), addresses.size() * sizeof(gpu::device_address), rf.static_published[frame_index] * sizeof(gpu::device_address));
			rf.static_published[frame_index] = built;
		}
		if (rf.static_built == rf.tile_count && rf.static_published[frame_index] == rf.tile_count && rf.static_vertices[frame_index].valid()) {
			rf.static_vertices[frame_index] = gpu::buffer{};
		}

		std::array<gpu::device_address, layout.grass_slot_count> dynamic{};
		const auto& blases = rf.dynamic_blas[frame_index];
		for (const auto& occupied : rf.occupied) {
			dynamic[occupied.slot] = blases[occupied.slot].device_address();
		}
		rf.dynamic_addresses[frame_index].host_write(dynamic.data(), sizeof(dynamic));
	}

	{
		auto rec = co_await gpu::pass<^^rt_mesh_pass>(pass_out)
			.pipeline(d.rt_mesh_pipeline)
			.after<^^cull_pass, ^^simulate_pass>();
		constexpr std::uint32_t groups_x = (rt_tile_vertices + layout.grass_sim_threads - 1) / layout.grass_sim_threads;
		for (const auto& item : work) {
			const auto& fb = *item.buffers;
			auto& rf = *item.field;
			if (item.static_rows != 0) {
				rec.dispatch<rt_mesh_entry>(
					{
						.first_tile = item.static_first,
						.dynamic = 0,
					},
					{
						.grass_params_in = fb.params[frame_index],
						.grass_pool = fb.nodes,
						.rt_slots = fb.slots,
						.rt_vertices_out = rf.static_vertices[frame_index],
					},
					vec3u{ groups_x, item.static_rows, 1u }
				);
			}
			if (const auto capacity = rf.dynamic_capacity[frame_index]; capacity != 0) {
				rec.dispatch<rt_mesh_entry>(
					{
						.first_tile = 0,
						.dynamic = 1,
					},
					{
						.grass_params_in = fb.params[frame_index],
						.grass_pool = fb.nodes,
						.rt_slots = fb.slots,
						.rt_vertices_out = rf.dynamic_vertices[frame_index],
					},
					vec3u{ groups_x, capacity, 1u }
				);
			}
			rec.copy_buffer(fb.slots, rf.slot_readback.publish_target(++rf.slot_generation, rt_slot_bytes), rt_slot_bytes);
		}
	}

	auto rec = co_await gpu::pass<^^rt_instances_pass>(pass_out)
		.pipeline(d.rt_instance_pipeline)
		.extends<^^scene_acceleration::instances_pass>()
		.after<^^rt_mesh_pass>();

	if (builds != 0) {
		const auto scratch_base = (d.rt_scratch[frame_index].device_address() + scratch_alignment - 1) / scratch_alignment * scratch_alignment;
		std::uint32_t build_index = 0;
		const auto next_scratch = [&] {
			return scratch_base + scratch_stride * build_index++;
		};
		for (const auto& item : work) {
			auto& rf = *item.field;
			for (std::uint32_t row = 0; row < item.static_rows; ++row) {
				record_rt_blas(rec, rf.static_blas[item.static_first + row].handle(), rt_tile_geometry(rf.static_vertices[frame_index], row, d.indices), rebuilt_blas_flags, gpu::build_acceleration_structure_mode::build, next_scratch());
			}
			rf.static_built += item.static_rows;
			if (item.static_rows != 0 && rf.static_built == rf.tile_count) {
				log::println(log::category::render, "Grass RT: all {} static tile BLASes built", rf.tile_count);
			}
			const auto& blases = rf.dynamic_blas[frame_index];
			auto& built_tile = rf.dynamic_built_tile[frame_index];
			for (const auto& occupied : rf.occupied) {
				const auto mode = built_tile[occupied.slot] == occupied.tile_plus_one ? gpu::build_acceleration_structure_mode::update : gpu::build_acceleration_structure_mode::build;
				record_rt_blas(rec, blases[occupied.slot].handle(), rt_tile_geometry(rf.dynamic_vertices[frame_index], occupied.slot, d.indices), refit_blas_flags, mode, next_scratch());
				built_tile[occupied.slot] = occupied.tile_plus_one;
			}
		}
	}

	for (const auto& item : work) {
		const auto& fb = *item.buffers;
		const auto& rf = *item.field;
		rec.dispatch<rt_instance_entry>(
			{
				.first_instance = item.first_instance,
				.material_index = material_index,
			},
			{
				.grass_params_in = fb.params[frame_index],
				.grass_tiles = fb.tiles,
				.rt_static_blas = rf.static_addresses[frame_index],
				.rt_dynamic_blas = rf.dynamic_addresses[frame_index],
				.tlas_instances = rt.tlas_instance_views[frame_index],
			},
			vec3u{ (rf.tile_count + 63) / 64, 1u, 1u }
		);
	}
}

auto sandbox::grass::render::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<forward::data> forward_s, data& d) -> async::task<> {
	d.ray_traced = forward_s.ray_traced;
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.cull_pipeline, cull_entry::pod);
		batch.add(d.depth_pipeline, depth_entry::pod);
		batch.add(d.shadow_pipeline, shadow_entry::pod);
		batch.add(d.forward_pipeline, d.ray_traced ? ray_traced_entry::pod : raster_entry::pod);
		if (d.ray_traced) {
			batch.add(d.rt_mesh_pipeline, rt_mesh_entry::pod);
			batch.add(d.rt_instance_pipeline, rt_instance_entry::pod);
		}
	}

	const auto indices = build_tile_indices();
	const auto bytes = indices.size() * sizeof(std::uint32_t);
	d.index_upload = gpu_s.device->create_buffer(
		{
			.size = bytes,
			.stride = sizeof(std::uint32_t),
			.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::transfer_src },
			.data = indices.data(),
		},
		"grass.index_upload"
	);
	d.indices = gpu_s.device->create_buffer(
		{
			.size = bytes,
			.stride = sizeof(std::uint32_t),
			.usage = { gpu::buffer_flag::index, gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst, gpu::buffer_flag::acceleration_structure_build_input },
			.device_local = true,
		},
		"grass.indices"
	);
	return {};
}

auto sandbox::grass::render::frame(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d, const shared_view<grass::data> grass_s, const shared_view<camera::data> cam_state, const shared_view<camera_latch::data> latch_s, const shared_view<forward::data> forward_s, const shared_view<geometry_collector::data> gc_s, const std::optional<shared_view<raster_shadow::data>> shadow_s, const std::optional<shared_view<scene_acceleration::data>> rt_s, const channel_write<gpu::render_pass_request> pass_out, const channel_write<scene_acceleration::instance_reservation, geometry_collector::material_reservation> reservations_out) -> async::task<> {
	reservations_out.push<geometry_collector::material_reservation>({
		.owner = material_owner,
		.material = material,
	});

	if (rt_s) {
		std::uint32_t wanted = 0;
		for (const auto& fb : grass_s.buffers | std::views::values) {
			if (wanted + fb.tile_count <= scene_acceleration::contributed_instance_capacity) {
				wanted += fb.tile_count;
			}
		}
		if (wanted != d.reserved_instances) {
			d.reserved_instances = wanted;
			reservations_out.push<scene_acceleration::instance_reservation>({
				.owner = gpu::pass_id<^^rt_instances_pass>(),
				.capacity = wanted,
			});
		}
	}

	std::erase_if(d.fields, [&](const auto& entry) {
		return !grass_s.buffers.contains(entry.first);
	});
	for (auto& fr : d.fields | std::views::values) {
		fr.culled = false;
	}

	const auto material_index = gc_s.reserved_material_indices.find(material_owner);
	if (grass_s.buffers.empty() || !forward_s.surface || material_index == gc_s.reserved_material_indices.end() || !gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	for (const auto& [owner, fb] : grass_s.buffers) {
		if (fb.tile_count == 0) {
			continue;
		}

		auto& fr = d.fields[owner];
		if (fr.tile_count != fb.tile_count) {
			create_field_render(*gpu_s.device, fb.tile_count, fr);
		}

		const spatial_matrix model(physics::transformation_matrix(fb.transform));
		fr.instance[frame_index].host_write(shaders::common::instance_data{
			.model_matrix = model,
			.normal_matrix = spatial_matrix(mat4f(mat3_cast(fb.transform.orientation))),
			.prev_model_matrix = fr.previous_model.value_or(model),
			.material_index = material_index->second,
			.tint = vec4f(1.f),
		});
		fr.previous_model = model;
		fr.pool = fb.nodes.slot();
		fr.tiles = fb.tiles.slot();
		fr.params[frame_index] = fb.params[frame_index].slot();
		fr.culled = true;
	}

	if (std::ranges::none_of(d.fields | std::views::values, &field_render::culled)) {
		co_return;
	}

	const auto camera_view_projection = cam_state.projection_matrix * cam_state.view_matrix;

	{
		auto rec = co_await gpu::pass<^^cull_pass>(pass_out)
			.pipeline(d.cull_pipeline)
			.after<^^grid_pass, ^^simulate_pass>();

		if (!d.indices_uploaded) {
			rec.copy_buffer(d.index_upload, d.indices, d.indices.size());
			d.indices_uploaded = true;
		}

		const cull_push_constants pc{
			.camera_view_projection = camera_view_projection,
			.first_view = views.grass_camera_view,
			.shadow_views = 0,
		};
		for (const auto& fr : d.fields | std::views::values) {
			if (fr.culled) {
				record_cull(rec, fr, frame_index, pc, 1, fr.params[frame_index]);
			}
		}
	}

	{
		const auto ext = gpu_s.render_graph->extent();
		auto rec = co_await gpu::pass<^^depth_pass>(pass_out)
			.pipeline(d.depth_pipeline)
			.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::velocity>()))
			.depth(gpu::load_depth())
			.extends<^^depth_prepass::frame>()
			.after<^^cull_pass>();
		rec.set_viewport(ext);
		rec.set_scissor(ext);

		const auto camera_slot = latch_s.camera_buffers[frame_index].slot();
		for (const auto& field : d.fields | std::views::values) {
			if (!field.culled) {
				continue;
			}
			rec.push_bindings<depth_entry>(
				{
					.view = views.grass_camera_view,
				},
				{
					.camera_ubo = camera_slot,
					.instance_data_buffer = field.instance[frame_index],
					.grass_params_in = field.params[frame_index],
					.grass_pool = field.pool,
					.grass_tiles = field.tiles,
					.grass_visible = field.visible,
				}
			);
			record_draw(rec, d.indices, field, views.grass_camera_view);
		}
	}

	if (shadow_s && !shadow_s->requests.empty()) {
		co_await record_shadows(gpu_s, d, *shadow_s, camera_view_projection, frame_index, pass_out);
	}

	if (rt_s) {
		co_await record_rt(gpu_s, d, grass_s, *rt_s, material_index->second, frame_index, pass_out);
	}

	co_await record_forward(gpu_s, d, *forward_s.surface, frame_index, pass_out);
}
