export module gse.platform:gpu_factory;

import std;

import :gpu_types;
import :gpu_compute;
import :gpu_context;
import :shader;
import :render_graph;

import gse.assert;
import :vulkan_allocator;
import gse.utility;

export namespace gse::gpu {
	struct graphics_pipeline_desc {
		rasterization_state rasterization;
		depth_state depth;
		blend_preset blend = blend_preset::none;
		color_format color = color_format::swapchain;
		depth_format depth_fmt = depth_format::d32_sfloat;
		topology topology = topology::triangle_list;
		std::string push_constant_block;
	};

	auto create_buffer(
		context& ctx,
		const buffer_desc& desc
	) -> buffer;

	auto create_graphics_pipeline(
		context& ctx,
		const shader& s,
		const graphics_pipeline_desc& desc = {}
	) -> pipeline;

	auto create_compute_pipeline(
		context& ctx,
		const shader& s,
		const std::string& push_constant_block = {}
	) -> pipeline;

	auto create_sampler(
		context& ctx,
		const sampler_desc& desc = {}
	) -> sampler;

	auto allocate_descriptor_set(
		context& ctx,
		const shader& s,
		shader::set::binding_type type = shader::set::binding_type::persistent
	) -> descriptor_set;

	auto update_descriptors_raw(
		context& ctx,
		std::span<const vk::WriteDescriptorSet> writes
	) -> void;

	auto update_descriptors(
		context& ctx,
		const descriptor_set& set,
		const shader& s,
		const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
		const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos = {}
	) -> void;

	auto update_descriptors(
		context& ctx,
		const descriptor_set& set,
		const shader& s,
		const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
		const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos,
		const std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>>& image_array_infos
	) -> void;

	auto create_compute_queue(
		context& ctx
	) -> compute_queue;
}

namespace {
	auto to_vk(gse::gpu::buffer_usage usage) -> vk::BufferUsageFlags {
		vk::BufferUsageFlags flags;
		if (gse::gpu::has_flag(usage, gse::gpu::buffer_usage::uniform))      flags |= vk::BufferUsageFlagBits::eUniformBuffer;
		if (gse::gpu::has_flag(usage, gse::gpu::buffer_usage::storage))      flags |= vk::BufferUsageFlagBits::eStorageBuffer;
		if (gse::gpu::has_flag(usage, gse::gpu::buffer_usage::indirect))     flags |= vk::BufferUsageFlagBits::eIndirectBuffer;
		if (gse::gpu::has_flag(usage, gse::gpu::buffer_usage::transfer_dst)) flags |= vk::BufferUsageFlagBits::eTransferDst;
		if (gse::gpu::has_flag(usage, gse::gpu::buffer_usage::vertex))       flags |= vk::BufferUsageFlagBits::eVertexBuffer;
		if (gse::gpu::has_flag(usage, gse::gpu::buffer_usage::index))        flags |= vk::BufferUsageFlagBits::eIndexBuffer;
		return flags;
	}

	auto to_vk(gse::gpu::cull_mode mode) -> vk::CullModeFlags {
		switch (mode) {
			case gse::gpu::cull_mode::none:  return vk::CullModeFlagBits::eNone;
			case gse::gpu::cull_mode::front: return vk::CullModeFlagBits::eFront;
			case gse::gpu::cull_mode::back:  return vk::CullModeFlagBits::eBack;
		}
		return vk::CullModeFlagBits::eNone;
	}

	auto to_vk(gse::gpu::compare_op op) -> vk::CompareOp {
		switch (op) {
			case gse::gpu::compare_op::never:              return vk::CompareOp::eNever;
			case gse::gpu::compare_op::less:               return vk::CompareOp::eLess;
			case gse::gpu::compare_op::equal:              return vk::CompareOp::eEqual;
			case gse::gpu::compare_op::less_or_equal:      return vk::CompareOp::eLessOrEqual;
			case gse::gpu::compare_op::greater:            return vk::CompareOp::eGreater;
			case gse::gpu::compare_op::not_equal:          return vk::CompareOp::eNotEqual;
			case gse::gpu::compare_op::greater_or_equal:   return vk::CompareOp::eGreaterOrEqual;
			case gse::gpu::compare_op::always:             return vk::CompareOp::eAlways;
		}
		return vk::CompareOp::eAlways;
	}

