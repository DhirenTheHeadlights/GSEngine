export module gse.graphics:skin_compute_renderer;

import std;

import :geometry_collector;
import gse.platform;
import gse.utility;

export namespace gse::renderer::skin_compute {
	struct state {
		gpu::context* ctx = nullptr;
		resource::handle<shader> shader_handle;
		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;

		explicit state(gpu::context& c) : ctx(std::addressof(c)) {}
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
	auto& config = s.ctx->config();

	s.shader_handle = s.ctx->get<shader>("Shaders/Compute/skin_compute");
	s.ctx->instantly_load(s.shader_handle);

	assert(s.shader_handle->is_compute(), std::source_location::current(), "Skin compute shader is not loaded as a compute shader");

	const auto* gc = phase.try_state_of<geometry_collector::state>();

	auto compute_layouts = s.shader_handle->layouts();
	std::vector compute_ranges = { s.shader_handle->push_constant_range("push_constants") };

	s.pipeline_layout = config.device_config().device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(compute_layouts.size()),
		.pSetLayouts = compute_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(compute_ranges.size()),
		.pPushConstantRanges = compute_ranges.data()
	});

	s.pipeline = config.device_config().device.createComputePipeline(nullptr, {
		.stage = s.shader_handle->compute_stage(),
		.layout = *s.pipeline_layout
	});

	constexpr vk::DeviceSize skin_buffer_size = geometry_collector::state::max_skin_matrices * sizeof(mat4f);
	constexpr vk::DeviceSize local_pose_size = geometry_collector::state::max_skin_matrices * sizeof(mat4f);

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.descriptor_sets[i] = s.shader_handle->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		const vk::DescriptorBufferInfo skeleton_info{
			.buffer = gc->skeleton_buffer.buffer,
			.offset = 0,
			.range = geometry_collector::state::max_joints * gc->joint_stride
		};

		const vk::DescriptorBufferInfo local_pose_info{
			.buffer = gc->local_pose_buffer[i].buffer,
			.offset = 0,
			.range = local_pose_size
		};

		const vk::DescriptorBufferInfo skin_info{
			.buffer = gc->skin_buffer[i].buffer,
			.offset = 0,
			.range = skin_buffer_size
		};

		std::array writes{
			vk::WriteDescriptorSet{
				.dstSet = *s.descriptor_sets[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skeleton_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.descriptor_sets[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &local_pose_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.descriptor_sets[i],
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skin_info
			}
		};

		config.device_config().device.updateDescriptorSets(writes, nullptr);
	}
}

auto gse::renderer::skin_compute::system::render(const render_phase& phase, const state& s) -> void {
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

	auto pass = s.ctx->graph().add_pass<state>();
	pass.when(data.pending_compute_instance_count > 0);

	pass.track(gc->skeleton_buffer);
	pass.track(gc->local_pose_buffer[frame_index]);

	pass.writes(vulkan::storage(gc->skin_buffer[frame_index], vk::PipelineStageFlagBits2::eComputeShader))
		.record([&s, frame_index, instance_count = data.pending_compute_instance_count, skin_pc = std::move(skin_pc)](vulkan::recording_context& ctx) {
			ctx.bind_pipeline(vk::PipelineBindPoint::eCompute, *s.pipeline);
			const vk::DescriptorSet sets[]{ *s.descriptor_sets[frame_index] };
			ctx.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, *s.pipeline_layout, 0, sets);
			ctx.push(skin_pc, *s.pipeline_layout);
			ctx.dispatch(instance_count, 1, 1);
		});
}
