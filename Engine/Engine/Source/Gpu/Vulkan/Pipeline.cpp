module gse.gpu;

import std;
import vulkan;

gse::vulkan::pipeline::pipeline(vk::raii::Pipeline&& pipeline, vk::raii::PipelineLayout&& layout, vk::PipelineBindPoint bind_point)
	: m_pipeline(std::move(pipeline)), m_layout(std::move(layout)), m_bind_point(bind_point) {}

auto gse::vulkan::pipeline::handle(this const pipeline& self) -> gpu::handle<pipeline> {
	return std::bit_cast<gpu::handle<pipeline>>(*self.m_pipeline);
}

auto gse::vulkan::pipeline::layout(this const pipeline& self) -> gpu::handle<pipeline_layout> {
	return std::bit_cast<gpu::handle<pipeline_layout>>(*self.m_layout);
}

auto gse::vulkan::pipeline::bind_point(this const pipeline& self) -> gpu::bind_point {
	return self.m_bind_point == vk::PipelineBindPoint::eCompute ? gpu::bind_point::compute : gpu::bind_point::graphics;
}

gse::vulkan::pipeline::operator bool() const {
	return *m_pipeline != nullptr;
}

auto gse::vulkan::pipeline::create_graphics(const device& dev, const graphics_pipeline_create_info& info) -> pipeline {
	std::vector<vk::DescriptorSetLayout> vk_set_layouts;
	vk_set_layouts.reserve(info.set_layouts.size());
	for (const auto h : info.set_layouts) {
		vk_set_layouts.push_back(std::bit_cast<vk::DescriptorSetLayout>(h));
	}

	const std::uint32_t pc_count = info.push_constant_range.has_value() ? 1u : 0u;
	std::optional<vk::PushConstantRange> vk_pc_range;
	if (info.push_constant_range.has_value()) {
		vk_pc_range = to_vk(*info.push_constant_range);
	}
	const vk::PushConstantRange* pc_ptr = vk_pc_range.has_value() ? &*vk_pc_range : nullptr;

	vk::raii::PipelineLayout layout = dev.raii_device().createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(vk_set_layouts.size()),
		.pSetLayouts = vk_set_layouts.data(),
		.pushConstantRangeCount = pc_count,
		.pPushConstantRanges = pc_ptr
	});

	const vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = to_vk(info.rasterization.polygon),
		.cullMode = to_vk(info.rasterization.cull),
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = info.rasterization.depth_bias ? vk::True : vk::False,
		.depthBiasConstantFactor = info.rasterization.depth_bias_constant,
		.depthBiasClamp = info.rasterization.depth_bias_clamp,
		.depthBiasSlopeFactor = info.rasterization.depth_bias_slope,
		.lineWidth = info.rasterization.line_width
	};

	const vk::PipelineDepthStencilStateCreateInfo depth_stencil{
		.depthTestEnable = info.depth.test ? vk::True : vk::False,
		.depthWriteEnable = info.depth.write ? vk::True : vk::False,
		.depthCompareOp = to_vk(info.depth.compare),
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
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

	const vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = to_vk(info.topology),
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

	const vk::Format vk_color_format = to_vk(info.color_format);
	const vk::Format vk_depth_format = to_vk(info.depth_format);
	const vk::PipelineRenderingCreateInfoKHR rendering_info{
		.colorAttachmentCount = info.has_color ? 1u : 0u,
		.pColorAttachmentFormats = info.has_color ? &vk_color_format : nullptr,
		.depthAttachmentFormat = vk_depth_format
	};

	const auto blend_attachment = [&]() -> vk::PipelineColorBlendAttachmentState {
		switch (info.blend) {
			case gpu::blend_preset::none:
				return {
					.blendEnable = vk::False,
					.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
				};
			case gpu::blend_preset::alpha:
				return {
					.blendEnable = vk::True,
					.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
					.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
					.colorBlendOp = vk::BlendOp::eAdd,
					.srcAlphaBlendFactor = vk::BlendFactor::eOne,
					.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
					.alphaBlendOp = vk::BlendOp::eAdd,
					.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
				};
			case gpu::blend_preset::alpha_premultiplied:
				return {
					.blendEnable = vk::True,
					.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
					.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
					.colorBlendOp = vk::BlendOp::eAdd,
					.srcAlphaBlendFactor = vk::BlendFactor::eOne,
					.dstAlphaBlendFactor = vk::BlendFactor::eZero,
					.alphaBlendOp = vk::BlendOp::eAdd,
					.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
				};
		}
		return {};
	}();

	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = info.has_color ? 1u : 0u,
		.pAttachments = info.has_color ? &blend_attachment : nullptr
	};

	std::vector<vk::VertexInputBindingDescription> vk_vertex_bindings;
	vk_vertex_bindings.reserve(info.vertex_bindings.size());
	for (const auto& b : info.vertex_bindings) {
		vk_vertex_bindings.push_back({
			.binding = b.binding,
			.stride = b.stride,
			.inputRate = b.per_instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex,
		});
	}

	std::vector<vk::VertexInputAttributeDescription> vk_vertex_attrs;
	vk_vertex_attrs.reserve(info.vertex_attributes.size());
	for (const auto& a : info.vertex_attributes) {
		vk_vertex_attrs.push_back({
			.location = a.location,
			.binding = a.binding,
			.format = to_vk(a.format),
			.offset = a.offset,
		});
	}

	const vk::PipelineVertexInputStateCreateInfo vertex_input{
		.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vk_vertex_bindings.size()),
		.pVertexBindingDescriptions = vk_vertex_bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vk_vertex_attrs.size()),
		.pVertexAttributeDescriptions = vk_vertex_attrs.data(),
	};

	std::vector<vk::PipelineShaderStageCreateInfo> vk_stages;
	vk_stages.reserve(info.stages.size());
	for (const auto& s : info.stages) {
		vk_stages.push_back({
			.stage = to_vk(s.stage),
			.module = std::bit_cast<vk::ShaderModule>(s.module_handle),
			.pName = s.entry_point.c_str(),
		});
	}

	vk::raii::Pipeline handle = dev.raii_device().createGraphicsPipeline(nullptr, {
		.pNext = &rendering_info,
		.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT,
		.stageCount = static_cast<std::uint32_t>(vk_stages.size()),
		.pStages = vk_stages.data(),
		.pVertexInputState = info.is_mesh_pipeline ? nullptr : &vertex_input,
		.pInputAssemblyState = info.is_mesh_pipeline ? nullptr : &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = info.has_color ? &color_blending : nullptr,
		.pDynamicState = &dynamic_state,
		.layout = *layout
	});

	return pipeline(std::move(handle), std::move(layout), vk::PipelineBindPoint::eGraphics);
}