	auto to_vk(gse::gpu::polygon_mode mode) -> vk::PolygonMode {
		switch (mode) {
			case gse::gpu::polygon_mode::fill:  return vk::PolygonMode::eFill;
			case gse::gpu::polygon_mode::line:  return vk::PolygonMode::eLine;
			case gse::gpu::polygon_mode::point: return vk::PolygonMode::ePoint;
		}
		return vk::PolygonMode::eFill;
	}

	auto to_vk(gse::gpu::topology t) -> vk::PrimitiveTopology {
		switch (t) {
			case gse::gpu::topology::triangle_list: return vk::PrimitiveTopology::eTriangleList;
			case gse::gpu::topology::line_list:     return vk::PrimitiveTopology::eLineList;
			case gse::gpu::topology::point_list:    return vk::PrimitiveTopology::ePointList;
		}
		return vk::PrimitiveTopology::eTriangleList;
	}

	auto to_vk(gse::gpu::sampler_filter f) -> vk::Filter {
		switch (f) {
			case gse::gpu::sampler_filter::nearest: return vk::Filter::eNearest;
			case gse::gpu::sampler_filter::linear:  return vk::Filter::eLinear;
		}
		return vk::Filter::eLinear;
	}

	auto to_vk(gse::gpu::sampler_address_mode m) -> vk::SamplerAddressMode {
		switch (m) {
			case gse::gpu::sampler_address_mode::repeat:          return vk::SamplerAddressMode::eRepeat;
			case gse::gpu::sampler_address_mode::clamp_to_edge:   return vk::SamplerAddressMode::eClampToEdge;
			case gse::gpu::sampler_address_mode::clamp_to_border: return vk::SamplerAddressMode::eClampToBorder;
			case gse::gpu::sampler_address_mode::mirrored_repeat: return vk::SamplerAddressMode::eMirroredRepeat;
		}
		return vk::SamplerAddressMode::eRepeat;
	}

	auto to_vk(gse::gpu::border_color c) -> vk::BorderColor {
		switch (c) {
			case gse::gpu::border_color::float_opaque_white:       return vk::BorderColor::eFloatOpaqueWhite;
			case gse::gpu::border_color::float_opaque_black:       return vk::BorderColor::eFloatOpaqueBlack;
			case gse::gpu::border_color::float_transparent_black:  return vk::BorderColor::eFloatTransparentBlack;
		}
		return vk::BorderColor::eFloatOpaqueWhite;
	}

