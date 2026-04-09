export module gse.platform:gpu_factory;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_pipeline;
import :gpu_image;
import :gpu_descriptor;
import :gpu_descriptor_writer;
import :gpu_compute;
import :gpu_device;
import :shader;
import :render_graph;

import gse.assert;
import :vulkan_allocator;
import :vulkan_uploader;
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
		device& dev,
		const buffer_desc& desc
	) -> buffer;

	auto create_graphics_pipeline(
		device& dev,
		const shader& s,
		const graphics_pipeline_desc& desc = {}
	) -> pipeline;

	auto create_compute_pipeline(
		device& dev,
		const shader& s,
		const std::string& push_constant_block = {}
	) -> pipeline;

	auto create_image(
		device& dev,
		const image_desc& desc
	) -> image;

	auto create_sampler(
		device& dev,
		const sampler_desc& desc = {}
	) -> sampler;

	auto allocate_descriptors(
		device& dev,
		const shader& s
	) -> descriptor_region;

	auto create_descriptor_writer(
		device& dev,
		resource::handle<shader> s,
		descriptor_region& region
	) -> descriptor_writer;

	auto create_push_writer(
		device& dev,
		resource::handle<shader> s
	) -> descriptor_writer;

	struct buffer_upload {
		const buffer* dst = nullptr;
		const void* data = nullptr;
		std::size_t size = 0;
		std::size_t dst_offset = 0;
	};

	auto upload_to_buffers(
		device& dev,
		std::span<const buffer_upload> uploads
	) -> void;

	auto create_compute_queue(
		device& dev
	) -> compute_queue;

	auto upload_image_2d(
		device& dev,
		image& img,
		const void* pixel_data,
		std::size_t data_size
	) -> void;

	auto upload_image_layers(
		device& dev,
		image& img,
		const std::vector<const void*>& face_data,
		std::size_t bytes_per_face
	) -> void;

}

