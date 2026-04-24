export module gse.gpu:gpu_pipeline;

import std;
import vulkan;

import :gpu_types;
import :vulkan_handles;
import :vulkan_reflect;
import :gpu_device;
import :gpu_context;
import :shader;
import :shader_registry;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::gpu {
	class pipeline final : public non_copyable {
	public:
		pipeline() = default;
		pipeline(
			vk::raii::Pipeline&& handle,
			vk::raii::PipelineLayout&& layout,
			bind_point point = bind_point::graphics
		);
		pipeline(pipeline&&) noexcept = default;
		auto operator=(pipeline&&) noexcept -> pipeline& = default;

		[[nodiscard]] auto native_pipeline(this const pipeline& self) -> vk::Pipeline { return *self.m_handle; }
		[[nodiscard]] auto native_layout(this const pipeline& self) -> vk::PipelineLayout { return *self.m_layout; }
		[[nodiscard]] auto point(this const pipeline& self) -> bind_point { return self.m_point; }

		operator vulkan::pipeline_handle(
		) const;

		explicit operator bool() const;
	private:
		vk::raii::Pipeline m_handle = nullptr;
		vk::raii::PipelineLayout m_layout = nullptr;
		bind_point m_point = bind_point::graphics;
	};

	class descriptor_set final : public non_copyable {
	public:
		descriptor_set() = default;
		descriptor_set(vk::raii::DescriptorSet&& set);
		descriptor_set(descriptor_set&&) noexcept = default;
		auto operator=(descriptor_set&&) noexcept -> descriptor_set& = default;

		[[nodiscard]] auto native(this const descriptor_set& self) -> vk::DescriptorSet { return *self.m_set; }

		explicit operator bool() const;
	private:
		vk::raii::DescriptorSet m_set = nullptr;
	};

	class sampler final : public non_copyable {
	public:
		sampler() = default;
		sampler(vk::raii::Sampler&& s);
		sampler(sampler&&) noexcept = default;
		auto operator=(sampler&&) noexcept -> sampler& = default;

		[[nodiscard]] auto native(this const sampler& self) -> vk::Sampler { return *self.m_sampler; }

		explicit operator bool() const;
	private:
		vk::raii::Sampler m_sampler = nullptr;
	};

	struct graphics_pipeline_desc {
		rasterization_state rasterization;
		depth_state depth;
		blend_preset blend = blend_preset::none;
		color_format color = color_format::swapchain;
		depth_format depth_fmt = depth_format::d32_sfloat;
		topology topology = topology::triangle_list;
		std::string push_constant_block;
	};

	auto create_graphics_pipeline(
		context& ctx,
		const shader& s,
		const graphics_pipeline_desc& desc = {}
	) -> pipeline;

	auto create_compute_pipeline(
		context& ctx,
		const shader& s,
		std::string_view push_constant_block = {}
	) -> pipeline;

	auto create_sampler(
		context& ctx,
		const sampler_desc& desc = {}
	) -> sampler;
}

gse::gpu::pipeline::pipeline(
	vk::raii::Pipeline&& handle,
	vk::raii::PipelineLayout&& layout,
	bind_point point
) : m_handle(std::move(handle)),
    m_layout(std::move(layout)),
    m_point(point) {}

gse::gpu::pipeline::operator vulkan::pipeline_handle() const {
	return {
		.pipeline = *m_handle,
		.layout = *m_layout,
		.point = m_point
	};
}

gse::gpu::pipeline::operator bool() const {
	return *m_handle != nullptr;
}

gse::gpu::descriptor_set::descriptor_set(vk::raii::DescriptorSet&& set)
	: m_set(std::move(set)) {}

gse::gpu::descriptor_set::operator bool() const {
	return *m_set != nullptr;
}

gse::gpu::sampler::sampler(vk::raii::Sampler&& s)
	: m_sampler(std::move(s)) {}

gse::gpu::sampler::operator bool() const {
	return *m_sampler != nullptr;
}

