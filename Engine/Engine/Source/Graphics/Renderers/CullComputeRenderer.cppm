export module gse.graphics:cull_compute_renderer;

import std;

import :geometry_collector;
import :skin_compute_renderer;
import :camera_system;
import gse.platform;
import gse.math;
import gse.utility;

export namespace gse::renderer::cull_compute {
	struct state {
		resource::handle<shader> shader_handle;
		gpu::pipeline pipeline;
		per_frame_resource<vulkan::descriptor_region> normal_descriptors;
		per_frame_resource<vulkan::descriptor_region> skinned_descriptors;
		per_frame_resource<gpu::buffer> frustum_buffer;
		per_frame_resource<gpu::buffer> batch_info_buffer;
		std::uint32_t batch_stride = 0;
		std::unordered_map<std::string, std::uint32_t> batch_offsets;
		bool enabled = true;

		state() = default;
	};

	struct system {
		static auto initialize(
			initialize_phase& phase,
			state& s
		) -> void;

		static auto render(
			const render_phase& phase,
			const state& s
		) -> void;
	};
}

auto gse::renderer::cull_compute::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.shader_handle = ctx.get<shader>("Shaders/Compute/cull_instances");
	ctx.instantly_load(s.shader_handle);

	if (!s.shader_handle.valid() || !s.shader_handle->is_compute()) {
		s.enabled = false;
		return;
	}

	const auto batch_block = s.shader_handle->uniform_block("batches");
	s.batch_stride = batch_block.size;
	for (const auto& [name, member] : batch_block.members) {
		s.batch_offsets[name] = member.offset;
	}

	const auto* gc = phase.try_state_of<geometry_collector::state>();

	s.pipeline = gpu::create_compute_pipeline(ctx, *s.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr vk::DeviceSize frustum_size = sizeof(std::array<vec4f, 6>);
		s.frustum_buffer[i] = gpu::create_buffer(ctx, {
			.size = frustum_size,
			.usage = gpu::buffer_flag::uniform | gpu::buffer_flag::transfer_dst
		});

		const vk::DeviceSize batch_info_size = geometry_collector::state::max_batches * 2 * s.batch_stride;
		s.batch_info_buffer[i] = gpu::create_buffer(ctx, {
			.size = batch_info_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
	}

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		s.normal_descriptors[i] = gpu::allocate_descriptors(ctx, *s.shader_handle);
		s.skinned_descriptors[i] = gpu::allocate_descriptors(ctx, *s.shader_handle);
	}

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		const vk::DescriptorBufferInfo frustum_info{
			.buffer = s.frustum_buffer[i].native().buffer,
			.offset = 0,
			.range = sizeof(std::array<vec4f, 6>)
		};

		const vk::DescriptorBufferInfo instance_info{
			.buffer = gc->instance_buffer[i].native().buffer,
			.offset = 0,
			.range = gc->instance_buffer[i].size()
		};

		const vk::DescriptorBufferInfo batch_info{
			.buffer = s.batch_info_buffer[i].native().buffer,
			.offset = 0,
			.range = s.batch_info_buffer[i].size()
		};

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> shared_infos = {
			{ "FrustumUBO", frustum_info },
			{ "instances", instance_info },
			{ "batches", batch_info }
		};

		auto normal_infos = shared_infos;
		normal_infos["indirectCommands"] = {
			.buffer = gc->normal_indirect_commands_buffer[i].native().buffer,
			.offset = 0,
			.range = gc->normal_indirect_commands_buffer[i].size()
		};

		auto skinned_infos = shared_infos;
		skinned_infos["indirectCommands"] = {
			.buffer = gc->skinned_indirect_commands_buffer[i].native().buffer,
			.offset = 0,
			.range = gc->skinned_indirect_commands_buffer[i].size()
		};

		gpu::write_descriptors(ctx, s.normal_descriptors[i], *s.shader_handle, normal_infos);
		gpu::write_descriptors(ctx, s.skinned_descriptors[i], *s.shader_handle, skinned_infos);
	}
}