namespace {
	auto to_vk(gse::gpu::buffer_usage usage) -> vk::BufferUsageFlags {
		using enum gse::gpu::buffer_flag;
		vk::BufferUsageFlags flags;
		if (usage.test(uniform))      flags |= vk::BufferUsageFlagBits::eUniformBuffer;
		if (usage.test(storage))      flags |= vk::BufferUsageFlagBits::eStorageBuffer;
		if (usage.test(indirect))     flags |= vk::BufferUsageFlagBits::eIndirectBuffer;
		if (usage.test(transfer_dst)) flags |= vk::BufferUsageFlagBits::eTransferDst;
		if (usage.test(vertex))       flags |= vk::BufferUsageFlagBits::eVertexBuffer;
		if (usage.test(index))        flags |= vk::BufferUsageFlagBits::eIndexBuffer;
		if (usage.test(transfer_src)) flags |= vk::BufferUsageFlagBits::eTransferSrc;
		if (usage.test(acceleration_structure_storage)) {
			flags |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
			       | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		}
		if (usage.test(acceleration_structure_build_input)) {
			flags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
			       | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		}
		if (usage.test(uniform) || usage.test(storage) || usage.test(indirect)) {
			flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
		}
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

	auto to_vk(gse::gpu::image_layout l) -> vk::ImageLayout {
		switch (l) {
			case gse::gpu::image_layout::general:          return vk::ImageLayout::eGeneral;
			case gse::gpu::image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default:                                       return vk::ImageLayout::eUndefined;
		}
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

auto gse::gpu::create_buffer(device& dev, const buffer_desc& desc) -> buffer {
	auto vk_usage = to_vk(desc.usage);
	auto resource = dev.allocator().create_buffer({
		.size = static_cast<vk::DeviceSize>(desc.size),
		.usage = vk_usage
	}, desc.data);
	return buffer(std::move(resource));
}

auto gse::gpu::create_image(device& dev, const image_desc& desc) -> image {
	auto to_vk_format = [](image_format f) -> vk::Format {
		switch (f) {
			case image_format::d32_sfloat:       return vk::Format::eD32Sfloat;
			case image_format::r8g8b8a8_srgb:    return vk::Format::eR8G8B8A8Srgb;
			case image_format::r8g8b8a8_unorm:   return vk::Format::eR8G8B8A8Unorm;
			case image_format::r8g8b8_srgb:      return vk::Format::eR8G8B8Srgb;
			case image_format::r8g8b8_unorm:     return vk::Format::eR8G8B8Unorm;
			case image_format::r8_unorm:          return vk::Format::eR8Unorm;
			case image_format::b10g11r11_ufloat:      return vk::Format::eB10G11R11UfloatPack32;
			case image_format::r8g8_snorm:            return vk::Format::eR8G8Snorm;
			case image_format::r16g16b16a16_sfloat:   return vk::Format::eR16G16B16A16Sfloat;
		}
		return vk::Format::eD32Sfloat;
	};

	const auto vk_format = to_vk_format(desc.format);
	const bool is_depth = desc.format == image_format::d32_sfloat;
	const bool is_cube = desc.view == image_view_type::cube;
	const std::uint32_t layers = is_cube ? 6u : 1u;

	vk::ImageUsageFlags usage{};
	using enum image_flag;
	if (desc.usage.test(sampled))          usage |= vk::ImageUsageFlagBits::eSampled;
	if (desc.usage.test(depth_attachment)) usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	if (desc.usage.test(color_attachment)) usage |= vk::ImageUsageFlagBits::eColorAttachment;
	if (desc.usage.test(transfer_dst))     usage |= vk::ImageUsageFlagBits::eTransferDst;
	if (desc.usage.test(storage))          usage |= vk::ImageUsageFlagBits::eStorage;

	auto resource = dev.allocator().create_image(
		{
			.flags = is_cube ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{},
			.imageType = vk::ImageType::e2D,
			.format = vk_format,
			.extent = { desc.size.x(), desc.size.y(), 1 },
			.mipLevels = 1,
			.arrayLayers = layers,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = usage
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{
			.viewType = is_cube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
			.format = vk_format,
			.subresourceRange = {
				.aspectMask = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = layers
			}
		}
	);

	if (desc.ready_layout != image_layout::undefined) {
		auto to_vk_layout = [](image_layout l) -> vk::ImageLayout {
			switch (l) {
				case image_layout::general:          return vk::ImageLayout::eGeneral;
				case image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
				default:                             return vk::ImageLayout::eUndefined;
			}
		};

		const auto target = to_vk_layout(desc.ready_layout);
		const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
		const auto img = resource.image;

		dev.add_transient_work([img, target, aspect, layers, is_depth](const auto& cmd) {
			const auto dst_stage = is_depth
				? (vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
				: vk::PipelineStageFlagBits2::eAllCommands;
			const auto dst_access = is_depth
				? (vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead)
				: vk::AccessFlagBits2::eShaderRead;

			const vk::ImageMemoryBarrier2 barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = dst_stage,
				.dstAccessMask = dst_access,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = target,
				.image = img,
				.subresourceRange = {
					.aspectMask = aspect,
					.baseMipLevel = 0, .levelCount = 1,
					.baseArrayLayer = 0, .layerCount = layers
				}
			};

			const vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
			cmd.pipelineBarrier2(dep);

			return std::vector<vulkan::buffer_resource>{};
		});

		resource.current_layout = target;
	}

	std::vector<vk::raii::ImageView> layer_views;
	if (is_cube) {
		const auto& vk_device = dev.logical_device();
		const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
		layer_views.reserve(layers);
		for (std::uint32_t i = 0; i < layers; ++i) {
			layer_views.push_back(vk_device.createImageView({
				.image = resource.image,
				.viewType = vk::ImageViewType::e2D,
				.format = vk_format,
				.subresourceRange = {
					.aspectMask = aspect,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = i,
					.layerCount = 1
				}
			}));
		}
	}

	return image(std::move(resource), desc.format, desc.size, std::move(layer_views));
}

auto gse::gpu::create_graphics_pipeline(device& dev, const shader& s, const graphics_pipeline_desc& desc) -> pipeline {
	auto& vk_device = dev.logical_device();
	const auto& cache = dev.shader_cache(s);

	const auto& layouts = cache.layout_handles;

	const bool has_push = !desc.push_constant_block.empty();
	vk::PushConstantRange pc_range;
	if (has_push) {
		const auto pb = s.push_block(desc.push_constant_block);
		pc_range = vk::PushConstantRange{
			vulkan::to_vk_stage_flags(pb.stage_flags),
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

	const auto blend_attachment = make_blend_attachment(desc.blend);
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
			.format = vulkan::to_vk_format(a.format),
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

auto gse::gpu::create_compute_pipeline(device& dev, const shader& s, const std::string& push_constant_block) -> pipeline {
	auto& vk_device = dev.logical_device();
	const auto& cache = dev.shader_cache(s);

	const auto& layouts = cache.layout_handles;

	const bool has_push = !push_constant_block.empty();
	vk::PushConstantRange pc_range;
	if (has_push) {
		const auto pb = s.push_block(push_constant_block);
		pc_range = vk::PushConstantRange{
			vulkan::to_vk_stage_flags(pb.stage_flags),
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

auto gse::gpu::create_sampler(device& dev, const sampler_desc& desc) -> sampler {
	auto& vk_device = dev.logical_device();

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

	return sampler(vk_device.createSampler(info));
}

auto gse::gpu::allocate_descriptors(device& dev, const shader& s) -> descriptor_region {
	const auto& cache = dev.shader_cache(s);
	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	assert(persistent_idx < cache.layout_handles.size(), std::source_location::current(), "Shader has no persistent descriptor set to allocate");

	auto& heap = dev.descriptor_heap();
	const auto set_layout = cache.layout_handles[persistent_idx];
	const auto size = heap.layout_size(set_layout);
	return descriptor_region(heap.allocate(size));
}

auto gse::gpu::create_descriptor_writer(device& dev, resource::handle<shader> s, descriptor_region& region) -> descriptor_writer {
	return descriptor_writer(dev, std::move(s), region);
}

auto gse::gpu::create_push_writer(device& dev, resource::handle<shader> s) -> descriptor_writer {
	return descriptor_writer(dev, std::move(s));
}

auto gse::gpu::upload_to_buffers(device& dev, const std::span<const buffer_upload> uploads) -> void {
	if (uploads.empty()) return;

	struct upload_entry {
		vk::Buffer dst;
		const void* data;
		vk::DeviceSize size;
		vk::DeviceSize offset;
	};

	std::vector<upload_entry> entries;
	entries.reserve(uploads.size());
	for (std::size_t i = 0; i < uploads.size(); ++i) {
		const auto& [dst, data, size, dst_offset] = uploads[i];
		entries.push_back({
			.dst = dst->native().buffer,
			.data = data,
			.size = static_cast<vk::DeviceSize>(size),
			.offset = static_cast<vk::DeviceSize>(dst_offset)
		});
	}

	dev.add_transient_work([&dev, entries = std::move(entries)](const auto& cmd) {
		std::vector<vulkan::buffer_resource> transient;
		transient.reserve(entries.size());

		for (const auto& [dst, data, size, offset] : entries) {
			auto staging = dev.allocator().create_buffer({
				.size = size,
				.usage = vk::BufferUsageFlagBits::eTransferSrc
			}, data);

			cmd.copyBuffer(staging.buffer, dst, vk::BufferCopy(0, offset, size));
			transient.push_back(std::move(staging));
		}

		return transient;
	});
}

auto gse::gpu::upload_image_2d(device& dev, image& img, const void* pixel_data, const std::size_t data_size) -> void {
	vulkan::uploader::upload_image_2d(dev, img.native(), img.extent(), pixel_data, data_size, vk::ImageLayout::eShaderReadOnlyOptimal);
	img.set_layout(image_layout::shader_read_only);
}

auto gse::gpu::upload_image_layers(device& dev, image& img, const std::vector<const void*>& face_data, const std::size_t bytes_per_face) -> void {
	vulkan::uploader::upload_image_layers(dev, img.native(), img.extent(), face_data, bytes_per_face, vk::ImageLayout::eShaderReadOnlyOptimal);
	img.set_layout(image_layout::shader_read_only);
}

auto gse::gpu::create_compute_queue(device& dev) -> compute_queue {
	auto& vk_device = dev.logical_device();

	auto pool = vk_device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = dev.compute_queue_family()
	});

	auto cmd = std::move(vk_device.allocateCommandBuffers({
		.commandPool = *pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	}).front());

	auto fence = vk_device.createFence({
		.flags = vk::FenceCreateFlagBits::eSignaled
	});

	auto query_pool = vk_device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 4
	});

	static_cast<vk::Device>(*vk_device).resetQueryPool(*query_pool, 0, 4);

	const float timestamp_period = dev.physical_device().getProperties().limits.timestampPeriod;

	return compute_queue(
		std::move(pool),
		std::move(cmd),
		std::move(fence),
		std::move(query_pool),
		&dev.compute_queue(),
		&vk_device,
		&dev.descriptor_heap(),
		timestamp_period
	);
}
