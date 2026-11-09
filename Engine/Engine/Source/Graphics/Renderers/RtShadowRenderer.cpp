module gse.graphics:rt_shadow_renderer_impl;

import std;

import :rt_shadow_renderer;
import :deformed_target;
import :geometry_collector;
import :grass_renderer;
import :lighting_source;
import :mesh;
import :physics_transform_renderer;
import :skin_renderer;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.log;
import gse.physics;

namespace gse::renderer::rt_shadow {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t count;
	};

	struct transform_bindings {
		shaders::ssbo_readonly<shaders::common::instance_data> source_instance_data;
		shaders::ssbo_readonly<std::uint32_t> index_mapping;
		shaders::rw_byte_address_buffer tlas_instances;
	};

	using entry = gpu::compute_entry<gpu::body_path<"Compute/tlas_transform_update">, gpu::types<shaders::common::shader_types>, gpu::bindings<transform_bindings>, gpu::threads<64>, gpu::push_constant<push_constants>, gpu::system_values<gpu::dispatch_thread_id>>;

	struct [[= shaders::shader_constant_block]] grass_rt_layout {
		std::uint32_t grass_tlas_instance_bytes = sizeof(gpu::acceleration_structure_instance);
		std::uint32_t grass_tlas_mask = 0xFF;
		std::uint32_t grass_tlas_cull_disable = 1;
	};

	struct grass_mesh_bindings {
		shaders::ssbo_readonly<physics::grass::grass_params> grass_params_in;
		shaders::ssbo_readonly<physics::grass::grass_node> grass_pool;
		shaders::ssbo_readonly<physics::grass::grass_slot> rt_slots;
		shaders::ssbo_readwrite<vec4f> rt_vertices_out;
	};

	struct grass_instance_bindings {
		shaders::ssbo_readonly<physics::grass::grass_params> grass_params_in;
		shaders::ssbo_readonly<physics::grass::grass_tile> grass_tiles;
		shaders::ssbo_readonly<vec2u> rt_static_blas;
		shaders::ssbo_readonly<vec2u> rt_dynamic_blas;
		shaders::rw_byte_address_buffer tlas_instances;
	};

	struct [[= shaders::shader_struct]] grass_mesh_push_constants {
		std::uint32_t first_tile;
		std::uint32_t dynamic;
	};

	struct [[= shaders::shader_struct]] grass_instance_push_constants {
		std::uint32_t first_instance;
		std::uint32_t material_index;
	};

	using grass_mesh_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tile_mesh">,
		gpu::types<grass::shader_types>,
		gpu::bindings<grass_mesh_bindings>,
		gpu::helpers<"Grass/grass_shape", "Grass/grass_geometry">,
		gpu::threads<physics::grass::layout.grass_sim_threads>,
		gpu::push_constant<grass_mesh_push_constants>,
		gpu::system_values<gpu::group_id, gpu::group_thread_id>
	>;

	using grass_instance_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tlas_instances">,
		gpu::types<grass::shader_types, type_pack<grass_rt_layout>>,
		gpu::bindings<grass_instance_bindings>,
		gpu::helpers<"Grass/grass_shape">,
		gpu::threads<64>,
		gpu::push_constant<grass_instance_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	constexpr std::uint32_t grass_tile_vertices = physics::grass::layout.grass_tile_blade_count * physics::grass::layout.grass_side_vertices;
	constexpr std::uint32_t grass_tile_primitives = grass::index_count() / 3;
	constexpr std::size_t grass_tile_vertex_bytes = static_cast<std::size_t>(grass_tile_vertices) * sizeof(vec4f);
	constexpr std::size_t grass_slot_bytes = static_cast<std::size_t>(physics::grass::layout.grass_slot_count) * sizeof(physics::grass::grass_slot);
	constexpr std::size_t mebibyte = 1024 * 1024;

	constexpr gpu::build_acceleration_structure_flags rebuilt_blas_flags{ gpu::build_acceleration_structure_flag::prefer_fast_build };
	constexpr gpu::build_acceleration_structure_flags refit_blas_flags{ gpu::build_acceleration_structure_flag::prefer_fast_build, gpu::build_acceleration_structure_flag::allow_update };

	struct grass_rt_work {
		const physics::grass::field_buffers* buffers = nullptr;
		grass_rt_field* field = nullptr;
		std::uint32_t first_instance = 0;
		std::uint32_t static_first = 0;
		std::uint32_t static_rows = 0;
	};

	auto write_tlas_slots(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto grass_tile_geometry(
		const gpu::buffer& vertices,
		std::uint32_t row,
		const gpu::buffer& indices
	) -> gpu::acceleration_structure_geometry;

	auto record_grass_blas(
		gpu::recording_context& rec,
		gpu::acceleration_structure dst,
		const gpu::acceleration_structure_geometry& geometry,
		gpu::build_acceleration_structure_flags flags,
		gpu::build_acceleration_structure_mode mode,
		gpu::device_address scratch
	) -> void;

	auto grass_vertex_buffer(
		gpu::device& device,
		std::uint32_t rows,
		std::string_view tag
	) -> gpu::buffer;

	auto grass_address_buffer(
		gpu::device& device,
		std::uint32_t count,
		std::string_view tag
	) -> gpu::buffer;

	auto create_grass_rt_field(
		shared_view<gpu::context::data> gpu_s,
		data& d,
		const physics::grass::field_buffers& fb,
		const gpu::buffer& indices,
		grass_rt_field& rf
	) -> void;

	auto prepare_grass_field(
		gpu::device& device,
		std::uint32_t frame_index,
		const gpu::buffer& indices,
		grass_rt_field& rf
	) -> void;

	struct deformed_build {
		blas_key key;
		const gpu::buffer* vertices = nullptr;
		const mesh* source = nullptr;
		std::uint32_t vertex_count = 0;
		std::uint32_t index_count = 0;
	};

	auto deformed_key_of(
		const normal_instance_batch& batch
	) -> blas_key;

	auto deformed_vertices_of(
		const normal_instance_batch& batch,
		shared_view<skin::data> skin_s
	) -> const gpu::buffer*;
}

