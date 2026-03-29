export module gse.graphics:depth_prepass_renderer;

import std;

import :geometry_collector;
import :cull_compute_renderer;
import :camera_system;

import gse.platform;
import gse.utility;

export namespace gse::renderer::depth_prepass {
	struct state {
		gpu::context* ctx = nullptr;

		vk::raii::Pipeline meshlet_pipeline = nullptr;
		vk::raii::PipelineLayout meshlet_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> meshlet_descriptor_sets;
		resource::handle<shader> meshlet_shader;

		vk::raii::Pipeline skinned_pipeline = nullptr;
		vk::raii::PipelineLayout skinned_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> skinned_descriptor_sets;
		resource::handle<shader> skinned_shader;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> ubo_allocations;

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

auto gse::renderer::depth_prepass::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& config = s.ctx->config();

	auto transition_images = [](vulkan::config& cfg) {
		cfg.add_transient_work([&cfg](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
			const vk::ImageMemoryBarrier2 barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = cfg.swap_chain_config().depth_image.image,
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
			cfg.swap_chain_config().depth_image.current_layout = vk::ImageLayout::eGeneral;

			return {};
		});
	};

	transition_images(config);
	config.on_swap_chain_recreate(transition_images);

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.depthBiasConstantFactor = 2.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 2.0f,
		.lineWidth = 1.0f
	};

	constexpr vk::PipelineDepthStencilStateCreateInfo depth_stencil{
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state{
		.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	constexpr vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};

	const vk::PipelineRenderingCreateInfoKHR depth_rendering_info{
		.colorAttachmentCount = 0,
		.depthAttachmentFormat = vk::Format::eD32Sfloat
	};

	s.meshlet_shader = s.ctx->get<shader>("Shaders/Standard3D/meshlet_depth_only");
	s.ctx->instantly_load(s.meshlet_shader);

	const auto meshlet_camera_ubo = s.meshlet_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = config.allocator().create_buffer({
			.size = meshlet_camera_ubo.size,
			.usage = vk::BufferUsageFlagBits::eUniformBuffer,
			.sharingMode = vk::SharingMode::eExclusive
		});
	}

	auto meshlet_layouts = s.meshlet_shader->layouts();
	const auto meshlet_pc_range = s.meshlet_shader->push_constant_range("push_constants");
	s.meshlet_pipeline_layout = config.device_config().device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(meshlet_layouts.size()),
		.pSetLayouts = meshlet_layouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &meshlet_pc_range
	});

	const auto meshlet_stages = s.meshlet_shader->mesh_shader_stages();
	s.meshlet_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, {
		.pNext = &depth_rendering_info,
		.stageCount = static_cast<std::uint32_t>(meshlet_stages.size()),
		.pStages = meshlet_stages.data(),
		.pVertexInputState = nullptr,
		.pInputAssemblyState = nullptr,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = nullptr,
		.pDynamicState = &dynamic_state,
		.layout = *s.meshlet_pipeline_layout
	});

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.meshlet_descriptor_sets[i] = s.meshlet_shader->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> meshlet_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = meshlet_camera_ubo.size
				}
			}
		};

		config.device_config().device.updateDescriptorSets(
			s.meshlet_shader->descriptor_writes(*s.meshlet_descriptor_sets[i], meshlet_buffer_infos, {}),
			nullptr
		);
	}

	s.skinned_shader = s.ctx->get<shader>("Shaders/Standard3D/skinned_depth_only");
	s.ctx->instantly_load(s.skinned_shader);

	auto skinned_layouts = s.skinned_shader->layouts();
	s.skinned_pipeline_layout = config.device_config().device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(skinned_layouts.size()),
		.pSetLayouts = skinned_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	});

	auto skinned_stages = s.skinned_shader->shader_stages();
	auto skinned_vertex_input_info = s.skinned_shader->vertex_input_state();
	s.skinned_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, {
		.pNext = &depth_rendering_info,
		.stageCount = static_cast<std::uint32_t>(skinned_stages.size()),
		.pStages = skinned_stages.data(),
		.pVertexInputState = &skinned_vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = nullptr,
		.pDynamicState = &dynamic_state,
		.layout = s.skinned_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	});

	const auto skinned_camera_ubo = s.skinned_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.skinned_descriptor_sets[i] = s.skinned_shader->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		const std::unordered_map<std::string, vk::DescriptorBufferInfo> skinned_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = skinned_camera_ubo.size
				}
			}
		};

		config.device_config().device.updateDescriptorSets(
			s.skinned_shader->descriptor_writes(*s.skinned_descriptor_sets[i], skinned_buffer_infos, {}),
			nullptr
		);
	}
}

