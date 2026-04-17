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
import :gpu_context;
import :shader;
import :render_graph;
import :vulkan_enums;
import :shader_registry;

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
		context& ctx,
		const shader& s,
		const graphics_pipeline_desc& desc = {}
	) -> pipeline;

	auto create_compute_pipeline(
		context& ctx,
		const shader& s,
		const std::string& push_constant_block = {}
	) -> pipeline;

	auto create_image(
		context& ctx,
		const image_desc& desc
	) -> image;

	auto create_sampler(
		device& dev,
		const sampler_desc& desc = {}
	) -> sampler;

	auto allocate_descriptors(
		context& ctx,
		const shader& s
	) -> descriptor_region;

	auto create_descriptor_writer(
		context& ctx,
		resource::handle<shader> s,
		descriptor_region& region
	) -> descriptor_writer;

	auto create_push_writer(
		context& ctx,
		resource::handle<shader> s
	) -> descriptor_writer;

	struct buffer_upload {
		const buffer* dst = nullptr;
		const void* data = nullptr;
		std::size_t size = 0;
		std::size_t dst_offset = 0;
	};

	auto upload_to_buffers(
		context& ctx,
		std::span<const buffer_upload> uploads
	) -> void;

	auto create_compute_queue(
		device& dev
	) -> compute_queue;

	auto upload_image_2d(
		context& ctx,
		image& img,
		const void* pixel_data,
		std::size_t data_size
	) -> void;

	auto upload_image_layers(
		context& ctx,
		image& img,
		const std::vector<const void*>& face_data,
		std::size_t bytes_per_face
	) -> void;

}

auto gse::gpu::create_buffer(device& dev, const buffer_desc& desc) -> buffer {
	auto vk_usage = vulkan::to_vk(desc.usage);
	auto resource = dev.allocator().create_buffer({
		.size = static_cast<vk::DeviceSize>(desc.size),
		.usage = vk_usage
	}, desc.data);
	return buffer(std::move(resource));
}