auto gse::vulkan::pipeline::create_compute(const device& dev, const compute_pipeline_create_info& info) -> pipeline {
	std::vector<vk::DescriptorSetLayout> vk_set_layouts;
	vk_set_layouts.reserve(info.set_layouts.size());
	for (const auto h : info.set_layouts) {
		vk_set_layouts.push_back(std::bit_cast<vk::DescriptorSetLayout>(h));
	}

	const std::uint32_t pc_count = info.push_constant_range.has_value() ? 1u : 0u;
	std::optional<vk::PushConstantRange> vk_pc_range;
	if (info.push_constant_range.has_value()) {
		vk_pc_range = to_vk(*info.push_constant_range);
	}
	const vk::PushConstantRange* pc_ptr = vk_pc_range.has_value() ? &*vk_pc_range : nullptr;

	vk::raii::PipelineLayout layout = dev.raii_device().createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(vk_set_layouts.size()),
		.pSetLayouts = vk_set_layouts.data(),
		.pushConstantRangeCount = pc_count,
		.pPushConstantRanges = pc_ptr
	});

	const vk::PipelineShaderStageCreateInfo vk_stage{
		.stage = to_vk(info.stage.stage),
		.module = std::bit_cast<vk::ShaderModule>(info.stage.module_handle),
		.pName = info.stage.entry_point.c_str(),
	};

	vk::raii::Pipeline handle = dev.raii_device().createComputePipeline(nullptr, {
		.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT,
		.stage = vk_stage,
		.layout = *layout
	});

	return pipeline(std::move(handle), std::move(layout), vk::PipelineBindPoint::eCompute);
}
