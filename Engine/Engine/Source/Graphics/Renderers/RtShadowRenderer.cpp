module gse.graphics:rt_shadow_renderer_impl;

import std;

import :rt_shadow_renderer;
import :geometry_collector;
import :mesh;
import :physics_transform_renderer;


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

namespace gse::renderer::rt_shadow {
	constexpr bool use_gpu_tlas_transform_update = true;

	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t count;
		std::uint32_t instance_stride;
		std::uint32_t model_matrix_offset;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::byte_address_buffer
	]] source_instance_data {};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] index_mapping {
		using element = std::uint32_t;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::rw_byte_address_buffer
	]] tlas_instances {};

	using shader_binding_types = type_pack<source_instance_data, index_mapping, tlas_instances>;

	using entry = gpu::compute_entry<gpu::body_path<"Compute/tlas_transform_update">, gpu::bindings<shader_binding_types>, gpu::threads<64>, gpu::push_constant<push_constants>, gpu::system_values<gpu::dispatch_thread_id>>;
}

auto gse::renderer::rt_shadow::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	log::println(log::category::render, "RT shadow: initialized");
	log::println(log::category::render, "RT shadow: gpu tlas transform update {}", use_gpu_tlas_transform_update ? "enabled" : "disabled");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		d.tlas_per_frame[i] = gpu::build_tlas(*gpu_s.device, geometry_collector::data::max_instances);
		log::println(log::category::render, "RT shadow: tlas[{}] device_addr=0x{:x} instance_buf_addr=0x{:x}", i, d.tlas_per_frame[i].device_address(), d.tlas_per_frame[i].instance_buffer().device_address());
		d.tlas_ptrs[i] = &d.tlas_per_frame[i];
		d.instances[i].reserve(geometry_collector::data::max_instances);
	}

	if constexpr (use_gpu_tlas_transform_update) {
		d.tlas_update_pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);
	}

	return {};
}

auto gse::renderer::rt_shadow::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, shared_view<geometry_collector::data> gc_r) -> async::task<> {
	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	std::vector<const mesh*> new_blas_meshes;
	gpu::device_size max_blas_scratch = 0;

	for (const auto& batch : data.normal_batches) {
		const auto& m = batch.key.model_ptr->meshes()[batch.key.mesh_index];
		const auto* mesh_ptr = &m;

		if (d.blas_cache.contains(mesh_ptr)) {
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
		log::println(log::category::render, "rt_shadow: BLAS build verts={} gpu_indices={} cpu_indices={} max_index={} oob={}", vertex_count, index_count, mesh_indices.size(), max_index, indices_out_of_range);
		if (indices_out_of_range) {
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

		d.blas_cache[mesh_ptr] = gpu_s.device->create_blas(geometry, prim_count);

		const auto sizes = gpu_s.device->query_blas_build_sizes(geometry, prim_count);
		max_blas_scratch = std::max(max_blas_scratch, sizes.build_scratch_size);

		new_blas_meshes.push_back(mesh_ptr);
	}

	if (!new_blas_meshes.empty()) {
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

	auto& instances = d.instances[frame_index];
	instances.clear();

	linear_vector<std::uint32_t> mapping;
	if constexpr (use_gpu_tlas_transform_update) {
		mapping.reserve(data.render_queue.size());
	}

	std::uint32_t render_queue_idx = 0;
	for (const auto& queue_entry : data.render_queue) {
		const auto& entry = queue_entry.entry;
		if (!entry.model.valid()) {
			++render_queue_idx;
			continue;
		}
		const auto& mdl = entry.model.resolve();
		if (entry.index >= mdl.meshes().size()) {
			++render_queue_idx;
			continue;
		}

		const auto* mesh_ptr = &mdl.meshes()[entry.index];
		const auto it = d.blas_cache.find(mesh_ptr);
		if (it == d.blas_cache.end()) {
			++render_queue_idx;
			continue;
		}

		std::uint32_t palette_idx = 0;
		if (const auto palette_it = data.material_palette_map.find(&mesh_ptr->material()); palette_it != data.material_palette_map.end()) {
			palette_idx = palette_it->second;
		}

		instances.push_back({
			.transform = entry.model_matrix,
			.custom_index = palette_idx,
			.cull_disable = true,
			.blas_address = it->second.device_address()
		});

		if constexpr (use_gpu_tlas_transform_update) {
			mapping.push_back(render_queue_idx);
		}
		++render_queue_idx;

		if (instances.size() >= geometry_collector::data::max_instances) {
			break;
		}
	}

	const auto instance_count = static_cast<std::uint32_t>(instances.size());

	gpu::write_tlas_instances(d.tlas_per_frame[frame_index], instances.span());

	if constexpr (use_gpu_tlas_transform_update) {
		if (instance_count != 0) {
			const auto mapping_bytes = instance_count * sizeof(std::uint32_t);
			if (d.mapping_buffer_capacity < mapping_bytes) {
				for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
					d.mapping_buffers[i] = gpu_s.device->create_buffer(
						{
							.size = mapping_bytes,
							.stride = sizeof(std::uint32_t),
							.usage = gpu::buffer_flag::storage,
							.bindless = true
						}
					);
				}
				d.mapping_buffer_capacity = mapping_bytes;
			}

			d.mapping_buffers[frame_index].host_write(mapping.data(), mapping_bytes);

			auto& tlas_inst_buf = d.tlas_per_frame[frame_index].instance_buffer();

			if (!d.tlas_instance_views[frame_index].valid()) {
				d.tlas_instance_views[frame_index] = gpu_s.device->allocate_buffer_slot();
			}
			gpu_s.device->write_storage_buffer(d.tlas_instance_views[frame_index].slot(), tlas_inst_buf.device_address(), instance_count * 64);
		}
	}

	const auto build_new_blas = [&](auto& rec) {
		for (const auto* mesh_ptr : new_blas_meshes) {
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

			gpu::build_blas_in_place(*gpu_s.device, d.blas_cache.at(mesh_ptr).handle(), geometry, prim_count, d.blas_scratch[frame_index], rec);
		}
	};

	if constexpr (use_gpu_tlas_transform_update) {
		auto rec = co_await gpu::pass<^^gse::renderer::rt_shadow::frame>(ctx).pipeline(d.tlas_update_pipeline).after<^^geometry_collector::frame, ^^physics_transform::frame>();
		build_new_blas(rec);
		if (instance_count != 0) {
			const std::uint32_t workgroups = (instance_count + 63) / 64;
			rec.barrier(gpu::barrier_scope::host_to_compute);
			rec.barrier(gpu::barrier_scope::compute_to_compute);
			rec.dispatch<entry>(
				{
					.count = instance_count,
					.instance_stride = static_cast<std::uint32_t>(sizeof(shaders::common::instance_data)),
					.model_matrix_offset = 0,
				},
				{
					.source_instance_data = gc_r.instance_buffer[frame_index].slot(),
					.index_mapping = d.mapping_buffers[frame_index].slot(),
					.tlas_instances = d.tlas_instance_views[frame_index].slot(),
				},
				vec3u{ workgroups, 1u, 1u }
			);
		}
		gpu::build_tlas_in_place(*gpu_s.device, d.tlas_per_frame[frame_index], instance_count, rec);
	}
	else {
		auto rec = co_await gpu::pass<^^gse::renderer::rt_shadow::frame>(ctx).after<^^geometry_collector::frame>();
		build_new_blas(rec);
		gpu::build_tlas_in_place(*gpu_s.device, d.tlas_per_frame[frame_index], instance_count, rec);
	}
}
