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
		gpu::context* ctx = nullptr;
		resource::handle<shader> shader_handle;
		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> normal_descriptor_sets;
		per_frame_resource<vk::raii::DescriptorSet> skinned_descriptor_sets;
		per_frame_resource<vulkan::buffer_resource> frustum_buffer;
		per_frame_resource<vulkan::buffer_resource> batch_info_buffer;
		std::uint32_t batch_stride = 0;
		std::unordered_map<std::string, std::uint32_t> batch_offsets;
		bool enabled = true;

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

auto gse::renderer::cull_compute::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& config = s.ctx->config();

	s.shader_handle = s.ctx->get<shader>("Shaders/Compute/cull_instances");
	s.ctx->instantly_load(s.shader_handle);

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

	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		constexpr vk::DeviceSize frustum_size = sizeof(std::array<vec4f, 6>);
		s.frustum_buffer[i] = config.allocator().create_buffer({
			.size = frustum_size,
			.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst
		});

		const vk::DeviceSize batch_info_size = geometry_collector::state::max_batches * 2 * s.batch_stride;
		s.batch_info_buffer[i] = config.allocator().create_buffer({
			.size = batch_info_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.normal_descriptor_sets[i] = s.shader_handle->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);
		s.skinned_descriptor_sets[i] = s.shader_handle->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);
	}

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		const vk::DescriptorBufferInfo frustum_info{
			.buffer = s.frustum_buffer[i].buffer,
			.offset = 0,
			.range = sizeof(std::array<vec4f, 6>)
		};

		const vk::DescriptorBufferInfo instance_info{
			.buffer = gc->instance_buffer[i].buffer,
			.offset = 0,
			.range = vk::WholeSize
		};

		const vk::DescriptorBufferInfo batch_info{
			.buffer = s.batch_info_buffer[i].buffer,
			.offset = 0,
			.range = vk::WholeSize
		};

		const vk::DescriptorBufferInfo normal_indirect_info{
			.buffer = gc->normal_indirect_commands_buffer[i].buffer,
			.offset = 0,
			.range = vk::WholeSize
		};

		const vk::DescriptorBufferInfo skinned_indirect_info{
			.buffer = gc->skinned_indirect_commands_buffer[i].buffer,
			.offset = 0,
			.range = vk::WholeSize
		};

		std::array<vk::WriteDescriptorSet, 4> normal_writes{
			vk::WriteDescriptorSet{
				.dstSet = *s.normal_descriptor_sets[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &frustum_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.normal_descriptor_sets[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &instance_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.normal_descriptor_sets[i],
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &batch_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.normal_descriptor_sets[i],
				.dstBinding = 3,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &normal_indirect_info
			}
		};

		std::array<vk::WriteDescriptorSet, 4> skinned_writes{
			vk::WriteDescriptorSet{
				.dstSet = *s.skinned_descriptor_sets[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &frustum_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.skinned_descriptor_sets[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &instance_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.skinned_descriptor_sets[i],
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &batch_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.skinned_descriptor_sets[i],
				.dstBinding = 3,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skinned_indirect_info
			}
		};

		config.device_config().device.updateDescriptorSets(normal_writes, nullptr);
		config.device_config().device.updateDescriptorSets(skinned_writes, nullptr);
	}

	auto culling_layouts = s.shader_handle->layouts();
	std::vector culling_ranges = { s.shader_handle->push_constant_range("push_constants") };

	s.pipeline_layout = config.device_config().device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(culling_layouts.size()),
		.pSetLayouts = culling_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(culling_ranges.size()),
		.pPushConstantRanges = culling_ranges.data()
	});

	s.pipeline = config.device_config().device.createComputePipeline(nullptr, {
		.stage = s.shader_handle->compute_stage(),
		.layout = *s.pipeline_layout
	});
}

auto gse::renderer::cull_compute::system::render(const render_phase& phase, const state& s) -> void {
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

		gse::memcpy(s.frustum_buffer[frame_index].allocation.mapped(), frustum);

		std::byte* batch_data = s.batch_info_buffer[frame_index].allocation.mapped();

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

	auto& graph = s.ctx->graph();

	if (s.enabled) {
		if (!normal_batches.empty()) {
			auto cull_pc = s.shader_handle->cache_push_block("push_constants");
			cull_pc.set("batch_offset", 0u);

			const auto& dc = s.normal_descriptor_sets[frame_index];

			auto normal_pass = graph.add_pass<state>();
			normal_pass.track(s.frustum_buffer[frame_index]);
			normal_pass.track(s.batch_info_buffer[frame_index]);

			normal_pass
				.writes(vulkan::storage(gc->normal_indirect_commands_buffer[frame_index], vk::PipelineStageFlagBits2::eComputeShader))
				.record([&s, frame_index, batch_count = static_cast<std::uint32_t>(normal_batches.size()), &dc, cull_pc = std::move(cull_pc)](vulkan::recording_context& ctx) {
					ctx.bind_pipeline(vk::PipelineBindPoint::eCompute, *s.pipeline);
					const vk::DescriptorSet sets[]{ *dc.value() };
					ctx.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, *s.pipeline_layout, 0, sets);
					ctx.push(cull_pc, *s.pipeline_layout);
					ctx.dispatch(batch_count, 1, 1);
				});
		}

		if (!skinned_batches.empty()) {
			auto cull_pc = s.shader_handle->cache_push_block("push_constants");
			cull_pc.set("batch_offset", static_cast<std::uint32_t>(normal_batches.size()));

			const auto& dc = s.skinned_descriptor_sets[frame_index];

			auto skinned_pass = graph.add_pass<state>();
			skinned_pass.track(s.frustum_buffer[frame_index]);
			skinned_pass.track(s.batch_info_buffer[frame_index]);

			skinned_pass
				.after<skin_compute::state>()
				.writes(vulkan::storage(gc->skinned_indirect_commands_buffer[frame_index], vk::PipelineStageFlagBits2::eComputeShader))
				.record([&s, frame_index, batch_count = static_cast<std::uint32_t>(skinned_batches.size()), &dc, cull_pc = std::move(cull_pc)](vulkan::recording_context& ctx) {
					ctx.bind_pipeline(vk::PipelineBindPoint::eCompute, *s.pipeline);
					const vk::DescriptorSet sets[]{ *dc.value() };
					ctx.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, *s.pipeline_layout, 0, sets);
					ctx.push(cull_pc, *s.pipeline_layout);
					ctx.dispatch(batch_count, 1, 1);
				});
		}
	} else {
		if (!normal_batches.empty()) {
			auto normal_upload = graph.add_pass<state>();
			normal_upload.track(gc->normal_indirect_commands_buffer[frame_index]);
			normal_upload.record([](vulkan::recording_context&) {});
		}
		if (!skinned_batches.empty()) {
			auto skinned_upload = graph.add_pass<state>();
			skinned_upload.track(gc->skinned_indirect_commands_buffer[frame_index]);
			skinned_upload.record([](vulkan::recording_context&) {});
		}
	}
}
