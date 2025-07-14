export module gse.graphics:lighting_renderer;

import std;

import :base_renderer;

export namespace gse::renderer {
	class lighting final : public base_renderer {
	public:
		explicit lighting(const std::unique_ptr<context>& context, registry& registry) : base_renderer(context, registry) {}

		auto initialize() -> void override;
		auto render() -> void override;
	private:
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		vk::raii::DescriptorSet m_descriptor_set = nullptr;
		vk::raii::Sampler m_buffer_sampler = nullptr;

		std::unordered_map<std::string, vulkan::persistent_allocator::buffer_resource> m_ubo_allocations;
	};
}

auto gse::renderer::lighting::initialize() -> void {
	auto& config = m_context->config();
	const auto id = m_context->queue<shader>(config::shader_spirv_path / "lighting_pass.vert.spv");

	const auto* lighting_shader = m_context->instantly_load<shader>(id).shader;
	auto lighting_layouts = lighting_shader->layouts();

	m_descriptor_set = lighting_shader->descriptor_set(config.device_data.device, config.descriptor.pool, shader::set::binding_type::persistent);

	const auto cam_block = lighting_shader->uniform_block("cam");

	vk::BufferCreateInfo cam_buffer_info{
		.size = cam_block.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	auto buffer = vulkan::persistent_allocator::create_buffer(
		config.device_data,
		cam_buffer_info,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	m_ubo_allocations["cam"] = std::move(buffer);

	const std::unordered_map<std::string, vk::DescriptorBufferInfo> lighting_buffer_infos = {
		{
			"cam",
			{
				.buffer = m_ubo_allocations["cam"].buffer,
				.offset = 0,
				.range = cam_block.size
			}
		}
	};


	constexpr vk::SamplerCreateInfo sampler_create_info{
		.magFilter = vk::Filter::eNearest,
		.minFilter = vk::Filter::eNearest,
		.mipmapMode = vk::SamplerMipmapMode::eNearest,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
		.addressModeW = vk::SamplerAddressMode::eClampToEdge,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::False,
		.maxAnisotropy = 1.0f,
		.compareEnable = vk::False,
		.minLod = 0.0f,
		.maxLod = 1.0f,
		.borderColor = vk::BorderColor::eFloatOpaqueWhite
	};
	m_buffer_sampler = config.device_data.device.createSampler(sampler_create_info);

	const std::unordered_map<std::string, vk::DescriptorImageInfo> lighting_image_infos = {
		{
			"g_albedo",
			{
				.sampler = m_buffer_sampler,
				.imageView = *config.swap_chain_data.albedo_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		},
		{
			"g_normal",
			{
				.sampler = m_buffer_sampler,
				.imageView = *config.swap_chain_data.normal_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		},
		{
			"g_depth",
			{
				.sampler = m_buffer_sampler,
				.imageView = *config.swap_chain_data.depth_image.view,
				.imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal
			}
		}
	};

	auto writes = lighting_shader->descriptor_writes(
		m_descriptor_set,
		lighting_buffer_infos,
		lighting_image_infos
	);

	config.device_data.device.updateDescriptorSets(writes, nullptr);

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(lighting_layouts.size()),
		.pSetLayouts = lighting_layouts.data()
	};
	m_pipeline_layout = config.device_data.device.createPipelineLayout(pipeline_layout_info);

	auto lighting_stages = lighting_shader->shader_stages();

	constexpr vk::PipelineVertexInputStateCreateInfo empty_vertex_input{
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr
	};

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	const vk::Viewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(config.swap_chain_data.extent.width),
		.height = static_cast<float>(config.swap_chain_data.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	const vk::Rect2D scissor{
		.offset = { 0, 0 },
		.extent = config.swap_chain_data.extent
	};

	const vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eNone,
		.frontFace = vk::FrontFace::eClockwise,
		.depthBiasEnable = vk::False,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};

	constexpr vk::PipelineMultisampleStateCreateInfo multi_sample_state{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	constexpr vk::PipelineColorBlendAttachmentState color_blend_attachment{
		.blendEnable = vk::False,
		.srcColorBlendFactor = vk::BlendFactor::eOne,
		.dstColorBlendFactor = vk::BlendFactor::eZero,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};

	const vk::PipelineColorBlendStateCreateInfo color_blend_state{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
		.blendConstants = std::array{ 0.0f, 0.0f, 0.0f, 0.0f }
	};

	const auto color_format = config.swap_chain_data.surface_format.format;
	const vk::PipelineRenderingCreateInfoKHR lighting_rendering_info{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color_format
	};

	const vk::GraphicsPipelineCreateInfo lighting_pipeline_info{
		.pNext = &lighting_rendering_info,
		.stageCount = static_cast<std::uint32_t>(lighting_stages.size()),
		.pStages = lighting_stages.data(),
		.pVertexInputState = &empty_vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multi_sample_state,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &color_blend_state,
		.pDynamicState = nullptr,
		.layout = m_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};
	m_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, lighting_pipeline_info);
}

auto gse::renderer::lighting::render() -> void {
	auto& config = m_context->config();
	const auto command = config.frame_context.command_buffer;

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_data.albedo_image,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead
	);

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_data.normal_image,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead
	);

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_data.depth_image,
		vk::ImageLayout::eDepthReadOnlyOptimal,
		vk::ImageAspectFlagBits::eDepth,
		vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead
	);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_data.image_views[config.frame_context.image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }}}
	};

	const vk::RenderingInfo lighting_rendering_info{
		.renderArea = { { 0, 0 }, config.swap_chain_data.extent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr
	};

	vulkan::render(
		config, 
		lighting_rendering_info, [&] {
			command.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
			command.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				m_pipeline_layout,
				0,
				1,
				&*m_descriptor_set,
				0,
				nullptr
			);
			command.draw(3, 1, 0, 0);
		}
	);
}