auto gse::renderer::cull_compute::system::render(const render_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	const auto& data = render_items[0];
	const auto frame_index = data.frame_index;

	const auto* gc = phase.try_state_of<geometry_collector::state>();
	if (!gc) {
		return;
	}

	const auto& normal_batches = data.normal_batches;
	const auto& skinned_batches = data.skinned_batches;
	const std::size_t total_batch_count = normal_batches.size() + skinned_batches.size();

	if (total_batch_count > 0 && s.enabled) {
		const auto* cam_state = phase.try_state_of<camera::state>();
		const auto view_matrix = cam_state ? cam_state->view_matrix : gse::view_matrix{};
		const auto proj_matrix = cam_state ? cam_state->projection_matrix : projection_matrix{};
		const auto view_proj = proj_matrix * view_matrix;
		const auto frustum = extract_frustum_planes(view_proj);

		gse::memcpy(s.frustum_buffer[frame_index].mapped(), frustum);

		std::byte* batch_data = s.batch_info_buffer[frame_index].mapped();

		auto write_batch_info = [&](const auto& batch, const std::size_t index) {
			std::byte* offset = batch_data + (index * s.batch_stride);

			gse::memcpy(offset + s.batch_offsets.at("first_instance"), batch.first_instance);
			gse::memcpy(offset + s.batch_offsets.at("instance_count"), batch.instance_count);
			gse::memcpy(offset + s.batch_offsets.at("aabb_min"), batch.world_aabb_min);
			gse::memcpy(offset + s.batch_offsets.at("aabb_max"), batch.world_aabb_max);
		};

		std::size_t batch_index = 0;
		for (const auto& batch : normal_batches) {
			write_batch_info(batch, batch_index++);
		}
		for (const auto& batch : skinned_batches) {
			write_batch_info(batch, batch_index++);
		}
	}

	auto& graph = ctx.graph();

	if (s.enabled) {
		if (!normal_batches.empty()) {
			auto cull_pc = s.shader_handle->cache_push_block("push_constants");
			cull_pc.set("batch_offset", 0u);

			auto normal_pass = graph.add_pass<state>();
			normal_pass.track(s.frustum_buffer[frame_index].native());
			normal_pass.track(s.batch_info_buffer[frame_index].native());

			normal_pass
				.writes(vulkan::storage(gc->normal_indirect_commands_buffer[frame_index].native(), vk::PipelineStageFlagBits2::eComputeShader))
				.record([&s, frame_index, batch_count = static_cast<std::uint32_t>(normal_batches.size()), cull_pc = std::move(cull_pc)](vulkan::recording_context& ctx) {
					ctx.bind(s.pipeline);
					ctx.bind_descriptors(s.pipeline, s.normal_descriptors[frame_index]);
					ctx.push(s.pipeline, cull_pc);
					ctx.dispatch(batch_count, 1, 1);
				});
		}

		if (!skinned_batches.empty()) {
			auto cull_pc = s.shader_handle->cache_push_block("push_constants");
			cull_pc.set("batch_offset", static_cast<std::uint32_t>(normal_batches.size()));

			auto skinned_pass = graph.add_pass<state>();
			skinned_pass.track(s.frustum_buffer[frame_index].native());
			skinned_pass.track(s.batch_info_buffer[frame_index].native());

			skinned_pass
				.after<skin_compute::state>()
				.writes(vulkan::storage(gc->skinned_indirect_commands_buffer[frame_index].native(), vk::PipelineStageFlagBits2::eComputeShader))
				.record([&s, frame_index, batch_count = static_cast<std::uint32_t>(skinned_batches.size()), cull_pc = std::move(cull_pc)](vulkan::recording_context& ctx) {
					ctx.bind(s.pipeline);
					ctx.bind_descriptors(s.pipeline, s.skinned_descriptors[frame_index]);
					ctx.push(s.pipeline, cull_pc);
					ctx.dispatch(batch_count, 1, 1);
				});
		}
	} else {
		if (!normal_batches.empty()) {
			auto normal_upload = graph.add_pass<state>();
			normal_upload.track(gc->normal_indirect_commands_buffer[frame_index].native());
			normal_upload.record([](vulkan::recording_context&) {});
		}
		if (!skinned_batches.empty()) {
			auto skinned_upload = graph.add_pass<state>();
			skinned_upload.track(gc->skinned_indirect_commands_buffer[frame_index].native());
			skinned_upload.record([](vulkan::recording_context&) {});
		}
	}
}
