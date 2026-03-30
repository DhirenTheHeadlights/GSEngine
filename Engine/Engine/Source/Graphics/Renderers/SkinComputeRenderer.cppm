export module gse.graphics:skin_compute_renderer;

import std;

import :geometry_collector;
import gse.platform;
import gse.utility;

export namespace gse::renderer::skin_compute {
	struct state {
		resource::handle<shader> shader_handle;
		gpu::pipeline pipeline;
		per_frame_resource<vulkan::descriptor_region> descriptors;

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

auto gse::renderer::skin_compute::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.shader_handle = ctx.get<shader>("Shaders/Compute/skin_compute");
	ctx.instantly_load(s.shader_handle);

	assert(s.shader_handle->is_compute(), std::source_location::current(), "Skin compute shader is not loaded as a compute shader");

	const auto* gc = phase.try_state_of<geometry_collector::state>();

	s.pipeline = gpu::create_compute_pipeline(ctx, *s.shader_handle, "push_constants");

	constexpr vk::DeviceSize skin_buffer_size = geometry_collector::state::max_skin_matrices * sizeof(mat4f);
	constexpr vk::DeviceSize local_pose_size = geometry_collector::state::max_skin_matrices * sizeof(mat4f);

	for (std::size_t i = 0; i < per_frame_resource<vulkan::descriptor_region>::frames_in_flight; ++i) {
		s.descriptors[i] = gpu::allocate_descriptors(ctx, *s.shader_handle);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
			{
				"skeletonData",
				{
					.buffer = gc->skeleton_buffer.buffer,
					.offset = 0,
					.range = geometry_collector::state::max_joints * gc->joint_stride
				}
			},
			{
				"localPoses",
				{
					.buffer = gc->local_pose_buffer[i].buffer,
					.offset = 0,
					.range = local_pose_size
				}
			},
			{
				"skinMatrices",
				{
					.buffer = gc->skin_buffer[i].buffer,
					.offset = 0,
					.range = skin_buffer_size
				}
			}
		};

		gpu::write_descriptors(ctx, s.descriptors[i], *s.shader_handle, buffer_infos);
	}
}

auto gse::renderer::skin_compute::system::render(const render_phase& phase, const state& s) -> void {
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

	auto skin_pc = s.shader_handle->cache_push_block("push_constants");
	skin_pc.set("joint_count", gc->current_joint_count);
	skin_pc.set("instance_count", data.pending_compute_instance_count);
	skin_pc.set("local_pose_stride", gc->current_joint_count);
	skin_pc.set("skin_stride", gc->current_joint_count);

	auto pass = ctx.graph().add_pass<state>();
	pass.when(data.pending_compute_instance_count > 0);

	pass.track(gc->skeleton_buffer);
	pass.track(gc->local_pose_buffer[frame_index]);

	pass.writes(vulkan::storage(gc->skin_buffer[frame_index], vk::PipelineStageFlagBits2::eComputeShader))
		.record([&s, frame_index, instance_count = data.pending_compute_instance_count, skin_pc = std::move(skin_pc)](vulkan::recording_context& ctx) {
			ctx.bind_pipeline(vk::PipelineBindPoint::eCompute, s.pipeline);
			ctx.bind_descriptors(vk::PipelineBindPoint::eCompute, s.pipeline, s.descriptors[frame_index]);
			ctx.push(s.pipeline, skin_pc);
			ctx.dispatch(instance_count, 1, 1);
		});
}
