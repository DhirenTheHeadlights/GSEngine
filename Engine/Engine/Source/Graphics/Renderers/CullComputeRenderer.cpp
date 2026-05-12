module gse.graphics;

import std;

import :cull_compute_renderer;
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

auto gse::renderer::cull_compute::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, const geometry_collector::system::resources& gc_r, resources& r, state& s) -> async::task<> {
	r.shader_handle = co_await asset::load<shader>(ctx, "Shaders/Compute/cull_instances");

	if (!r.shader_handle.valid() || !r.shader_handle->is_compute()) {
		s.enabled = false;
		co_return;
	}

	r.batch_layout = layout_of(r.shader_handle->uniform_block("batches"));

	r.pipeline = gpu::create_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, r.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr std::size_t frustum_size = sizeof(std::array<vec4f, 6>);
		r.frustum_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = frustum_size,
			.usage = gpu::buffer_flag::uniform | gpu::buffer_flag::transfer_dst
		});

		const std::size_t batch_info_size = geometry_collector::render_data::max_batches * 2 * r.batch_layout.stride;
		r.batch_info_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = batch_info_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.normal_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);
		r.skinned_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		auto write_shared = [&](gpu::descriptor_writer& w) -> gpu::descriptor_writer& {
			return w.buffer("FrustumUBO", r.frustum_buffer[i], 0, sizeof(std::array<vec4f, 6>))
				.buffer("batches", r.batch_info_buffer[i]);
		};

		gpu::descriptor_writer normal_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.normal_descriptors[i]);
		write_shared(normal_writer)
			.buffer("indirectCommands", gc_r.normal_indirect_commands_buffer[i])
			.commit();

		gpu::descriptor_writer skinned_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.skinned_descriptors[i]);
		write_shared(skinned_writer)
			.buffer("indirectCommands", gc_r.skinned_indirect_commands_buffer[i])
			.commit();
	}

	co_return;
}

auto gse::renderer::cull_compute::system::frame(frame_context& ctx, const gpu::context::state& gpu_s, const geometry_collector::system::resources& gc_r, const resources& r, const state& s) -> async::task<> {
	if (!s.enabled) {
		co_return;
	}

	const auto& items = ctx.read_channel<geometry_collector::render_data>();
	if (items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto normal_count = static_cast<std::uint32_t>(data.normal_batches.size());
	const auto skinned_count = static_cast<std::uint32_t>(data.skinned_batches.size());

	if (normal_count == 0 && skinned_count == 0) {
		co_return;
	}

	const view_projection_matrix view_proj = data.proj * data.view;
	const auto planes = extract_frustum_planes(view_proj);
	gse::memcpy(r.frustum_buffer[frame_index].mapped(), planes);

	const auto& min_offset = r.batch_layout.offsets.at("aabb_min");
	const auto& max_offset = r.batch_layout.offsets.at("aabb_max");
	const auto& first_instance_offset = r.batch_layout.offsets.at("first_instance");
	const auto& instance_count_offset = r.batch_layout.offsets.at("instance_count");

	std::vector<std::byte> batch_staging((normal_count + skinned_count) * r.batch_layout.stride, std::byte{ 0 });

	auto write_batch = [&](std::size_t index, std::uint32_t first_instance, std::uint32_t instance_count, const vec3<length>& aabb_min, const vec3<length>& aabb_max) {
		std::byte* dst = batch_staging.data() + index * r.batch_layout.stride;
		gse::memcpy(dst + first_instance_offset, first_instance);
		gse::memcpy(dst + instance_count_offset, instance_count);
		gse::memcpy(dst + min_offset, aabb_min);
		gse::memcpy(dst + max_offset, aabb_max);
	};

	for (std::size_t i = 0; i < data.normal_batches.size(); ++i) {
		const auto& b = data.normal_batches[i];
		write_batch(i, b.first_instance, b.instance_count, b.world_aabb_min, b.world_aabb_max);
	}
	for (std::size_t i = 0; i < data.skinned_batches.size(); ++i) {
		const auto& b = data.skinned_batches[i];
		write_batch(normal_count + i, b.first_instance, b.instance_count, b.world_aabb_min, b.world_aabb_max);
	}

	if (!batch_staging.empty()) {
		gse::memcpy(r.batch_info_buffer[frame_index].mapped(), batch_staging.data(), batch_staging.size());
	}

	auto rec = co_await gpu::pass<system>(ctx)
		.writes(
			gpu::storage_write(gc_r.normal_indirect_commands_buffer[frame_index], gpu::pipeline_stage::compute_shader),
			gpu::storage_write(gc_r.skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::compute_shader)
		)
		.tracks(r.frustum_buffer[frame_index], r.batch_info_buffer[frame_index]);

	rec.bind(r.pipeline);

	if (normal_count > 0) {
		rec.bind_descriptors(r.pipeline, r.normal_descriptors[frame_index]);
		auto pc = gpu::cache_push_block(r.shader_handle, "push_constants");
		pc.set("batch_offset", static_cast<std::uint32_t>(0));
		pc.set("indirect_stride", static_cast<std::uint32_t>(sizeof(gpu::draw_mesh_tasks_indirect_command)));
		rec.push(r.pipeline, pc);
		rec.dispatch(normal_count, 1, 1);
	}

	if (skinned_count > 0) {
		rec.bind_descriptors(r.pipeline, r.skinned_descriptors[frame_index]);
		auto pc = gpu::cache_push_block(r.shader_handle, "push_constants");
		pc.set("batch_offset", normal_count);
		pc.set("indirect_stride", static_cast<std::uint32_t>(sizeof(gpu::draw_indexed_indirect_command)));
		rec.push(r.pipeline, pc);
		rec.dispatch(skinned_count, 1, 1);
	}
}