auto gse::gpu::create_graphics_pipeline(context& ctx, const shader& s, const graphics_pipeline_desc& desc) -> pipeline {
	auto& dev = ctx.device();
	auto& vk_device = dev.logical_device();
	const auto& cache = ctx.shader_registry().cache(s);

	auto layouts = cache.layout_handles;
	constexpr auto bindless_idx = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	if (layouts.size() > bindless_idx) {
		layouts[bindless_idx] = ctx.bindless_textures().layout();
	}

	const bool has_push = !desc.push_constant_block.empty();
	vk::PushConstantRange pc_range;
	if (has_push) {
		const auto pb = s.push_block(desc.push_constant_block);
		pc_range = vk::PushConstantRange{
			vulkan::to_vk(pb.stage_flags),
			0,
			pb.size
		};
	}

	vk::raii::PipelineLayout pipeline_layout = vk_device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data(),
		.pushConstantRangeCount = has_push ? 1u : 0u,
		.pPushConstantRanges = has_push ? &pc_range : nullptr
	});

	const vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vulkan::to_vk(desc.rasterization.polygon),
		.cullMode = vulkan::to_vk(desc.rasterization.cull),
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = desc.rasterization.depth_bias ? vk::True : vk::False,
		.depthBiasConstantFactor = desc.rasterization.depth_bias_constant,
		.depthBiasClamp = desc.rasterization.depth_bias_clamp,
		.depthBiasSlopeFactor = desc.rasterization.depth_bias_slope,
		.lineWidth = desc.rasterization.line_width
	};

	const vk::PipelineDepthStencilStateCreateInfo depth_stencil{
		.depthTestEnable = desc.depth.test ? vk::True : vk::False,
		.depthWriteEnable = desc.depth.write ? vk::True : vk::False,
		.depthCompareOp = vulkan::to_vk(desc.depth.compare),
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
		.topology = vulkan::to_vk(desc.topology),
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

	vk::Format vk_color_format = vk::Format::eUndefined;
	if (desc.color == color_format::swapchain) {
		vk_color_format = dev.surface_format();
	}

	vk::Format vk_depth_format = vk::Format::eUndefined;
	if (desc.depth_fmt == depth_format::d32_sfloat) {
		vk_depth_format = vk::Format::eD32Sfloat;
	}

	const bool has_color = desc.color != color_format::none;
	const vk::PipelineRenderingCreateInfoKHR rendering_info{
		.colorAttachmentCount = has_color ? 1u : 0u,
		.pColorAttachmentFormats = has_color ? &vk_color_format : nullptr,
		.depthAttachmentFormat = vk_depth_format
	};

	const auto blend_attachment = [&]() -> vk::PipelineColorBlendAttachmentState {
		switch (desc.blend) {
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
		.attachmentCount = has_color ? 1u : 0u,
		.pAttachments = has_color ? &blend_attachment : nullptr
	};

	auto make_stage = [&](gpu::shader_stage stage, vk::ShaderStageFlagBits vk_stage) -> vk::PipelineShaderStageCreateInfo {
		return {
			.stage = vk_stage,
			.module = *cache.modules.at(stage),
			.pName = "main"
		};
	};

	if (s.is_mesh_shader()) {
		const std::array stages = {
			make_stage(gpu::shader_stage::task, vk::ShaderStageFlagBits::eTaskEXT),
			make_stage(gpu::shader_stage::mesh, vk::ShaderStageFlagBits::eMeshEXT),
			make_stage(gpu::shader_stage::fragment, vk::ShaderStageFlagBits::eFragment),
		};

		vk::raii::Pipeline handle = vk_device.createGraphicsPipeline(nullptr, {
			.pNext = &rendering_info,
			.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT,
			.stageCount = static_cast<std::uint32_t>(stages.size()),
			.pStages = stages.data(),
			.pVertexInputState = nullptr,
			.pInputAssemblyState = nullptr,
			.pTessellationState = nullptr,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depth_stencil,
			.pColorBlendState = has_color ? &color_blending : nullptr,
			.pDynamicState = &dynamic_state,
			.layout = *pipeline_layout
		});

		return pipeline(std::move(handle), std::move(pipeline_layout), bind_point::graphics);
	}

	const std::array stages = {
		make_stage(gpu::shader_stage::vertex, vk::ShaderStageFlagBits::eVertex),
		make_stage(gpu::shader_stage::fragment, vk::ShaderStageFlagBits::eFragment),
	};

	const auto& vi = s.vertex_input_data();
	std::vector<vk::VertexInputBindingDescription> vk_bindings;
	vk_bindings.reserve(vi.bindings.size());
	for (const auto& b : vi.bindings) {
		vk_bindings.push_back({
			.binding = b.binding,
			.stride = b.stride,
			.inputRate = b.per_instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex
		});
	}

	std::vector<vk::VertexInputAttributeDescription> vk_attrs;
	vk_attrs.reserve(vi.attributes.size());
	for (const auto& a : vi.attributes) {
		vk_attrs.push_back({
			.location = a.location,
			.binding = a.binding,
			.format = vulkan::to_vk(a.format),
			.offset = a.offset
		});
	}

	const vk::PipelineVertexInputStateCreateInfo vertex_input{
		.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vk_bindings.size()),
		.pVertexBindingDescriptions = vk_bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vk_attrs.size()),
		.pVertexAttributeDescriptions = vk_attrs.data()
	};

	vk::raii::Pipeline handle = vk_device.createGraphicsPipeline(nullptr, {
		.pNext = &rendering_info,
		.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT,
		.stageCount = static_cast<std::uint32_t>(stages.size()),
		.pStages = stages.data(),
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = has_color ? &color_blending : nullptr,
		.pDynamicState = &dynamic_state,
		.layout = *pipeline_layout
	});

	return pipeline(std::move(handle), std::move(pipeline_layout), bind_point::graphics);
}