	auto make_blend_attachment(gse::gpu::blend_preset preset) -> vk::PipelineColorBlendAttachmentState {
		switch (preset) {
			case gse::gpu::blend_preset::none:
				return {
					.blendEnable = vk::False,
					.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
				};
			case gse::gpu::blend_preset::alpha:
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
			case gse::gpu::blend_preset::alpha_premultiplied:
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
	}
}

auto gse::gpu::create_buffer(context& ctx, const buffer_desc& desc) -> buffer {
	auto vk_usage = to_vk(desc.usage);
	auto resource = ctx.allocator().create_buffer({
		.size = static_cast<vk::DeviceSize>(desc.size),
		.usage = vk_usage
	});
	return buffer(std::move(resource));
}

auto gse::gpu::create_graphics_pipeline(context& ctx, const shader& s, const graphics_pipeline_desc& desc) -> pipeline {
	auto& device = ctx.device();

	auto layouts = s.layouts();

	const bool has_push = !desc.push_constant_block.empty();
	vk::PushConstantRange pc_range;
	if (has_push) {
		pc_range = s.push_constant_range(desc.push_constant_block);
	}

	vk::raii::PipelineLayout pipeline_layout = device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data(),
		.pushConstantRangeCount = has_push ? 1u : 0u,
		.pPushConstantRanges = has_push ? &pc_range : nullptr
	});

	const vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = to_vk(desc.rasterization.polygon),
		.cullMode = to_vk(desc.rasterization.cull),
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
		.depthCompareOp = to_vk(desc.depth.compare),
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
		.topology = to_vk(desc.topology),
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
		vk_color_format = ctx.surface_format();
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

	const auto blend_attachment = make_blend_attachment(desc.blend);
	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = has_color ? 1u : 0u,
		.pAttachments = has_color ? &blend_attachment : nullptr
	};

	const bool is_mesh = s.is_mesh_shader();

	if (is_mesh) {
		const auto stages = s.mesh_shader_stages();

		vk::raii::Pipeline handle = device.createGraphicsPipeline(nullptr, {
			.pNext = &rendering_info,
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

		return pipeline(std::move(handle), std::move(pipeline_layout));
	}

	const auto stages = s.shader_stages();
	const auto vertex_input = s.vertex_input_state();

	vk::raii::Pipeline handle = device.createGraphicsPipeline(nullptr, {
		.pNext = &rendering_info,
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

	return pipeline(std::move(handle), std::move(pipeline_layout));
}

auto gse::gpu::create_compute_pipeline(context& ctx, const shader& s, const std::string& push_constant_block) -> pipeline {
	auto& device = ctx.device();

	auto layouts = s.layouts();

	const bool has_push = !push_constant_block.empty();
	vk::PushConstantRange pc_range;
	if (has_push) {
		pc_range = s.push_constant_range(push_constant_block);
	}

	vk::raii::PipelineLayout pipeline_layout = device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data(),
		.pushConstantRangeCount = has_push ? 1u : 0u,
		.pPushConstantRanges = has_push ? &pc_range : nullptr
	});

	vk::raii::Pipeline handle = device.createComputePipeline(nullptr, {
		.stage = s.compute_stage(),
		.layout = *pipeline_layout
	});

	return pipeline(std::move(handle), std::move(pipeline_layout));
}

auto gse::gpu::create_sampler(context& ctx, const sampler_desc& desc) -> sampler {
	auto& device = ctx.device();

	const vk::SamplerCreateInfo info{
		.magFilter = to_vk(desc.mag),
		.minFilter = to_vk(desc.min),
		.mipmapMode = desc.min == sampler_filter::nearest ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear,
		.addressModeU = to_vk(desc.address_u),
		.addressModeV = to_vk(desc.address_v),
		.addressModeW = to_vk(desc.address_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = desc.max_anisotropy > 0.0f ? vk::True : vk::False,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.compare_enable ? vk::True : vk::False,
		.compareOp = to_vk(desc.compare),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = to_vk(desc.border),
		.unnormalizedCoordinates = vk::False
	};

	return sampler(device.createSampler(info));
}

auto gse::gpu::allocate_descriptor_set(context& ctx, const shader& s, const shader::set::binding_type type) -> descriptor_set {
	auto set = s.descriptor_set(
		ctx.device(),
		ctx.descriptor_pool(),
		type
	);
	return descriptor_set(std::move(set));
}

auto gse::gpu::update_descriptors_raw(context& ctx, const std::span<const vk::WriteDescriptorSet> writes) -> void {
	ctx.device().updateDescriptorSets(writes, nullptr);
}

auto gse::gpu::update_descriptors(
	context& ctx,
	const descriptor_set& set,
	const shader& s,
	const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
	const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos
) -> void {
	auto writes = s.descriptor_writes(set.native(), buffer_infos, image_infos);
	ctx.device().updateDescriptorSets(writes, nullptr);
}

auto gse::gpu::update_descriptors(
	context& ctx,
	const descriptor_set& set,
	const shader& s,
	const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
	const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos,
	const std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>>& image_array_infos
) -> void {
	auto writes = s.descriptor_writes(set.native(), buffer_infos, image_infos, image_array_infos);
	ctx.device().updateDescriptorSets(writes, nullptr);
}

auto gse::gpu::create_compute_queue(context& ctx) -> compute_queue {
	auto& device = ctx.device();

	auto pool = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = ctx.compute_queue_family()
	});

	auto cmd = std::move(device.allocateCommandBuffers({
		.commandPool = *pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	}).front());

	auto fence = device.createFence({
		.flags = vk::FenceCreateFlagBits::eSignaled
	});

	auto query_pool = device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 4
	});

	static_cast<vk::Device>(*device).resetQueryPool(*query_pool, 0, 4);

	const float timestamp_period = ctx.physical_device().getProperties().limits.timestampPeriod;

	return compute_queue(
		std::move(pool),
		std::move(cmd),
		std::move(fence),
		std::move(query_pool),
		&ctx.compute_queue_ref(),
		&device,
		timestamp_period
	);
}