auto gse::renderer::rt_shadow::deformed_key_of(const normal_instance_batch& batch) -> blas_key {
	const auto mesh_index = static_cast<std::uint32_t>(batch.key.mesh_index);
	return batch.key.skinned.valid()
		? blas_key{ batch.key.skinned_owner, mesh_index }
		: blas_key{ batch.key.model, mesh_index };
}

auto gse::renderer::rt_shadow::deformed_vertices_of(const normal_instance_batch& batch, const shared_view<skin::data> skin_s) -> const gpu::buffer* {
	const deformed_target* target = nullptr;
	if (batch.key.skinned.valid()) {
		if (const auto it = skin_s.targets.find(deformed_key_of(batch)); it != skin_s.targets.end()) {
			target = &it->second;
		}
	}
	if (target == nullptr || !target->current().valid()) {
		return nullptr;
	}
	return &target->current();
}

auto gse::renderer::rt_shadow::write_tlas_slots(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		if (!d.tlas_slots[i].valid()) {
			d.tlas_slots[i] = gpu_s.device->allocate_acceleration_structure_slot();
		}
		gpu_s.device->write_acceleration_structure(d.tlas_slots[i].slot(), d.tlas_per_frame[i].device_address());
	}
}

auto gse::renderer::rt_shadow::grass_tile_geometry(const gpu::buffer& vertices, const std::uint32_t row, const gpu::buffer& indices) -> gpu::acceleration_structure_geometry {
	auto geometry = gpu::make_blas_geometry({
		.vertex_buffer = &vertices,
		.vertex_count = grass_tile_vertices,
		.vertex_stride = static_cast<std::uint32_t>(sizeof(vec4f)),
		.index_buffer = &indices,
		.index_count = grass::index_count(),
	});
	geometry.triangles.vertex_data += static_cast<gpu::device_address>(row) * grass_tile_vertex_bytes;
	return geometry;
}