auto gse::gpu::create_compute_pipeline(context& ctx, const shader& s, const std::string_view push_constant_block) -> pipeline {
	auto& vk_device = ctx.logical_device();
	const auto& cache = ctx.shader_registry().cache(s);

	auto layouts = cache.layout_handles;
	constexpr auto bindless_idx = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	if (layouts.size() > bindless_idx) {
		layouts[bindless_idx] = ctx.bindless_textures().layout();
	}

	const bool has_push = !push_constant_block.empty();
	vk::PushConstantRange pc_range;
	if (has_push) {
		const auto pb = s.push_block(push_constant_block);
		pc_range = vk::PushConstantRange{
			vulkan::to_vk(pb.stage_flags),
			0,
			pb.size
		};
	}

	vk::raii::PipelineLayout pipeline_layout = vk_device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data(),
		.pushConstantRangeCount = has_push ? 1u : 0u,
		.pPushConstantRanges = has_push ? &pc_range : nullptr
	});

	const vk::PipelineShaderStageCreateInfo compute_stage{
		.stage = vk::ShaderStageFlagBits::eCompute,
		.module = *cache.modules.at(gpu::shader_stage::compute),
		.pName = "main"
	};

	vk::raii::Pipeline handle = vk_device.createComputePipeline(nullptr, {
		.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT,
		.stage = compute_stage,
		.layout = *pipeline_layout
	});

	return pipeline(std::move(handle), std::move(pipeline_layout), bind_point::compute);
}

auto gse::gpu::create_sampler(context& ctx, const sampler_desc& desc) -> sampler {
	auto& vk_device = ctx.logical_device();

	const vk::SamplerCreateInfo info{
		.magFilter = vulkan::to_vk(desc.mag),
		.minFilter = vulkan::to_vk(desc.min),
		.mipmapMode = desc.min == sampler_filter::nearest ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear,
		.addressModeU = vulkan::to_vk(desc.address_u),
		.addressModeV = vulkan::to_vk(desc.address_v),
		.addressModeW = vulkan::to_vk(desc.address_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = desc.max_anisotropy > 0.0f ? vk::True : vk::False,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.compare_enable ? vk::True : vk::False,
		.compareOp = vulkan::to_vk(desc.compare),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = vulkan::to_vk(desc.border),
		.unnormalizedCoordinates = vk::False
	};

	return sampler(vk_device.createSampler(info));
}
