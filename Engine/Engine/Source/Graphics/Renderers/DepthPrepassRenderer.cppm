export module gse.graphics:depth_prepass_renderer;

import std;

import :geometry_collector;
import :cull_compute_renderer;
import :camera_system;

import gse.platform;
import gse.utility;

export namespace gse::renderer::depth_prepass {
	struct state {
		gpu::pipeline meshlet_pipeline;
		per_frame_resource<gpu::descriptor_set> meshlet_descriptor_sets;
		resource::handle<shader> meshlet_shader;

		gpu::pipeline skinned_pipeline;
		per_frame_resource<gpu::descriptor_set> skinned_descriptor_sets;
		resource::handle<shader> skinned_shader;

		std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;

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

auto gse::renderer::depth_prepass::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	auto* gpu = &ctx;
	auto transition_depth = [gpu]() {
		gpu->add_transient_work([gpu](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
			auto& depth = gpu->graph().depth_image();
			const vk::ImageMemoryBarrier2 barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = depth.image,
				.subresourceRange = {
					.aspectMask = vk::ImageAspectFlagBits::eDepth,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			const vk::DependencyInfo dep{
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &barrier
			};

			cmd.pipelineBarrier2(dep);
			const_cast<vulkan::image_resource&>(depth).current_layout = vk::ImageLayout::eGeneral;

			return {};
		});
	};

	transition_depth();
	ctx.on_swap_chain_recreate([gpu](vulkan::config&) {
		gpu->add_transient_work([gpu](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
			auto& depth = gpu->graph().depth_image();
			const vk::ImageMemoryBarrier2 barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = depth.image,
				.subresourceRange = {
					.aspectMask = vk::ImageAspectFlagBits::eDepth,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};
			const vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
			cmd.pipelineBarrier2(dep);
			const_cast<vulkan::image_resource&>(depth).current_layout = vk::ImageLayout::eGeneral;
			return {};
		});
	});

	s.meshlet_shader = ctx.get<shader>("Shaders/Standard3D/meshlet_depth_only");
	ctx.instantly_load(s.meshlet_shader);

	const auto meshlet_camera_ubo = s.meshlet_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx, {
			.size = meshlet_camera_ubo.size,
			.usage = gpu::buffer_usage::uniform
		});
	}

	s.meshlet_pipeline = gpu::create_graphics_pipeline(ctx, *s.meshlet_shader, {
		.depth = { .test = true, .write = true, .compare = gpu::compare_op::less },
		.color = gpu::color_format::none,
		.push_constant_block = "push_constants"
	});

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_set>::frames_in_flight; ++i) {
		s.meshlet_descriptor_sets[i] = gpu::allocate_descriptor_set(ctx, *s.meshlet_shader);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> meshlet_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].native().buffer,
					.offset = 0,
					.range = meshlet_camera_ubo.size
				}
			}
		};

		gpu::update_descriptors(ctx, s.meshlet_descriptor_sets[i], *s.meshlet_shader, meshlet_buffer_infos);
	}

	s.skinned_shader = ctx.get<shader>("Shaders/Standard3D/skinned_depth_only");
	ctx.instantly_load(s.skinned_shader);

	s.skinned_pipeline = gpu::create_graphics_pipeline(ctx, *s.skinned_shader, {
		.depth = { .test = true, .write = true, .compare = gpu::compare_op::less },
		.color = gpu::color_format::none
	});

	const auto skinned_camera_ubo = s.skinned_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_set>::frames_in_flight; ++i) {
		s.skinned_descriptor_sets[i] = gpu::allocate_descriptor_set(ctx, *s.skinned_shader);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> skinned_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].native().buffer,
					.offset = 0,
					.range = skinned_camera_ubo.size
				}
			}
		};

		gpu::update_descriptors(ctx, s.skinned_descriptor_sets[i], *s.skinned_shader, skinned_buffer_infos);
	}
}

