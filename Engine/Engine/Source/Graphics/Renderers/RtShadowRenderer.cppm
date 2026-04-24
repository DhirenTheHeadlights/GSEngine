export module gse.graphics:rt_shadow_renderer;

import std;

import :geometry_collector;
import :physics_transform_renderer;
import :mesh;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.log;

export namespace gse::renderer::rt_shadow {
	constexpr std::uint32_t max_instances = 4096;

	struct state {
		per_frame_resource<const gpu::tlas*> tlas_ptrs{};

		[[nodiscard]] auto tlas(
			const std::uint32_t frame_index
		) const -> const gpu::tlas& {
			return *tlas_ptrs[frame_index];
		}
	};

	struct system {
		struct frame_data {
			std::unordered_map<const mesh*, gpu::blas> blas_cache;
			per_frame_resource<gpu::tlas> tlas_per_frame;
			per_frame_resource<linear_vector<gpu::acceleration_structure_instance>> instances;

			resource::handle<shader> tlas_update_shader;
			gpu::pipeline tlas_update_pipeline;
			per_frame_resource<gpu::descriptor_region> tlas_update_descriptors;
			per_frame_resource<gpu::buffer> mapping_buffers;
			std::size_t mapping_buffer_capacity = 0;
		};

		static auto initialize(
			const init_context& phase,
			frame_data& fd,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			frame_data& fd,
			const state& s,
			const geometry_collector::state& gc_s
		) -> async::task<>;
	};
}

auto gse::renderer::rt_shadow::system::initialize(const init_context& phase, frame_data& fd, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	log::println(log::category::render, "RT shadow: initialized");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		fd.tlas_per_frame[i] = gpu::build_tlas(ctx, max_instances);
		s.tlas_ptrs[i] = &fd.tlas_per_frame[i];
		fd.instances[i].reserve(max_instances);
	}

	fd.tlas_update_shader = ctx.get<shader>("Shaders/Compute/tlas_transform_update");
	ctx.instantly_load(fd.tlas_update_shader);

	fd.tlas_update_pipeline = gpu::create_compute_pipeline(ctx, *fd.tlas_update_shader, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		fd.tlas_update_descriptors[i] = gpu::allocate_descriptors(ctx, *fd.tlas_update_shader);
	}
}

auto gse::renderer::rt_shadow::system::frame(frame_context& ctx, frame_data& fd, const state& s, const geometry_collector::state& gc_s) -> async::task<> {

	auto& gpu = ctx.get<gpu::context>();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu.graph().frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu.graph().current_frame();

	for (const auto& batch : data.normal_batches) {
		const auto& m = batch.key.model_ptr->meshes()[batch.key.mesh_index];

		if (const auto* mesh_ptr = &m; !fd.blas_cache.contains(mesh_ptr)) {
			const auto vertex_count = static_cast<std::uint32_t>(m.vertex_gpu_buffer().size() / sizeof(vertex));
			const auto index_count  = static_cast<std::uint32_t>(m.index_gpu_buffer().size() / sizeof(std::uint32_t));

			if (vertex_count == 0 || index_count == 0) {
				continue;
			}

			fd.blas_cache[mesh_ptr] = gpu::build_blas(gpu, {
				.vertex_buffer = &m.vertex_gpu_buffer(),
				.vertex_count = vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer = &m.index_gpu_buffer(),
				.index_count = index_count
			});
		}
	}

	auto& instances = fd.instances[frame_index];
	instances.clear();

	linear_vector<std::uint32_t> index_mapping;
	index_mapping.reserve(data.render_queue.size());

	std::uint32_t render_queue_idx = 0;
	for (const auto& queue_entry : data.render_queue) {
		const auto& entry = queue_entry.entry;
		const auto* mdl = entry.model.resolve();
		if (!mdl || entry.index >= mdl->meshes().size()) {
			++render_queue_idx;
			continue;
		}

		const auto* mesh_ptr = &mdl->meshes()[entry.index];
		const auto it = fd.blas_cache.find(mesh_ptr);
		if (it == fd.blas_cache.end()) {
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

		index_mapping.push_back(render_queue_idx);
		++render_queue_idx;

		if (instances.size() >= max_instances) {
			break;
		}
	}

	if (instances.empty()) {
		co_return;
	}

	const auto instance_count = static_cast<std::uint32_t>(instances.size());

	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();

	gpu::write_tlas_instances(fd.tlas_per_frame[frame_index], instances.span());

	if (gc_r) {
		const auto mapping_bytes = instance_count * sizeof(std::uint32_t);
		if (fd.mapping_buffer_capacity < mapping_bytes) {
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				fd.mapping_buffers[i] = gpu::create_buffer(gpu, {
					.size = mapping_bytes,
					.usage = gpu::buffer_flag::storage
				});
			}
			fd.mapping_buffer_capacity = mapping_bytes;
		}

		gse::memcpy(fd.mapping_buffers[frame_index].mapped(), index_mapping.data(), mapping_bytes);

		auto& tlas_inst_buf = fd.tlas_per_frame[frame_index].instances_buffer();

		gpu::descriptor_writer(gpu, fd.tlas_update_shader, fd.tlas_update_descriptors[frame_index])
			.buffer("instance_data", gc_r->instance_buffer[frame_index], 0, gc_r->instance_buffer[frame_index].size())
			.buffer("index_mapping", fd.mapping_buffers[frame_index], 0, mapping_bytes)
			.buffer("tlas_instances", tlas_inst_buf, 0, instance_count * 64)
			.commit();

		auto pc = fd.tlas_update_shader->cache_push_block("push_constants");
		pc.set("count", instance_count);
		pc.set("instance_stride", gc_r->instance_stride);
		pc.set("model_matrix_offset", gc_r->instance_offsets.at("model_matrix"));

		const std::uint32_t workgroups = (instance_count + 63) / 64;

		auto pass = gpu.graph().add_pass<state>();
		pass.track(gc_r->instance_buffer[frame_index]);
		pass.reads(gpu::storage_read(gc_r->instance_buffer[frame_index], gpu::pipeline_stage::compute_shader))
			.after<geometry_collector::state>()
			.record([&fd, &gpu, frame_index, instance_count, workgroups, pc = std::move(pc)](gpu::recording_context& rec) {
				rec.barrier(gpu::barrier_scope::transfer_to_compute);
				rec.bind(fd.tlas_update_pipeline);
				rec.bind_descriptors(fd.tlas_update_pipeline, fd.tlas_update_descriptors[frame_index]);
				rec.push(fd.tlas_update_pipeline, pc);
				rec.dispatch(workgroups, 1, 1);
				gpu::build_tlas_in_place(gpu, fd.tlas_per_frame[frame_index], instance_count, rec);
			});
	}
	else {
		auto pass = gpu.graph().add_pass<state>();
		pass.record([&fd, &gpu, frame_index](gpu::recording_context& rec) {
			gpu::rebuild_tlas(gpu, fd.tlas_per_frame[frame_index], fd.instances[frame_index].span(), rec);
		});
	}
}