auto gse::renderer::depth_prepass::system::render(const render_phase& phase, const state& s) -> void {
	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	auto& config = s.ctx->config();
	if (!config.frame_in_progress()) {
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

	const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index].allocation;
	s.meshlet_shader->set_uniform("CameraUBO.view", view, cam_alloc);
	s.meshlet_shader->set_uniform("CameraUBO.proj", proj, cam_alloc);
	s.skinned_shader->set_uniform("CameraUBO.view", view, cam_alloc);
	s.skinned_shader->set_uniform("CameraUBO.proj", proj, cam_alloc);

	const auto extent = config.swap_chain_config().extent;
	const auto graphics_upload_stage =
		vk::PipelineStageFlagBits2::eTaskShaderEXT |
		vk::PipelineStageFlagBits2::eMeshShaderEXT |
		vk::PipelineStageFlagBits2::eVertexShader;

	vk::RenderingAttachmentInfo depth_attachment{
		.imageView = config.swap_chain_config().depth_image.view,
		.imageLayout = vk::ImageLayout::eGeneral,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{
			.depthStencil = vk::ClearDepthStencilValue{
				.depth = 1.0f
			}
		}
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = { { 0, 0 }, extent },
		.layerCount = 1,
		.colorAttachmentCount = 0,
		.pColorAttachments = nullptr,
		.pDepthAttachment = &depth_attachment
	};

	s.ctx->graph()
		.add_pass<state>()
		.after<cull_compute::state>()
		.uploads(
			vulkan::upload(s.ubo_allocations.at("CameraUBO")[frame_index], graphics_upload_stage),
			vulkan::upload(gc_state->instance_buffer[frame_index], graphics_upload_stage)
		)
		.reads(
			vulkan::storage_read(gc_state->skin_buffer[frame_index], vk::PipelineStageFlagBits2::eVertexShader),
			vulkan::indirect_read(gc_state->skinned_indirect_commands_buffer[frame_index], vk::PipelineStageFlagBits2::eDrawIndirect)
		)
		.writes(vulkan::attachment(config.swap_chain_config().depth_image, vk::PipelineStageFlagBits2::eLateFragmentTests))
		.record_graphics(rendering_info, [&s, gc_state, &data, frame_index, extent](vulkan::recording_context& ctx) {
			const vk::Viewport viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = static_cast<float>(extent.width),
				.height = static_cast<float>(extent.height),
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			ctx.set_viewport(viewport);
			ctx.set_scissor({ { 0, 0 }, extent });

			if (!data.normal_batches.empty()) {
				ctx.bind_pipeline(vk::PipelineBindPoint::eGraphics, *s.meshlet_pipeline);
				const vk::DescriptorSet sets[]{ **s.meshlet_descriptor_sets[frame_index] };
				ctx.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, *s.meshlet_pipeline_layout, 0, sets);

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

					ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, *s.meshlet_pipeline_layout, 1, descriptor_writes);

					const std::uint32_t meshlet_count = mesh.meshlet_count();
					for (std::uint32_t inst = 0; inst < batch.instance_count; ++inst) {
						auto pc = s.meshlet_shader->cache_push_block("push_constants");
						pc.set("meshlet_offset", std::uint32_t(0));
						pc.set("meshlet_count", meshlet_count);
						pc.set("instance_index", batch.first_instance + inst);
						ctx.push(pc, *s.meshlet_pipeline_layout);

						const std::uint32_t task_groups = (meshlet_count + 31) / 32;
						ctx.draw_mesh_tasks(task_groups, 1, 1);
					}
				}
			}

			if (!data.skinned_batches.empty()) {
				ctx.bind_pipeline(vk::PipelineBindPoint::eGraphics, *s.skinned_pipeline);
				const vk::DescriptorSet sets[]{ **s.skinned_descriptor_sets[frame_index] };
				ctx.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, *s.skinned_pipeline_layout, 0, sets);

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
				ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, *s.skinned_pipeline_layout, 1, descriptor_writes);

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