auto gse::gpu::create_image(context& ctx, const image_desc& desc) -> image {
	auto& dev = ctx.device_ref();
	const auto vk_format = vulkan::to_vk(desc.format);
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
	if (desc.usage.test(transfer_src))     usage |= vk::ImageUsageFlagBits::eTransferSrc;

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
		const auto target = vulkan::to_vk(desc.ready_layout);
		const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
		const auto img = resource.image;

		ctx.frame().add_transient_work([img, target, aspect, layers, is_depth](const auto& cmd) {
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

auto gse::gpu::create_graphics_pipeline(context& ctx, const shader& s, const graphics_pipeline_desc& desc) -> pipeline {
	auto& dev = ctx.device_ref();
	auto& vk_device = dev.logical_device();
	const auto& cache = ctx.shader_registry().cache(s);

	const auto& layouts = cache.layout_handles;

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

	const auto blend_attachment = vulkan::to_vk(desc.blend);
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

auto gse::gpu::create_compute_pipeline(context& ctx, const shader& s, const std::string& push_constant_block) -> pipeline {
	auto& vk_device = ctx.device_ref().logical_device();
	const auto& cache = ctx.shader_registry().cache(s);

	const auto& layouts = cache.layout_handles;

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

auto gse::gpu::create_sampler(device& dev, const sampler_desc& desc) -> sampler {
	auto& vk_device = dev.logical_device();

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

auto gse::gpu::allocate_descriptors(context& ctx, const shader& s) -> descriptor_region {
	const auto& cache = ctx.shader_registry().cache(s);
	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	assert(persistent_idx < cache.layout_handles.size(), std::source_location::current(), "Shader has no persistent descriptor set to allocate");

	auto& heap = ctx.device_ref().descriptor_heap();
	const auto set_layout = cache.layout_handles[persistent_idx];
	const auto size = heap.layout_size(set_layout);
	return descriptor_region(heap.allocate(size));
}

auto gse::gpu::create_descriptor_writer(context& ctx, resource::handle<shader> s, descriptor_region& region) -> descriptor_writer {
	return descriptor_writer(ctx, std::move(s), region);
}

auto gse::gpu::create_push_writer(context& ctx, resource::handle<shader> s) -> descriptor_writer {
	return descriptor_writer(ctx, std::move(s));
}

auto gse::gpu::upload_to_buffers(context& ctx, const std::span<const buffer_upload> uploads) -> void {
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

	auto& dev = ctx.device_ref();
	ctx.frame().add_transient_work([&dev, entries = std::move(entries)](const auto& cmd) {
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

auto gse::gpu::upload_image_2d(context& ctx, image& img, const void* pixel_data, const std::size_t data_size) -> void {
	const auto extent = img.extent();
	auto& resource = img.native();
	auto& dev = ctx.device_ref();

	ctx.frame().add_transient_work([&dev, &resource, extent, pixel_data, data_size](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
		auto staging = dev.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = data_size,
				.usage = vk::BufferUsageFlagBits::eTransferSrc
			},
			pixel_data
		);

		vulkan::uploader::transition_image_layout(
			*cmd, resource,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTopOfPipe, {},
			vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
		);

		const vk::BufferImageCopy region{
			.bufferOffset = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.imageExtent = { extent.x(), extent.y(), 1 }
		};

		cmd.copyBufferToImage(staging.buffer, resource.image, vk::ImageLayout::eTransferDstOptimal, region);

		vulkan::uploader::transition_image_layout(
			*cmd, resource,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead
		);

		std::vector<vulkan::buffer_resource> buffers;
		buffers.push_back(std::move(staging));
		return buffers;
	});

	img.set_layout(image_layout::shader_read_only);
}

auto gse::gpu::upload_image_layers(context& ctx, image& img, const std::vector<const void*>& face_data, const std::size_t bytes_per_face) -> void {
	const auto extent = img.extent();
	auto& resource = img.native();
	auto& dev = ctx.device_ref();
	const std::uint32_t layer_count = static_cast<std::uint32_t>(face_data.size());
	const std::size_t total_size = layer_count * bytes_per_face;

	ctx.frame().add_transient_work([&dev, &resource, &face_data, extent, layer_count, total_size, bytes_per_face](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
		auto staging = dev.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = total_size,
				.usage = vk::BufferUsageFlagBits::eTransferSrc
			}
		);

		const auto mapped = staging.allocation.mapped();
		for (std::size_t i = 0; i < layer_count; ++i) {
			gse::memcpy(mapped + i * bytes_per_face, face_data[i], bytes_per_face);
		}

		vulkan::uploader::transition_image_layout(
			*cmd, resource,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTopOfPipe, {},
			vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
			1, layer_count
		);

		std::vector<vk::BufferImageCopy> regions;
		regions.reserve(layer_count);
		for (std::uint32_t i = 0; i < layer_count; ++i) {
			regions.emplace_back(vk::BufferImageCopy{
				.bufferOffset = i * bytes_per_face,
				.imageSubresource = {
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = i,
					.layerCount = 1
				},
				.imageExtent = { extent.x(), extent.y(), 1 }
			});
		}

		cmd.copyBufferToImage(staging.buffer, resource.image, vk::ImageLayout::eTransferDstOptimal, regions);

		vulkan::uploader::transition_image_layout(
			*cmd, resource,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor,
			vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
			1, layer_count
		);

		std::vector<vulkan::buffer_resource> buffers;
		buffers.push_back(std::move(staging));
		return buffers;
	});

	img.set_layout(image_layout::shader_read_only);
}


auto gse::gpu::create_compute_queue(device& dev) -> compute_queue {
	const auto& vk_device = dev.logical_device();

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

	constexpr vk::SemaphoreTypeCreateInfo timeline_type{
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0
	};
	auto timeline = vk_device.createSemaphore({
		.pNext = &timeline_type
	});

	auto query_pool = vk_device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = compute_queue::timing_slot_count
	});

	query_pool.reset(0, compute_queue::timing_slot_count);

	const float timestamp_period = dev.physical_device().getProperties().limits.timestampPeriod;

	return compute_queue(
		std::move(pool),
		std::move(cmd),
		std::move(fence),
		std::move(timeline),
		std::move(query_pool),
		&dev.compute_queue(),
		&vk_device,
		&dev.descriptor_heap(),
		timestamp_period
	);
}
