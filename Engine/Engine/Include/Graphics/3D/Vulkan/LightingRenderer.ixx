export module gse.graphics:lighting_renderer;

import std;

import :base_renderer;
import :point_light;
import :spot_light;
import :directional_light;
import :shader;

export namespace gse::renderer {
	class lighting final : public base_renderer {
	public:
		explicit lighting(context& context) : base_renderer(context) {}

		auto initialize() -> void override;
		auto render(std::span<std::reference_wrapper<registry>> registries) -> void override;
	private:
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		vk::raii::DescriptorSet m_descriptor_set = nullptr;
		vk::raii::Sampler m_buffer_sampler = nullptr;

		resource::handle<shader> m_shader;

		std::unordered_map<std::string, vulkan::persistent_allocator::buffer_resource> m_ubo_allocations;
		vulkan::persistent_allocator::buffer_resource m_light_buffer;
	};
}

auto gse::renderer::lighting::initialize() -> void {
	auto& config = m_context.config();
	m_shader = m_context.get<shader>("lighting_pass");
	m_context.instantly_load(m_shader);

	auto lighting_layouts = m_shader->layouts();

	m_descriptor_set = m_shader->descriptor_set(config.device_config().device, config.descriptor_config().pool, shader::set::binding_type::persistent);

	const auto cam_block = m_shader->uniform_block("CameraParams");

	vk::BufferCreateInfo cam_buffer_info{
		.size = cam_block.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	m_ubo_allocations["CameraParams"] = vulkan::persistent_allocator::create_buffer(
		config.device_config(),
		cam_buffer_info
	);

	const auto light_block = m_shader->uniform_block("lights_ssbo");

	vk::BufferCreateInfo light_buffer_info{
		.size = light_block.size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	m_light_buffer = vulkan::persistent_allocator::create_buffer(
		config.device_config(),
		light_buffer_info
	);

	const std::unordered_map<std::string, vk::DescriptorBufferInfo> lighting_buffer_infos = {
		{
			"CameraParams",
			{
				.buffer = m_ubo_allocations["CameraParams"].buffer,
				.offset = 0,
				.range = cam_block.size
			}
		},
		{
			"lights_ssbo",
			{
				.buffer = m_light_buffer.buffer,
				.offset = 0,
				.range = light_block.size
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
	m_buffer_sampler = config.device_config().device.createSampler(sampler_create_info);

	const std::unordered_map<std::string, vk::DescriptorImageInfo> lighting_image_infos = {
		{
			"g_albedo",
			{
				.sampler = m_buffer_sampler,
				.imageView = *config.swap_chain_config().albedo_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		},
		{
			"g_normal",
			{
				.sampler = m_buffer_sampler,
				.imageView = *config.swap_chain_config().normal_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		},
		{
			"g_depth",
			{
				.sampler = m_buffer_sampler,
				.imageView = *config.swap_chain_config().depth_image.view,
				.imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal
			}
		}
	};

	auto writes = m_shader->descriptor_writes(
		m_descriptor_set,
		lighting_buffer_infos,
		lighting_image_infos
	);

	config.device_config().device.updateDescriptorSets(writes, nullptr);

	const auto range = m_shader->push_constant_range("push_constants");

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(lighting_layouts.size()),
		.pSetLayouts = lighting_layouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &range
	};
	m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	auto lighting_stages = m_shader->shader_stages();

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
		.width = static_cast<float>(config.swap_chain_config().extent.width),
		.height = static_cast<float>(config.swap_chain_config().extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	const vk::Rect2D scissor{
		.offset = { 0, 0 },
		.extent = config.swap_chain_config().extent
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
		.frontFace = vk::FrontFace::eCounterClockwise,
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

	const auto color_format = config.swap_chain_config().surface_format.format;
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
	m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, lighting_pipeline_info);
}

auto gse::renderer::lighting::render(std::span<std::reference_wrapper<registry>> registries) -> void {
	auto& config = m_context.config();
	const auto command = config.frame_context().command_buffer;

	auto inv_pv = (m_context.camera().projection(m_context.window().viewport()) * m_context.camera().view()).inverse();
	auto view_pos = m_context.camera().position().as<units::meters>().data;

	const std::unordered_map<std::string, std::span<const std::byte>> cam_data_map = {
		{ "inv_pv", std::as_bytes(std::span(&inv_pv, 1)) },
		{ "view_pos", std::as_bytes(std::span(&view_pos, 1)) }
	};

	m_shader->set_uniform_block("CameraParams", cam_data_map, m_ubo_allocations.at("CameraParams").allocation);

	const auto light_block = m_shader->uniform_block("lights_ssbo");
	const auto stride = light_block.size;

	const auto& alloc = m_light_buffer.allocation;

	std::vector zero_elem(stride, std::byte{ 0 });

	auto zero_at = [&](
		std::size_t index
		) {
			m_shader->set_ssbo_struct(
				"lights_ssbo",
				index,
				std::span<const std::byte>(
					zero_elem.data(), 
					zero_elem.size()
				),
				alloc
			);
		};

	auto set = [&](
		const std::size_t index,
		std::string_view member, 
		auto const& v
		) {
			m_shader->set_ssbo_element(
				"lights_ssbo",
				index,
				member,
				std::as_bytes(std::span(&v, 1)),
				alloc
			);
		};

	std::size_t light_count = 0;

	for (auto& reg_ref : registries) {
		auto& reg = reg_ref.get();
		if (light_count >= stride) {
			break;
		}

		for (const auto& comp : reg.linked_objects<directional_light_component>()) {
			if (light_count >= stride) {
				break;
			}
			zero_at(light_count);

			int type = 0; // LightType::Directional
			set(light_count, "light_type", type);
			set(light_count, "direction", comp.direction);
			set(light_count, "color", comp.color);
			set(light_count, "intensity", comp.intensity);
			set(light_count, "ambient_strength", comp.ambient_strength);

			++light_count;
		}
		for (const auto& comp : reg.linked_objects<point_light_component>()) {
			if (light_count >= stride) {
				break;
			}
			zero_at(light_count);

			int type = 1; // LightType::Point
			auto pos_meters = comp.position.as<units::meters>();
			set(light_count, "light_type", type);
			set(light_count, "position", pos_meters);
			set(light_count, "color", comp.color);
			set(light_count, "intensity", comp.intensity);
			set(light_count, "constant", comp.constant);
			set(light_count, "linear", comp.linear);
			set(light_count, "quadratic", comp.quadratic);
			set(light_count, "ambient_strength", comp.ambient_strength);

			++light_count;
		}
		for (const auto& comp : reg.linked_objects<spot_light_component>()) {
			if (light_count >= stride) {
				break;
			}
			zero_at(light_count);

			int type = 2; // LightType::Spot
			auto pos_meters = comp.position.as<units::meters>();
			float cut_off_cos = std::cos(comp.cut_off.as<units::radians>());
			float outer_cut_off_cos = std::cos(comp.outer_cut_off.as<units::radians>());

			set(light_count, "light_type", type);
			set(light_count, "position", pos_meters);
			set(light_count, "direction", comp.direction);
			set(light_count, "color", comp.color);
			set(light_count, "intensity", comp.intensity);
			set(light_count, "constant", comp.constant);
			set(light_count, "linear", comp.linear);
			set(light_count, "quadratic", comp.quadratic);
			set(light_count, "cut_off", cut_off_cos);
			set(light_count, "outer_cut_off", outer_cut_off_cos);
			set(light_count, "ambient_strength", comp.ambient_strength);

			++light_count;
		}
	}

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_config().albedo_image,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead
	);

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_config().normal_image,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead
	);

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_config().depth_image,
		vk::ImageLayout::eDepthReadOnlyOptimal,
		vk::ImageAspectFlagBits::eDepth,
		vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead
	);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_config().image_views[config.frame_context().image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }}}
	};

	const vk::RenderingInfo lighting_rendering_info{
		.renderArea = { { 0, 0 }, config.swap_chain_config().extent },
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

			m_shader->push(
				command,
				m_pipeline_layout,
				"push_constants",
				&light_count,
				0
			);

			command.draw(3, 1, 0, 0);
		}
	);
}