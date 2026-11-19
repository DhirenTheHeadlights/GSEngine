module gse.graphics:scene_acceleration_renderer_impl;

import std;

import :scene_acceleration_renderer;
import :deformed_target;
import :geometry_collector;
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

namespace gse::renderer::scene_acceleration {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t count;
	};

	struct transform_bindings {
		shaders::ssbo_readonly<shaders::common::instance_data> source_instance_data;
		shaders::ssbo_readonly<std::uint32_t> index_mapping;
		shaders::rw_byte_address_buffer tlas_instances;
	};

	using entry = gpu::compute_entry<gpu::body_path<"Compute/tlas_transform_update">,gpu::bindings<transform_bindings>, gpu::threads<64>, gpu::push_constant<push_constants>>;

	constexpr gpu::build_acceleration_structure_flags rebuilt_blas_flags{ gpu::build_acceleration_structure_flag::prefer_fast_build };
	constexpr gpu::build_acceleration_structure_flags refit_blas_flags{ gpu::build_acceleration_structure_flag::prefer_fast_build, gpu::build_acceleration_structure_flag::allow_update };

	auto write_tlas_slots(
		shared_view<gpu::context::data> gpu_s,
		data& d
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

auto gse::renderer::scene_acceleration::deformed_key_of(const normal_instance_batch& batch) -> blas_key {
	const auto mesh_index = static_cast<std::uint32_t>(batch.key.mesh_index);
	return batch.key.skinned.valid()
		? blas_key{ batch.key.skinned_owner, mesh_index }
		: blas_key{ batch.key.model, mesh_index };
}

auto gse::renderer::scene_acceleration::deformed_vertices_of(const normal_instance_batch& batch, const shared_view<skin::data> skin_s) -> const gpu::buffer* {
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

auto gse::renderer::scene_acceleration::write_tlas_slots(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		if (!d.tlas_slots[i].valid()) {
			d.tlas_slots[i] = gpu_s.device->allocate_acceleration_structure_slot();
		}
		gpu_s.device->write_acceleration_structure(d.tlas_slots[i].slot(), d.tlas_per_frame[i].device_address());
	}
}

auto gse::renderer::scene_acceleration::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	log::println(log::category::render, "RT shadow: initialized");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		d.tlas_per_frame[i] = gpu::build_tlas(*gpu_s.device, geometry_collector::data::max_instances + contributed_instance_capacity);
		d.instances[i].reserve(geometry_collector::data::max_instances);
	}

	d.tlas_update_pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);

	write_tlas_slots(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			write_tlas_slots(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::scene_acceleration::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_write<lighting_source::ray_traced_scene> scene_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<geometry_collector::data> gc_r, const shared_view<skin::data> skin_s, const channel_read<instance_reservation> reservations_in) -> async::task<> {
	lighting_source::ray_traced_scene scene{
		.pass = gpu::pass_id<^^frame>(),
	};
	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		scene.tlas_addresses[i] = d.tlas_per_frame[i].device_address();
		scene.tlas_slots[i] = d.tlas_slots[i].slot();
	}
	scene_out.push<lighting_source::ray_traced_scene>(std::move(scene));

	for (const auto& [owner, capacity] : reservations_in.of<instance_reservation>()) {
		if (capacity == 0) {
			d.reservations.erase(owner);
		}
		else {
			d.reservations.insert_or_assign(owner, capacity);
		}
	}
	d.contributed.clear();

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
			log::println(log::level::warning, log::category::render, "scene_acceleration: BLAS skipped, index out of range: verts={} gpu_indices={} cpu_indices={} max_index={}", vertex_count, index_count, mesh_indices.size(), max_index);
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

	std::uint32_t contributed_count = 0;
	for (const auto& [owner, capacity] : d.reservations) {
		d.contributed.emplace(owner, instance_range{ .first = instance_count + contributed_count, .capacity = capacity });
		contributed_count += capacity;
	}
	assert(contributed_count <= contributed_instance_capacity, "scene_acceleration: {} contributed TLAS instances exceed the capacity of {}", contributed_count, contributed_instance_capacity);

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

	const auto total_instances = instance_count + contributed_count;

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

	{
		auto rec = co_await gpu::pass<^^instances_pass>(pass_out).pipeline(d.tlas_update_pipeline).after<^^geometry_collector::frame, ^^physics_transform::frame, ^^skin::deform_pass>();
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
		if (instance_count != 0) {
			rec.dispatch<entry>(
				{
					.count = instance_count,
				},
				{
					.source_instance_data = gc_r.instance_buffer[frame_index],
					.index_mapping = d.mapping_buffers[frame_index],
					.tlas_instances = d.tlas_instance_views[frame_index],
				},
				gpu::group_count<entry>(instance_count)
			);
		}
		if (contributed_count != 0) {
			constexpr std::size_t instance_bytes = sizeof(gpu::acceleration_structure_instance);
			rec.fill_buffer(d.tlas_per_frame[frame_index].instance_buffer(), instance_count * instance_bytes, contributed_count * instance_bytes);
		}
	}

	auto rec = co_await gpu::pass<^^frame>(pass_out).after<^^instances_pass>();
	gpu::build_tlas_in_place(*gpu_s.device, d.tlas_per_frame[frame_index], total_instances, rec);
}
