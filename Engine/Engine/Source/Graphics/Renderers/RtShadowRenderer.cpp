module gse.graphics;

import std;

import :rt_shadow_renderer;
import :geometry_collector;
import :mesh;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.log;

namespace gse::renderer::rt_shadow {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t count;
		std::uint32_t instance_stride;
		std::uint32_t model_matrix_offset;
	};

	struct [[= shaders::binding<0, 0>{}, = shaders::byte_address_buffer]] source_instance_data {};

	struct [[= shaders::binding<0, 1>{}, = shaders::ssbo_readonly]] index_mapping {
		using element = std::uint32_t;
	};

	struct [[= shaders::binding<0, 2>{}, = shaders::rw_byte_address_buffer]] tlas_instances {};

	using shader_binding_types = type_pack<source_instance_data, index_mapping, tlas_instances>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/tlas_transform_update">,
		gpu::layout<"tlas_transform_update">,
		gpu::bindings<shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

auto gse::renderer::rt_shadow::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d) -> async::task<> {
	log::println(log::category::render, "RT shadow: initialized");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		d.tlas_per_frame[i] = gpu::build_tlas(*gpu_s.device, geometry_collector::system::data::max_instances);
		d.tlas_ptrs[i] = &d.tlas_per_frame[i];
		d.instances[i].reserve(geometry_collector::system::data::max_instances);
	}

	d.tlas_update_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.tlas_update_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);
	}

	co_return;
}

auto gse::renderer::rt_shadow::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<geometry_collector::system> gc_r) -> async::task<> {
	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	for (const auto& batch : data.normal_batches) {
		const auto& m = batch.key.model_ptr->meshes()[batch.key.mesh_index];

		if (const auto* mesh_ptr = &m; !d.blas_cache.contains(mesh_ptr)) {
			const auto vertex_count = static_cast<std::uint32_t>(m.vertex_gpu_buffer().size() / sizeof(vertex));
			const auto index_count  = static_cast<std::uint32_t>(m.index_gpu_buffer().size() / sizeof(std::uint32_t));

			if (vertex_count == 0 || index_count == 0) {
				continue;
			}

			d.blas_cache[mesh_ptr] = gpu::build_blas(*gpu_s.device, {
				.vertex_buffer = &m.vertex_gpu_buffer(),
				.vertex_count = vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer = &m.index_gpu_buffer(),
				.index_count = index_count
			});
		}
	}

	auto& instances = d.instances[frame_index];
	instances.clear();

	linear_vector<std::uint32_t> mapping;
	mapping.reserve(data.render_queue.size());

	std::uint32_t render_queue_idx = 0;
	for (const auto& queue_entry : data.render_queue) {
		const auto& entry = queue_entry.entry;
		const auto* mdl = entry.model.resolve();
		if (!mdl || entry.index >= mdl->meshes().size()) {
			++render_queue_idx;
			continue;
		}

		const auto* mesh_ptr = &mdl->meshes()[entry.index];
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

		mapping.push_back(render_queue_idx);
		++render_queue_idx;

		if (instances.size() >= geometry_collector::system::data::max_instances) {
			break;
		}
	}

	if (instances.empty()) {
		co_return;
	}

	const auto instance_count = static_cast<std::uint32_t>(instances.size());

	gpu::write_tlas_instances(d.tlas_per_frame[frame_index], instances.span());

	const auto mapping_bytes = instance_count * sizeof(std::uint32_t);
	if (d.mapping_buffer_capacity < mapping_bytes) {
		for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
			d.mapping_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
				.size = mapping_bytes,
				.usage = gpu::buffer_flag::storage
			});
		}
		d.mapping_buffer_capacity = mapping_bytes;
	}

	gse::memcpy(d.mapping_buffers[frame_index].mapped(), mapping.data(), mapping_bytes);

	auto& tlas_inst_buf = d.tlas_per_frame[frame_index].instance_buffer();

	gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.tlas_update_descriptors[frame_index])
		.buffer<source_instance_data>(gc_r.instance_buffer[frame_index], 0, gc_r.instance_buffer[frame_index].size())
		.buffer<index_mapping>(d.mapping_buffers[frame_index], 0, mapping_bytes)
		.buffer<tlas_instances>(tlas_inst_buf, 0, instance_count * 64)
		.commit();

	const gpu::typed_push_constants<push_constants> pc{
		.data = {
			.count = instance_count,
			.instance_stride = static_cast<std::uint32_t>(sizeof(shaders::common::instance_data)),
			.model_matrix_offset = 0,
		},
		.stages = gpu::stage_flag::compute,
	};

	const std::uint32_t workgroups = (instance_count + 63) / 64;

	auto rec = co_await gpu::pass<system>(ctx)
		.after<geometry_collector::system>()
		.reads(gpu::storage_read(gc_r.instance_buffer[frame_index], gpu::pipeline_stage::compute_shader))
		.tracks(gc_r.instance_buffer[frame_index]);

	rec.barrier(gpu::barrier_scope::transfer_to_compute);
	rec.bind(d.tlas_update_pipeline);
	rec.bind_descriptors(d.tlas_update_pipeline, d.tlas_update_descriptors[frame_index]);
	rec.push(d.tlas_update_pipeline, pc);
	rec.dispatch(workgroups, 1, 1);
	gpu::build_tlas_in_place(*gpu_s.device, d.tlas_per_frame[frame_index], instance_count, rec);
}
