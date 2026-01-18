export module gse.graphics:geometry_renderer;

import std;

import :camera;
import :mesh;
import :model;
import :render_component;
import :material;
import :shader;

import gse.physics.math;
import gse.utility;
import gse.platform;
import gse.physics;

export namespace gse::renderer {
	class geometry final : public gse::system {
	public:
		explicit geometry(
			context& context
		);

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;

		auto render_queue(
		) const -> std::span<const render_queue_entry>;
	private:
		context& m_context;
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> m_descriptor_sets;

		resource::handle<shader> m_shader;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> m_ubo_allocations;

		double_buffer<std::vector<render_queue_entry>> m_render_queue;
	};
}

gse::renderer::geometry::geometry(context& context) : m_context(context) {}

auto gse::renderer::geometry::initialize() -> void {
	auto& config = m_context.config();

	auto transition_gbuffer_images = [](vulkan::config& cfg) {
		cfg.add_transient_work([&cfg](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
			auto transition_to_general = [&cmd](
				vulkan::image_resource& img,
				const vk::ImageAspectFlags aspect,
				const vk::PipelineStageFlags2 dst_stage,
				const vk::AccessFlags2 dst_access
			) {
				const vk::ImageMemoryBarrier2 barrier{
					.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.srcAccessMask = {},
					.dstStageMask = dst_stage,
					.dstAccessMask = dst_access,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eGeneral,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = img.image,
					.subresourceRange = {
						.aspectMask = aspect,
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
				img.current_layout = vk::ImageLayout::eGeneral;
			};

			transition_to_general(
				cfg.swap_chain_config().albedo_image,
				vk::ImageAspectFlagBits::eColor,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
			);

			transition_to_general(
				cfg.swap_chain_config().normal_image,
				vk::ImageAspectFlagBits::eColor,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
			);

			transition_to_general(
				cfg.swap_chain_config().depth_image,
				vk::ImageAspectFlagBits::eDepth,
				vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead
			);

			return {};
		});
	};

	transition_gbuffer_images(config);
	config.on_swap_chain_recreate(transition_gbuffer_images);

	m_shader = m_context.get<shader>("geometry_pass");
	m_context.instantly_load(m_shader);
	auto descriptor_set_layouts = m_shader->layouts();

	const auto camera_ubo = m_shader->uniform_block("CameraUBO");

	vk::BufferCreateInfo camera_ubo_buffer_info{
		.size = camera_ubo.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};
	
	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		m_ubo_allocations["CameraUBO"][i] = config.allocator().create_buffer(camera_ubo_buffer_info);

		m_descriptor_sets[i] = m_shader->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = m_ubo_allocations["CameraUBO"][i]->buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

		config.device_config().device.updateDescriptorSets(
			m_shader->descriptor_writes(*m_descriptor_sets[i], buffer_infos, image_infos),
			nullptr
		);
	}

	std::vector ranges = {
		m_shader->push_constant_range("push_constants"),
	};

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
		.pSetLayouts = descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size()),
		.pPushConstantRanges = ranges.data()
	};

	m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

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

	const std::array g_buffer_color_formats = {
		config.swap_chain_config().albedo_image.format,
		config.swap_chain_config().normal_image.format
	};

	std::array<vk::PipelineColorBlendAttachmentState, g_buffer_color_formats.size()> color_blend_attachments;
	color_blend_attachments.fill({
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	});

	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = static_cast<std::uint32_t>(color_blend_attachments.size()),
		.pAttachments = color_blend_attachments.data()
	};

	auto shader_stages = m_shader->shader_stages();

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	auto vertex_input_info = m_shader->vertex_input_state();

	const vk::PipelineRenderingCreateInfoKHR geometry_rendering_info = {
		.colorAttachmentCount = static_cast<uint32_t>(g_buffer_color_formats.size()),
		.pColorAttachmentFormats = g_buffer_color_formats.data(),
		.depthAttachmentFormat = vk::Format::eD32Sfloat
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state{
		.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	const vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};

	const vk::GraphicsPipelineCreateInfo pipeline_info{
		.pNext = &geometry_rendering_info,
		.stageCount = static_cast<std::uint32_t>(shader_stages.size()),
		.pStages = shader_stages.data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state,
		.layout = m_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

	frame_sync::on_end([this] {
		m_render_queue.flip();
	});
}

auto gse::renderer::geometry::update() -> void {
	this->write([this](const component_chunk<render_component>& render_chunk) {
		if (render_chunk.empty()) {
			return;
		}

		const auto frame_index = m_context.config().current_frame();

		m_shader->set_uniform(
			"CameraUBO.view",
			m_context.camera().view(),
			m_ubo_allocations.at("CameraUBO")[frame_index]->allocation
		);

		m_shader->set_uniform(
			"CameraUBO.proj",
			m_context.camera().projection(m_context.window().viewport()),
			m_ubo_allocations.at("CameraUBO")[frame_index]->allocation
		);

		auto& out = m_render_queue.write();
		out.clear();

		for (auto& component : render_chunk) {
			if (!component.render) {
				continue;
			}

			const auto* mc = render_chunk.read_from<physics::motion_component>(component);
			const auto* cc = render_chunk.read_from<physics::collision_component>(component);

			for (auto& model_handle : component.model_instances) {
				if (!model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				model_handle.update(*mc, *cc);
				out.append_range(model_handle.render_queue_entries());
			}
		}

		std::ranges::sort(
			out,
			[](const render_queue_entry& a, const render_queue_entry& b) {
				const auto* ma = a.model.resolve();
				const auto* mb = b.model.resolve();

				if (ma != mb) {
					return ma < mb;
				}

				return a.index < b.index;
			}
		);
	});
}

auto gse::renderer::geometry::render() -> void {
    auto& config = m_context.config();

    if (!config.frame_in_progress()) {
        return;
    }

    const auto command = config.frame_context().command_buffer;

    constexpr vk::MemoryBarrier2 pre_render_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
    };

    const vk::DependencyInfo pre_dep{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &pre_render_barrier
    };

    command.pipelineBarrier2(pre_dep);

    std::array color_attachments{
        vk::RenderingAttachmentInfo{
            .imageView = config.swap_chain_config().albedo_image.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                .color = vk::ClearColorValue{
                    .float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }
                }
            }
        },
        vk::RenderingAttachmentInfo{
            .imageView = config.swap_chain_config().normal_image.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                .color = vk::ClearColorValue{
                    .float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }
                }
            }
        }
    };

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
        .renderArea = { { 0, 0 }, config.swap_chain_config().extent },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
        .pDepthAttachment = &depth_attachment
    };

    vulkan::render(
        config,
        rendering_info,
        [&] {
            const auto& draw_list = m_render_queue.read();

            if (draw_list.empty()) {
                return;
            }

            command.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                m_pipeline
            );

            const vk::Viewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(config.swap_chain_config().extent.width),
                .height = static_cast<float>(config.swap_chain_config().extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };
            command.setViewport(0, viewport);

            const vk::Rect2D scissor{
                .offset = { 0, 0 },
                .extent = config.swap_chain_config().extent
            };
            command.setScissor(0, scissor);

            const auto frame_index = config.current_frame();
            const vk::DescriptorSet sets[]{ **m_descriptor_sets[frame_index] };

            command.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                m_pipeline_layout,
                0,
                vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
                {}
            );

            for (const auto& e : draw_list) {
                m_shader->push(
                    command,
                    m_pipeline_layout,
                    "push_constants",
                    "model", e.model_matrix,
                    "normal_matrix", e.normal_matrix
                );

                const auto& mesh = e.model->meshes()[e.index];

                if (!mesh.material().valid()) {
                    continue;
                }

                m_shader->push_descriptor(
                    command,
                    m_pipeline_layout,
                    "diffuseSampler",
                    mesh.material()->diffuse_texture->descriptor_info()
                );

                mesh.bind(command);
                mesh.draw(command);
            }
        }
    );

    constexpr vk::MemoryBarrier2 post_render_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eLateFragmentTests,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead
    };

    const vk::DependencyInfo post_dep{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &post_render_barrier
    };

    command.pipelineBarrier2(post_dep);
}

auto gse::renderer::geometry::render_queue() const -> std::span<const render_queue_entry> {
	return m_render_queue.read();
}