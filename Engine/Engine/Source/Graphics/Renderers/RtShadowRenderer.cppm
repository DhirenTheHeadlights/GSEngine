export module gse.graphics:rt_shadow_renderer;

import std;

import :geometry_collector;
import :mesh;

import gse.platform;
import gse.utility;
import gse.math;
import gse.log;

export namespace gse::renderer::rt_shadow {
	constexpr std::uint32_t max_instances = 4096;

	struct state {
		per_frame_resource<const gpu::tlas*> tlas_ptrs{};
		bool initialized = false;

		[[nodiscard]] auto tlas(const std::uint32_t frame_index) const -> const gpu::tlas& {
			return *tlas_ptrs[frame_index];
		}
	};

	struct system {
		struct frame_data {
			std::unordered_map<const mesh*, gpu::blas> blas_cache;
			per_frame_resource<gpu::tlas> tlas_per_frame;
		};

		static auto initialize(
			const init_context& phase,
			frame_data& fd,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};

	[[nodiscard]] auto transform_is_finite(const mat4f& transform) -> bool {
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				if (!std::isfinite(transform[col][row])) {
					return false;
				}
			}
		}
		return true;
	}
}

auto gse::renderer::rt_shadow::system::initialize(const init_context& phase, frame_data& fd, state& s) -> void {
	const auto& ctx = phase.get<gpu::context>();

	if (!ctx.device_ref().ray_tracing_enabled()) {
		log::println(log::level::warning, log::category::render, "RT shadow: ray tracing not supported, skipping");
		return;
	}

	s.initialized = true;
	log::println(log::category::render, "RT shadow: initialized");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		fd.tlas_per_frame[i] = gpu::build_tlas(ctx.device_ref(), max_instances);
		s.tlas_ptrs[i] = &fd.tlas_per_frame[i];
		log::println(
			log::category::render, "RT shadow: frame_slot={} tlas_handle={:#x}",
			i,
			s.tlas(static_cast<std::uint32_t>(i)).native_handle().value
		);
	}
}

auto gse::renderer::rt_shadow::system::frame(frame_context& ctx, frame_data& fd, const state& s) -> async::task<> {
	if (!s.initialized) {
		co_return;
	}

	co_await ctx.after<geometry_collector::state>();

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

	bool any_new_blas = false;
	for (const auto& batch : data.normal_batches) {
		const auto& m = batch.key.model_ptr->meshes()[batch.key.mesh_index];

		if (const auto* mesh_ptr = &m; !fd.blas_cache.contains(mesh_ptr)) {
			const auto vertex_count = static_cast<std::uint32_t>(m.vertex_gpu_buffer().size() / sizeof(vertex));
			const auto index_count  = static_cast<std::uint32_t>(m.index_gpu_buffer().size() / sizeof(std::uint32_t));

			log::println(log::category::render, "RT shadow: building BLAS mesh_ptr={} vertices={} indices={}",
				reinterpret_cast<std::uint64_t>(mesh_ptr), vertex_count, index_count);

			if (vertex_count == 0 || index_count == 0) {
				log::println(log::level::warning, log::category::render, "RT shadow: skipping BLAS — empty buffers");
				continue;
			}

			fd.blas_cache[mesh_ptr] = gpu::build_blas(gpu.device_ref(), {
				.vertex_buffer = &m.vertex_gpu_buffer(),
				.vertex_count  = vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer  = &m.index_gpu_buffer(),
				.index_count   = index_count
			});

			log::println(log::category::render, "RT shadow: BLAS built device_address={:#x}",
				fd.blas_cache[mesh_ptr].device_address());

			any_new_blas = true;
		}
	}

	if (any_new_blas) {
		log::println(log::category::render, "RT shadow: wait_idle for BLAS completion");
		gpu.device_ref().wait_idle();
	}

	std::vector<gpu::acceleration_structure_instance> instances;
	instances.reserve(data.render_queue.size());

	for (const auto& entry : data.render_queue) {
		const auto* mdl = entry.model.resolve();
		if (!mdl || entry.index >= mdl->meshes().size()) {
			continue;
		}

		const auto* mesh_ptr = &mdl->meshes()[entry.index];
		const auto it = fd.blas_cache.find(mesh_ptr);
		if (it == fd.blas_cache.end()) {
			continue;
		}

		std::uint32_t palette_idx = 0;
		if (const auto palette_it = data.material_palette_map.find(&mesh_ptr->material()); palette_it != data.material_palette_map.end()) {
			palette_idx = palette_it->second;
		}

		instances.push_back({
			.transform    = entry.model_matrix,
			.custom_index = palette_idx,
			.cull_disable = true,
			.blas_address = it->second.device_address()
		});

		if (instances.size() >= max_instances) {
			break;
		}
	}

	auto pass = gpu.graph().add_pass<state>();
	pass.record([&fd, &gpu, instances = std::move(instances), frame_index](vulkan::recording_context& record_ctx) mutable {
		gpu::rebuild_tlas(gpu.device_ref(), fd.tlas_per_frame[frame_index], instances, record_ctx);
	});
}
