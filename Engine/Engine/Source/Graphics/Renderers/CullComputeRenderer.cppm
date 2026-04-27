export module gse.graphics:cull_compute_renderer;

import std;

import :geometry_collector;
import :skin_compute_renderer;
import :camera_system;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::renderer::cull_compute {
	struct state {
		bool enabled = true;
	};

	struct system {
		struct resources {
			resource::handle<shader> shader_handle;
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> normal_descriptors;
			per_frame_resource<gpu::descriptor_region> skinned_descriptors;
			per_frame_resource<gpu::buffer> frustum_buffer;
			per_frame_resource<gpu::buffer> batch_info_buffer;
			std::uint32_t batch_stride = 0;
			std::unordered_map<std::string, std::uint32_t> batch_offsets;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			const resources& r,
			const state& s,
			const geometry_collector::state& gc_s
		) -> async::task<>;
	};
}

auto gse::renderer::cull_compute::system::initialize(const init_context& phase, resources& r, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& assets = *static_cast<asset_registry<gpu::context>*>(phase.assets_ptr);

	r.shader_handle = assets.get<shader>("Shaders/Compute/cull_instances");
	assets.instantly_load(r.shader_handle);

	if (!r.shader_handle.valid() || !r.shader_handle->is_compute()) {
		s.enabled = false;
		return;
	}

	const auto batch_block = r.shader_handle->uniform_block("batches");
	r.batch_stride = batch_block.size;
	for (const auto& [name, member] : batch_block.members) {
		r.batch_offsets[name] = member.offset;
	}

	const auto* gc_r = phase.try_resources_of<geometry_collector::system::resources>();

	r.pipeline = gpu::create_compute_pipeline(ctx, *r.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr std::size_t frustum_size = sizeof(std::array<vec4f, 6>);
		r.frustum_buffer[i] = gpu::create_buffer(ctx, {
			.size = frustum_size,
			.usage = gpu::buffer_flag::uniform | gpu::buffer_flag::transfer_dst
		});

		const std::size_t batch_info_size = geometry_collector::render_data::max_batches * 2 * r.batch_stride;
		r.batch_info_buffer[i] = gpu::create_buffer(ctx, {
			.size = batch_info_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.normal_descriptors[i] = gpu::allocate_descriptors(ctx, *r.shader_handle);
		r.skinned_descriptors[i] = gpu::allocate_descriptors(ctx, *r.shader_handle);
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		auto write_shared = [&](gpu::descriptor_writer& w) -> gpu::descriptor_writer& {
			return w.buffer("FrustumUBO", r.frustum_buffer[i], 0, sizeof(std::array<vec4f, 6>))
				.buffer("instances", gc_r->instance_buffer[i])
				.buffer("batches", r.batch_info_buffer[i]);
		};

		gpu::descriptor_writer normal_writer(ctx, r.shader_handle, r.normal_descriptors[i]);
		write_shared(normal_writer)
			.buffer("indirectCommands", gc_r->normal_indirect_commands_buffer[i])
			.commit();

		gpu::descriptor_writer skinned_writer(ctx, r.shader_handle, r.skinned_descriptors[i]);
		write_shared(skinned_writer)
			.buffer("indirectCommands", gc_r->skinned_indirect_commands_buffer[i])
			.commit();
	}
}

auto gse::renderer::cull_compute::system::frame(frame_context& ctx, const resources& r, const state& s, const geometry_collector::state& gc_s) -> async::task<> {
	auto& gpu = ctx.get<gpu::context>();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();
	if (!gc_r) {
		co_return;
	}

	const auto& normal_batches = data.normal_batches;
	const auto& skinned_batches = data.skinned_batches;
	if (normal_batches.empty() && skinned_batches.empty()) {
		co_return;
	}

	const auto frame_index = gpu.graph().current_frame();

	if (s.enabled) {
		const auto* cam_state = ctx.try_state_of<camera::state>();
		const auto view_matrix = cam_state ? cam_state->view_matrix : gse::view_matrix{};
		const auto proj_matrix = cam_state ? cam_state->projection_matrix : projection_matrix{};
		const auto view_proj = proj_matrix * view_matrix;
		const auto frustum = extract_frustum_planes(view_proj);

		gse::memcpy(r.frustum_buffer[frame_index].mapped(), frustum);

		std::byte* batch_data = r.batch_info_buffer[frame_index].mapped();

		auto write_batch_info = [&](const auto& batch, const std::size_t index) {
			std::byte* offset = batch_data + (index * r.batch_stride);

			gse::memcpy(offset + r.batch_offsets.at("first_instance"), batch.first_instance);
			gse::memcpy(offset + r.batch_offsets.at("instance_count"), batch.instance_count);
			gse::memcpy(offset + r.batch_offsets.at("aabb_min"), batch.world_aabb_min);
			gse::memcpy(offset + r.batch_offsets.at("aabb_max"), batch.world_aabb_max);
		};

		std::size_t batch_index = 0;
		for (const auto& batch : normal_batches) {
			write_batch_info(batch, batch_index++);
		}
		for (const auto& batch : skinned_batches) {
			write_batch_info(batch, batch_index++);
		}
	}

	auto pass = gpu.graph().add_pass<state>();

	if (!normal_batches.empty()) {
		pass.track(gc_r->normal_indirect_commands_buffer[frame_index]);
	}
	if (!skinned_batches.empty()) {
		pass.track(gc_r->skinned_indirect_commands_buffer[frame_index]);
	}

	if (s.enabled) {
		pass.track(r.frustum_buffer[frame_index]);
		pass.track(r.batch_info_buffer[frame_index]);
		pass.after<skin_compute::state>();

		if (!normal_batches.empty()) {
			pass.writes(gpu::storage_write(gc_r->normal_indirect_commands_buffer[frame_index], gpu::pipeline_stage::compute_shader));
		}
		if (!skinned_batches.empty()) {
			pass.writes(gpu::storage_write(gc_r->skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::compute_shader));
		}
	}

	auto& rec = co_await pass.record();

	if (!s.enabled) {
		co_return;
	}

	rec.bind(r.pipeline);

	if (!normal_batches.empty()) {
		auto normal_pc = r.shader_handle->cache_push_block("push_constants");
		normal_pc.set("batch_offset", 0u);

		rec.bind_descriptors(r.pipeline, r.normal_descriptors[frame_index]);
		rec.push(r.pipeline, normal_pc);
		rec.dispatch(static_cast<std::uint32_t>(normal_batches.size()), 1, 1);
	}

	if (!skinned_batches.empty()) {
		auto skinned_pc = r.shader_handle->cache_push_block("push_constants");
		skinned_pc.set("batch_offset", static_cast<std::uint32_t>(normal_batches.size()));

		rec.bind_descriptors(r.pipeline, r.skinned_descriptors[frame_index]);
		rec.push(r.pipeline, skinned_pc);
		rec.dispatch(static_cast<std::uint32_t>(skinned_batches.size()), 1, 1);
	}
}