auto gse::renderer::depth_prepass::system::render(const render_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	if (!ctx.graph().frame_in_progress()) {
		return;
	}

	const auto& data = render_items[0];
	const auto frame_index = data.frame_index;

	const auto* gc_state = phase.try_state_of<geometry_collector::state>();
	if (!gc_state) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};

	const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index].native().allocation;
	s.meshlet_shader->set_uniform("CameraUBO.view", view, cam_alloc);
	s.meshlet_shader->set_uniform("CameraUBO.proj", proj, cam_alloc);
	s.skinned_shader->set_uniform("CameraUBO.view", view, cam_alloc);
	s.skinned_shader->set_uniform("CameraUBO.proj", proj, cam_alloc);

	const auto ext = ctx.graph().extent();
	const auto ext_w = ext.x();
	const auto ext_h = ext.y();

	auto pass = ctx.graph().add_pass<state>();
	pass.track(s.ubo_allocations.at("CameraUBO")[frame_index].native());
	pass.track(gc_state->instance_buffer[frame_index]);

	pass.after<cull_compute::state>()
		.reads(
			vulkan::storage_read(gc_state->skin_buffer[frame_index], vk::PipelineStageFlagBits2::eVertexShader),
			vulkan::indirect_read(gc_state->skinned_indirect_commands_buffer[frame_index], vk::PipelineStageFlagBits2::eDrawIndirect)
		)
		.depth_output(vulkan::depth_clear{ 1.0f })
		.record([&s, gc_state, &data, frame_index, ext_w, ext_h](vulkan::recording_context& ctx) {
			ctx.set_viewport(0.0f, 0.0f, static_cast<float>(ext_w), static_cast<float>(ext_h));
			ctx.set_scissor(0, 0, ext_w, ext_h);

			if (!data.normal_batches.empty()) {
				ctx.bind_pipeline(vk::PipelineBindPoint::eGraphics, s.meshlet_pipeline);
				ctx.bind_descriptor_set(vk::PipelineBindPoint::eGraphics, s.meshlet_pipeline, 0, s.meshlet_descriptor_sets[frame_index]);

				const vk::DescriptorBufferInfo instance_buffer_info{
					.buffer = gc_state->instance_buffer[frame_index].buffer,
					.offset = 0,
					.range = gc_state->instance_buffer[frame_index].allocation.size()
				};

				for (const auto& batch : data.normal_batches) {
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];
					if (!mesh.has_meshlets()) {
						continue;
					}

					const vk::DescriptorBufferInfo vertex_storage_info{ .buffer = mesh.vertex_storage_buffer(), .offset = 0, .range = vk::WholeSize };
					const vk::DescriptorBufferInfo meshlet_desc_info{ .buffer = mesh.meshlet_descriptor_buffer(), .offset = 0, .range = vk::WholeSize };
					const vk::DescriptorBufferInfo meshlet_vert_info{ .buffer = mesh.meshlet_vertex_buffer(), .offset = 0, .range = vk::WholeSize };
					const vk::DescriptorBufferInfo meshlet_tri_info{ .buffer = mesh.meshlet_triangle_buffer(), .offset = 0, .range = vk::WholeSize };
					const vk::DescriptorBufferInfo meshlet_bounds_info{ .buffer = mesh.meshlet_bounds_buffer(), .offset = 0, .range = vk::WholeSize };

					const std::array<vk::WriteDescriptorSet, 6> descriptor_writes{
						vk::WriteDescriptorSet{ .dstBinding = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &vertex_storage_info },
						vk::WriteDescriptorSet{ .dstBinding = 1, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &meshlet_desc_info },
						vk::WriteDescriptorSet{ .dstBinding = 2, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &meshlet_vert_info },
						vk::WriteDescriptorSet{ .dstBinding = 3, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &meshlet_tri_info },
						vk::WriteDescriptorSet{ .dstBinding = 4, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &meshlet_bounds_info },
						vk::WriteDescriptorSet{ .dstBinding = 5, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &instance_buffer_info }
					};

					ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, s.meshlet_pipeline.native_layout(), 1, descriptor_writes);

					const std::uint32_t meshlet_count = mesh.meshlet_count();
					for (std::uint32_t inst = 0; inst < batch.instance_count; ++inst) {
						auto pc = s.meshlet_shader->cache_push_block("push_constants");
						pc.set("meshlet_offset", std::uint32_t(0));
						pc.set("meshlet_count", meshlet_count);
						pc.set("instance_index", batch.first_instance + inst);
						ctx.push(s.meshlet_pipeline, pc);

						const std::uint32_t task_groups = (meshlet_count + 31) / 32;
						ctx.draw_mesh_tasks(task_groups, 1, 1);
					}
				}
			}

			if (!data.skinned_batches.empty()) {
				ctx.bind_pipeline(vk::PipelineBindPoint::eGraphics, s.skinned_pipeline);
				ctx.bind_descriptor_set(vk::PipelineBindPoint::eGraphics, s.skinned_pipeline, 0, s.skinned_descriptor_sets[frame_index]);

				const vk::DescriptorBufferInfo skin_buffer_info{
					.buffer = gc_state->skin_buffer[frame_index].buffer,
					.offset = 0,
					.range = gc_state->skin_buffer[frame_index].allocation.size()
				};

				const vk::DescriptorBufferInfo instance_buffer_info{
					.buffer = gc_state->instance_buffer[frame_index].buffer,
					.offset = 0,
					.range = gc_state->instance_buffer[frame_index].allocation.size()
				};

				const std::array<vk::WriteDescriptorSet, 2> descriptor_writes{
					vk::WriteDescriptorSet{
						.dstBinding = 0,
						.dstArrayElement = 0,
						.descriptorCount = 1,
						.descriptorType = vk::DescriptorType::eStorageBuffer,
						.pBufferInfo = &skin_buffer_info
					},
					vk::WriteDescriptorSet{
						.dstBinding = 1,
						.dstArrayElement = 0,
						.descriptorCount = 1,
						.descriptorType = vk::DescriptorType::eStorageBuffer,
						.pBufferInfo = &instance_buffer_info
					}
				};
				ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, s.skinned_pipeline.native_layout(), 1, descriptor_writes);

				for (std::size_t i = 0; i < data.skinned_batches.size(); ++i) {
					const auto& batch = data.skinned_batches[i];
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					const vk::Buffer vertex_buffers[]{ mesh.vertex_buffer() };
					const vk::DeviceSize offsets[]{ 0 };
					ctx.bind_vertex_buffers(0, vertex_buffers, offsets);
					ctx.bind_index_buffer(mesh.index_buffer(), 0, vk::IndexType::eUint32);

					ctx.draw_indirect(
						gc_state->skinned_indirect_commands_buffer[frame_index].buffer,
						i * sizeof(vk::DrawIndexedIndirectCommand),
						1,
						0
					);
				}
			}
		});
}