auto gse::renderer::rt_shadow::record_grass_blas(gpu::recording_context& rec, const gpu::acceleration_structure dst, const gpu::acceleration_structure_geometry& geometry, const gpu::build_acceleration_structure_flags flags, const gpu::build_acceleration_structure_mode mode, const gpu::device_address scratch) -> void {
	const gpu::acceleration_structure_build_range_info range{
		.primitive_count = grass_tile_primitives,
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

auto gse::renderer::rt_shadow::grass_vertex_buffer(gpu::device& device, const std::uint32_t rows, const std::string_view tag) -> gpu::buffer {
	return device.create_buffer(
		{
			.size = static_cast<std::size_t>(rows) * grass_tile_vertex_bytes,
			.stride = sizeof(vec4f),
			.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::acceleration_structure_build_input },
			.writable = true,
			.device_local = true,
		},
		tag
	);
}

auto gse::renderer::rt_shadow::grass_address_buffer(gpu::device& device, const std::uint32_t count, const std::string_view tag) -> gpu::buffer {
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

auto gse::renderer::rt_shadow::create_grass_rt_field(const shared_view<gpu::context::data> gpu_s, data& d, const physics::grass::field_buffers& fb, const gpu::buffer& indices, grass_rt_field& rf) -> void {
	auto& device = *gpu_s.device;
	rf.field = fb.field;
	rf.tile_count = fb.tile_count;
	rf.static_built = 0;
	rf.occupied.clear();
	rf.slot_generation = 0;
	rf.slot_applied = 0;
	rf.slot_readback = gpu_s.render_graph->create_readback_channel(grass_slot_bytes, "grass.rt.slots");

	const auto batch_rows = std::min(grass_static_batch, rf.tile_count);
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		rf.static_published[i] = 0;
		rf.static_addresses[i] = grass_address_buffer(device, rf.tile_count, "grass.rt.static_addresses");
		rf.static_vertices[i] = grass_vertex_buffer(device, batch_rows, "grass.rt.static_vertices");
		rf.dynamic_blas[i].clear();
		rf.dynamic_built_tile[i].clear();
		rf.dynamic_vertices[i] = gpu::buffer{};
		rf.dynamic_capacity[i] = 0;
		rf.dynamic_addresses[i] = grass_address_buffer(device, physics::grass::layout.grass_slot_count, "grass.rt.dynamic_addresses");
	}

	const auto geometry = grass_tile_geometry(rf.static_vertices[0], 0, indices);
	const auto sizes = device.query_blas_build_sizes(geometry, grass_tile_primitives, rebuilt_blas_flags);
	const auto refit_sizes = device.query_blas_build_sizes(geometry, grass_tile_primitives, refit_blas_flags);
	d.grass_tile_blas_bytes = sizes.acceleration_structure_size;
	d.grass_tile_scratch = std::max({ sizes.build_scratch_size, refit_sizes.build_scratch_size, refit_sizes.update_scratch_size });

	const auto static_bytes = d.grass_tile_blas_bytes * rf.tile_count;
	rf.over_budget = static_bytes > grass_static_budget;
	log::println(
		log::category::render,
		"Grass RT: {} tiles x {} KiB BLAS ({} triangles each) = {} MiB static; build scratch {} KiB; batch vertices {} MiB x {}",
		rf.tile_count,
		d.grass_tile_blas_bytes / 1024,
		grass_tile_primitives,
		static_bytes / mebibyte,
		d.grass_tile_scratch / 1024,
		rf.static_vertices[0].size() / mebibyte,
		per_frame_resource<gpu::buffer>::frames_in_flight
	);

	rf.static_blas.clear();
	if (rf.over_budget) {
		log::println(log::level::warning, log::category::render, "Grass RT: static BLAS {} MiB exceeds the {} MiB budget; resting tiles cast no RT shadow", static_bytes / mebibyte, grass_static_budget / mebibyte);
		for (auto& vertices : rf.static_vertices) {
			vertices = gpu::buffer{};
		}
		return;
	}

	rf.static_blas.reserve(rf.tile_count);
	for (std::uint32_t tile = 0; tile < rf.tile_count; ++tile) {
		rf.static_blas.push_back(device.create_blas(geometry, grass_tile_primitives, rebuilt_blas_flags));
	}
}

auto gse::renderer::rt_shadow::prepare_grass_field(gpu::device& device, const std::uint32_t frame_index, const gpu::buffer& indices, grass_rt_field& rf) -> void {
	if (const auto view = rf.slot_readback.latest(); view.generation > rf.slot_applied && view.bytes.size() >= grass_slot_bytes) {
		rf.slot_applied = view.generation;
		rf.occupied.clear();
		for (std::uint32_t slot = 0; slot < physics::grass::layout.grass_slot_count; ++slot) {
			physics::grass::grass_slot entry;
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
		capacity = std::min((needed + grass_dynamic_growth - 1) / grass_dynamic_growth * grass_dynamic_growth, physics::grass::layout.grass_slot_count);
		rf.dynamic_vertices[frame_index] = grass_vertex_buffer(device, capacity, "grass.rt.dynamic_vertices");
		log::println(log::category::render, "Grass RT: dynamic vertices grown to {} slots ({} MiB) for frame {}", capacity, rf.dynamic_vertices[frame_index].size() / mebibyte, frame_index);
	}

	auto& blases = rf.dynamic_blas[frame_index];
	if (blases.size() < capacity) {
		blases.resize(capacity);
		rf.dynamic_built_tile[frame_index].resize(capacity, 0);
	}
	for (const auto& occupied : rf.occupied) {
		if (!blases[occupied.slot].valid()) {
			blases[occupied.slot] = device.create_blas(grass_tile_geometry(rf.dynamic_vertices[frame_index], occupied.slot, indices), grass_tile_primitives, refit_blas_flags);
		}
	}
}

auto gse::renderer::rt_shadow::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	log::println(log::category::render, "RT shadow: initialized");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		d.tlas_per_frame[i] = gpu::build_tlas(*gpu_s.device, geometry_collector::data::max_instances + grass_tlas_capacity);
		d.instances[i].reserve(geometry_collector::data::max_instances);
	}

	d.tlas_update_pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);
	d.grass_mesh_pipeline = gpu::build_compute_program(*gpu_s.device, grass_mesh_entry::pod);
	d.grass_instance_pipeline = gpu::build_compute_program(*gpu_s.device, grass_instance_entry::pod);

	write_tlas_slots(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			write_tlas_slots(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::rt_shadow::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_write<lighting_source::ray_traced_scene> scene_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<geometry_collector::data> gc_r, const shared_view<skin::data> skin_s, const shared_view<grass::data> grass_r, const shared_view<physics::grass::data> grass_s) -> async::task<> {
	lighting_source::ray_traced_scene scene{
		.pass = gpu::pass_id<^^frame>(),
	};
	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		scene.tlas_addresses[i] = d.tlas_per_frame[i].device_address();
		scene.tlas_slots[i] = d.tlas_slots[i].slot();
	}
	scene_out.push<lighting_source::ray_traced_scene>(std::move(scene));

	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	std::vector<blas_key> new_blas_keys;
	std::vector<deformed_build> deformed_builds;
	gpu::device_size max_blas_scratch = 0;

	for (const auto& batch : data.normal_batches) {
		if (batch.deformed_vertices.valid()) {
			const auto* vertices = deformed_vertices_of(batch, skin_s);
			const auto& m = batch.key.resolve_mesh();
			if (vertices == nullptr || !m.upload_token().ready()) {
				continue;
			}
			const auto vertex_count = static_cast<std::uint32_t>(m.vertex_gpu_buffer().size() / sizeof(vertex));
			const auto index_count = static_cast<std::uint32_t>(m.index_gpu_buffer().size() / sizeof(std::uint32_t));
			if (vertex_count == 0 || index_count < 3) {
				continue;
			}
			const auto geometry = gpu::make_blas_geometry({
				.vertex_buffer = vertices,
				.vertex_count = vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer = &m.index_gpu_buffer(),
				.index_count = index_count
			});
			const std::uint32_t prim_count = index_count / 3;
			const auto key = deformed_key_of(batch);
			auto& entry = d.deformed_blas[key][frame_index];
			if (!entry.blas.valid() || entry.source != &m || entry.vertex_count != vertex_count || entry.prim_count != prim_count) {
				const auto sizes = gpu_s.device->query_blas_build_sizes(geometry, prim_count, refit_blas_flags);
				entry = {
					.blas = gpu_s.device->create_blas(geometry, prim_count, refit_blas_flags),
					.source = &m,
					.vertex_count = vertex_count,
					.prim_count = prim_count,
					.scratch_size = std::max(sizes.build_scratch_size, sizes.update_scratch_size),
				};
			}
			max_blas_scratch = std::max(max_blas_scratch, entry.scratch_size);
			deformed_builds.push_back({
				.key = key,
				.vertices = vertices,
				.source = &m,
				.vertex_count = vertex_count,
				.index_count = index_count,
			});
			continue;
		}

		if (batch.key.skinned.valid()) {
			continue;
		}

		const auto& owner = batch.key.model_snapshot;
		const auto& m = owner->meshes()[batch.key.mesh_index];
		const auto* mesh_ptr = &m;
		const blas_key cache_key{ batch.key.model, static_cast<std::uint32_t>(batch.key.mesh_index) };

		if (const auto cached = d.blas_cache.find(cache_key); cached != d.blas_cache.end() && cached->second.source == mesh_ptr) {
			continue;
		}

		if (!m.upload_token().ready()) {
			continue;
		}

		const auto vertex_count = static_cast<std::uint32_t>(m.vertex_gpu_buffer().size() / sizeof(vertex));
		const auto index_count = static_cast<std::uint32_t>(m.index_gpu_buffer().size() / sizeof(std::uint32_t));

		if (vertex_count == 0 || index_count == 0) {
			continue;
		}

		const auto& mesh_indices = m.indices();
		std::uint32_t max_index = 0;
		for (const auto idx : mesh_indices) {
			max_index = std::max(max_index, idx);
		}
		const bool indices_out_of_range = !mesh_indices.empty() && max_index >= vertex_count;
		if (indices_out_of_range) {
			log::println(log::level::warning, log::category::render, "rt_shadow: BLAS skipped, index out of range: verts={} gpu_indices={} cpu_indices={} max_index={}", vertex_count, index_count, mesh_indices.size(), max_index);
			continue;
		}

		const auto geometry = gpu::make_blas_geometry({
			.vertex_buffer = &m.vertex_gpu_buffer(),
			.vertex_count = vertex_count,
			.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
			.index_buffer = &m.index_gpu_buffer(),
			.index_count = index_count
		});
		const std::uint32_t prim_count = index_count / 3;

		d.blas_cache.insert_or_assign(
			cache_key,
			blas_entry{
				.source = mesh_ptr,
				.blas = gpu_s.device->create_blas(geometry, prim_count, rebuilt_blas_flags)
			}
		);

		const auto sizes = gpu_s.device->query_blas_build_sizes(geometry, prim_count, rebuilt_blas_flags);
		max_blas_scratch = std::max(max_blas_scratch, sizes.build_scratch_size);

		new_blas_keys.push_back(cache_key);
	}

	auto& instances = d.instances[frame_index];
	instances.clear();

	linear_vector<std::uint32_t> mapping;
	mapping.reserve(geometry_collector::data::max_instances);

	for (const auto& batch : data.normal_batches) {
		if (instances.size() >= geometry_collector::data::max_instances) {
			break;
		}
		gpu::device_address blas_address{};
		if (batch.deformed_vertices.valid()) {
			const auto key = deformed_key_of(batch);
			if (std::ranges::none_of(deformed_builds, [&](const deformed_build& build) {
				return build.key == key;
			})) {
				continue;
			}
			blas_address = d.deformed_blas.at(key)[frame_index].blas.device_address();
		}
		else {
			if (batch.key.skinned.valid()) {
				continue;
			}

			const auto* mesh_ptr = &batch.key.model_snapshot->meshes()[batch.key.mesh_index];
			const auto it = d.blas_cache.find({ batch.key.model, static_cast<std::uint32_t>(batch.key.mesh_index) });
			if (it == d.blas_cache.end() || it->second.source != mesh_ptr) {
				continue;
			}

			blas_address = it->second.blas.device_address();
		}
		const auto count = std::min<std::size_t>(batch.instance_count, geometry_collector::data::max_instances - instances.size());
		for (std::uint32_t k = 0; k < count; ++k) {
			const std::uint32_t instance_index = batch.first_instance + k;
			const auto& staged = data.instance_staging[instance_index];
			instances.push_back({
				.transform = staged.model_matrix,
				.custom_index = staged.material_index,
				.cull_disable = true,
				.blas_address = blas_address
			});
			mapping.push_back(instance_index);
		}
	}

	const auto instance_count = static_cast<std::uint32_t>(instances.size());

	std::erase_if(d.grass_fields, [&](const auto& entry) {
		return !grass_s.buffers.contains(entry.first);
	});

	std::vector<grass_rt_work> grass_work;
	std::uint32_t grass_instances = 0;
	std::uint32_t grass_builds = 0;
	for (const auto& [owner, fb] : grass_s.buffers) {
		if (fb.tile_count == 0) {
			continue;
		}
		const auto rendered = grass_r.fields.find(owner);
		if (rendered == grass_r.fields.end() || !rendered->second.culled) {
			continue;
		}
		if (grass_instances + fb.tile_count > grass_tlas_capacity) {
			continue;
		}

		auto found = d.grass_fields.find(owner);
		if (found == d.grass_fields.end() || found->second.tile_count != fb.tile_count || physics::grass::rest_shape_changed(found->second.field, fb.field)) {
			d.grass_fields.erase(owner);
			found = d.grass_fields.try_emplace(owner).first;
			create_grass_rt_field(gpu_s, d, fb, grass_r.indices, found->second);
		}
		auto& rf = found->second;
		prepare_grass_field(*gpu_s.device, frame_index, grass_r.indices, rf);

		const auto static_rows = rf.over_budget ? 0u : std::min(grass_static_batch, rf.tile_count - rf.static_built);
		grass_builds += static_rows + static_cast<std::uint32_t>(rf.occupied.size());
		grass_work.push_back({
			.buffers = &fb,
			.field = &rf,
			.first_instance = instance_count + grass_instances,
			.static_first = rf.static_built,
			.static_rows = static_rows,
		});
		grass_instances += fb.tile_count;
	}

	if (max_blas_scratch != 0) {
		const auto scratch_alignment = gpu_s.device->acceleration_structure_scratch_alignment();
		const auto required_scratch = max_blas_scratch + scratch_alignment;
		if (d.blas_scratch[frame_index].size() < required_scratch) {
			d.blas_scratch[frame_index] = gpu_s.device->create_buffer(
				{
					.size = required_scratch,
					.usage = gpu::buffer_flag::acceleration_structure_scratch,
				}
			);
		}
	}

	const auto scratch_alignment = gpu_s.device->acceleration_structure_scratch_alignment();
	const auto grass_scratch_stride = (d.grass_tile_scratch + scratch_alignment - 1) / scratch_alignment * scratch_alignment;
	if (grass_builds != 0) {
		const auto required = grass_scratch_stride * grass_builds + scratch_alignment;
		if (d.grass_scratch[frame_index].size() < required) {
			d.grass_scratch[frame_index] = gpu_s.device->create_buffer(
				{
					.size = required,
					.usage = gpu::buffer_flag::acceleration_structure_scratch,
				},
				"grass.rt.scratch"
			);
			log::println(log::category::render, "Grass RT: build scratch grown to {} KiB for {} concurrent builds (frame {})", required / 1024, grass_builds, frame_index);
		}
	}

	for (const auto& work : grass_work) {
		auto& rf = *work.field;
		const auto built = rf.static_built + work.static_rows;
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

		std::array<gpu::device_address, physics::grass::layout.grass_slot_count> dynamic{};
		const auto& blases = rf.dynamic_blas[frame_index];
		for (const auto& occupied : rf.occupied) {
			dynamic[occupied.slot] = blases[occupied.slot].device_address();
		}
		rf.dynamic_addresses[frame_index].host_write(dynamic.data(), sizeof(dynamic));
	}

	const auto total_instances = instance_count + grass_instances;

	gpu::write_tlas_instances(d.tlas_per_frame[frame_index], instances.span());

	if (total_instances != 0) {
		auto& tlas_inst_buf = d.tlas_per_frame[frame_index].instance_buffer();
		if (!d.tlas_instance_views[frame_index].valid()) {
			d.tlas_instance_views[frame_index] = gpu_s.device->allocate_buffer_slot();
		}
		gpu_s.device->write_storage_buffer(d.tlas_instance_views[frame_index].slot(), tlas_inst_buf, total_instances * sizeof(gpu::acceleration_structure_instance));
	}

	if (instance_count != 0) {
		const auto mapping_bytes = instance_count * sizeof(std::uint32_t);
		if (d.mapping_buffer_capacity < mapping_bytes) {
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				d.mapping_buffers[i] = gpu_s.device->create_buffer(
					{
						.size = mapping_bytes,
						.stride = sizeof(std::uint32_t),
						.usage = gpu::buffer_flag::storage,
					}
				);
			}
			d.mapping_buffer_capacity = mapping_bytes;
		}

		d.mapping_buffers[frame_index].host_write(mapping.data(), mapping_bytes);
	}

	const auto build_new_blas = [&](auto& rec) {
		for (const auto& cache_key : new_blas_keys) {
			auto& cached = d.blas_cache.at(cache_key);
			const auto* mesh_ptr = cached.source;
			const auto vertex_count = static_cast<std::uint32_t>(mesh_ptr->vertex_gpu_buffer().size() / sizeof(vertex));
			const auto index_count = static_cast<std::uint32_t>(mesh_ptr->index_gpu_buffer().size() / sizeof(std::uint32_t));

			const auto geometry = gpu::make_blas_geometry({
				.vertex_buffer = &mesh_ptr->vertex_gpu_buffer(),
				.vertex_count = vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer = &mesh_ptr->index_gpu_buffer(),
				.index_count = index_count
			});
			const std::uint32_t prim_count = index_count / 3;

			gpu::build_blas_in_place(*gpu_s.device, cached.blas.handle(), geometry, prim_count, rebuilt_blas_flags, gpu::build_acceleration_structure_mode::build, d.blas_scratch[frame_index], rec);
		}
		for (const auto& build : deformed_builds) {
			const auto geometry = gpu::make_blas_geometry({
				.vertex_buffer = build.vertices,
				.vertex_count = build.vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer = &build.source->index_gpu_buffer(),
				.index_count = build.index_count
			});
			auto& entry = d.deformed_blas.at(build.key)[frame_index];
			const auto mode = entry.built ? gpu::build_acceleration_structure_mode::update : gpu::build_acceleration_structure_mode::build;
			gpu::build_blas_in_place(*gpu_s.device, entry.blas.handle(), geometry, entry.prim_count, refit_blas_flags, mode, d.blas_scratch[frame_index], rec);
			entry.built = true;
		}
		if (grass_builds == 0) {
			return;
		}
		const auto scratch_base = (d.grass_scratch[frame_index].device_address() + scratch_alignment - 1) / scratch_alignment * scratch_alignment;
		std::uint32_t build_index = 0;
		const auto next_scratch = [&] {
			return scratch_base + grass_scratch_stride * build_index++;
		};
		for (const auto& work : grass_work) {
			auto& rf = *work.field;
			for (std::uint32_t row = 0; row < work.static_rows; ++row) {
				record_grass_blas(rec, rf.static_blas[work.static_first + row].handle(), grass_tile_geometry(rf.static_vertices[frame_index], row, grass_r.indices), rebuilt_blas_flags, gpu::build_acceleration_structure_mode::build, next_scratch());
			}
			rf.static_built += work.static_rows;
			if (work.static_rows != 0 && rf.static_built == rf.tile_count) {
				log::println(log::category::render, "Grass RT: all {} static tile BLASes built", rf.tile_count);
			}
			const auto& blases = rf.dynamic_blas[frame_index];
			auto& built_tile = rf.dynamic_built_tile[frame_index];
			for (const auto& occupied : rf.occupied) {
				const auto mode = built_tile[occupied.slot] == occupied.tile_plus_one ? gpu::build_acceleration_structure_mode::update : gpu::build_acceleration_structure_mode::build;
				record_grass_blas(rec, blases[occupied.slot].handle(), grass_tile_geometry(rf.dynamic_vertices[frame_index], occupied.slot, grass_r.indices), refit_blas_flags, mode, next_scratch());
				built_tile[occupied.slot] = occupied.tile_plus_one;
			}
		}
	};

	if (!grass_work.empty()) {
		auto mesh_rec = co_await gpu::pass<^^grass_mesh_pass>(pass_out)
			.pipeline(d.grass_mesh_pipeline)
			.after<^^grass::cull_pass, ^^physics::grass::simulate_pass>();
		constexpr std::uint32_t groups_x = (grass_tile_vertices + physics::grass::layout.grass_sim_threads - 1) / physics::grass::layout.grass_sim_threads;
		for (const auto& work : grass_work) {
			const auto& fb = *work.buffers;
			auto& rf = *work.field;
			if (work.static_rows != 0) {
				mesh_rec.dispatch<grass_mesh_entry>(
					{
						.first_tile = work.static_first,
						.dynamic = 0,
					},
					{
						.grass_params_in = fb.params[frame_index].slot(),
						.grass_pool = fb.nodes.slot(),
						.rt_slots = fb.slots.slot(),
						.rt_vertices_out = rf.static_vertices[frame_index].slot(),
					},
					vec3u{ groups_x, work.static_rows, 1u }
				);
			}
			if (const auto capacity = rf.dynamic_capacity[frame_index]; capacity != 0) {
				mesh_rec.dispatch<grass_mesh_entry>(
					{
						.first_tile = 0,
						.dynamic = 1,
					},
					{
						.grass_params_in = fb.params[frame_index].slot(),
						.grass_pool = fb.nodes.slot(),
						.rt_slots = fb.slots.slot(),
						.rt_vertices_out = rf.dynamic_vertices[frame_index].slot(),
					},
					vec3u{ groups_x, capacity, 1u }
				);
			}
			mesh_rec.copy_buffer(fb.slots, rf.slot_readback.publish_target(++rf.slot_generation, grass_slot_bytes), grass_slot_bytes);
		}
	}

	auto rec = co_await gpu::pass<^^frame>(pass_out).pipeline(d.tlas_update_pipeline).after<^^geometry_collector::frame, ^^physics_transform::frame, ^^skin::deform_pass, ^^grass::cull_pass, ^^grass_mesh_pass>();
	build_new_blas(rec);
	if (instance_count != 0) {
		const std::uint32_t workgroups = (instance_count + 63) / 64;
		rec.dispatch<entry>(
			{
				.count = instance_count,
			},
			{
				.source_instance_data = gc_r.instance_buffer[frame_index].slot(),
				.index_mapping = d.mapping_buffers[frame_index].slot(),
				.tlas_instances = d.tlas_instance_views[frame_index].slot(),
			},
			vec3u{ workgroups, 1u, 1u }
		);
	}
	if (!grass_work.empty()) {
		rec.bind(d.grass_instance_pipeline);
		for (const auto& work : grass_work) {
			const auto& fb = *work.buffers;
			const auto& rf = *work.field;
			rec.dispatch<grass_instance_entry>(
				{
					.first_instance = work.first_instance,
					.material_index = geometry_collector::data::grass_material_index,
				},
				{
					.grass_params_in = fb.params[frame_index].slot(),
					.grass_tiles = fb.tiles.slot(),
					.rt_static_blas = rf.static_addresses[frame_index].slot(),
					.rt_dynamic_blas = rf.dynamic_addresses[frame_index].slot(),
					.tlas_instances = d.tlas_instance_views[frame_index].slot(),
				},
				vec3u{ (rf.tile_count + 63) / 64, 1u, 1u }
			);
		}
	}
	gpu::build_tlas_in_place(*gpu_s.device, d.tlas_per_frame[frame_index], total_instances, rec);
}