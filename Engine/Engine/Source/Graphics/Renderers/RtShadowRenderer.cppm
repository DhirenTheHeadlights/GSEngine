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

	struct render_state {
		std::unordered_map<const mesh*, gpu::blas> blas_cache;
		per_frame_resource<gpu::tlas> tlas_per_frame;
	};

	struct system {
		static auto initialize(
			const initialize_phase& phase,
			state& s,
			render_state& rs
		) -> void;

		static auto render(
			const render_phase& phase,
			const state& s,
			render_state& rs
		) -> void;
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

auto gse::renderer::rt_shadow::system::initialize(const initialize_phase& phase, state& s, render_state& rs) -> void {
	const auto& ctx = phase.get<gpu::context>();

	if (!ctx.device_ref().ray_tracing_enabled()) {
		log::println(log::level::warning, log::category::render, "RT shadow: ray tracing not supported, skipping");
		return;
	}

	s.initialized = true;
	log::println(log::category::render, "RT shadow: initialized");

	for (std::size_t i = 0; i < per_frame_resource<gpu::tlas>::frames_in_flight; ++i) {
		rs.tlas_per_frame[i] = gpu::build_tlas(ctx.device_ref(), max_instances);
		s.tlas_ptrs[i] = &rs.tlas_per_frame[i];
		log::println(
			log::category::render, "RT shadow: frame_slot={} tlas_handle={:#x}",
			i,
			s.tlas(static_cast<std::uint32_t>(i)).native_handle().value
		);
	}
}

auto gse::renderer::rt_shadow::system::render(const render_phase& phase, const state& s, render_state& rs) -> void {
	if (!s.initialized) {
		return;
	}

	auto& ctx = phase.get<gpu::context>();

	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	if (!ctx.graph().frame_in_progress()) {
		return;
	}

	const auto& data = render_items[0];
	const auto frame_index = ctx.graph().current_frame();

	bool any_new_blas = false;
	for (const auto& batch : data.normal_batches) {
		const auto& m = batch.key.model_ptr->meshes()[batch.key.mesh_index];

		if (const auto* mesh_ptr = &m; !rs.blas_cache.contains(mesh_ptr)) {
			const auto vertex_count = static_cast<std::uint32_t>(m.vertex_gpu_buffer().size() / sizeof(vertex));
			const auto index_count  = static_cast<std::uint32_t>(m.index_gpu_buffer().size() / sizeof(std::uint32_t));

			log::println(log::category::render, "RT shadow: building BLAS mesh_ptr={} vertices={} indices={}",
				reinterpret_cast<std::uint64_t>(mesh_ptr), vertex_count, index_count);

			if (vertex_count == 0 || index_count == 0) {
				log::println(log::level::warning, log::category::render, "RT shadow: skipping BLAS — empty buffers");
				continue;
			}

			rs.blas_cache[mesh_ptr] = gpu::build_blas(ctx.device_ref(), {
				.vertex_buffer = &m.vertex_gpu_buffer(),
				.vertex_count  = vertex_count,
				.vertex_stride = static_cast<std::uint32_t>(sizeof(vertex)),
				.index_buffer  = &m.index_gpu_buffer(),
				.index_count   = index_count
			});

			log::println(log::category::render, "RT shadow: BLAS built device_address={:#x}",
				rs.blas_cache[mesh_ptr].device_address());

			any_new_blas = true;
		}
	}

	if (any_new_blas) {
		log::println(log::category::render, "RT shadow: wait_idle for BLAS completion");
		ctx.device_ref().wait_idle();
	}

	std::vector<gpu::acceleration_structure_instance> instances;
	instances.reserve(data.render_queue.size());

	for (const auto& entry : data.render_queue) {
		const auto* mdl = entry.model.resolve();
		if (!mdl || entry.index >= mdl->meshes().size()) {
			continue;
		}

		const auto* mesh_ptr = &mdl->meshes()[entry.index];
		const auto it = rs.blas_cache.find(mesh_ptr);
		if (it == rs.blas_cache.end()) {
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

	auto pass = ctx.graph().add_pass<render_state>();
	pass.record([&rs, &ctx, instances = std::move(instances), frame_index](vulkan::recording_context& record_ctx) mutable {
		gpu::rebuild_tlas(ctx.device_ref(), rs.tlas_per_frame[frame_index], instances, record_ctx);
	});
}
