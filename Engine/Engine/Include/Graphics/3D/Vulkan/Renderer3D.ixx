export module gse.graphics:renderer3d;

import std;

import :camera;
import :mesh;
import :model;
import :render_component;
import :shader_loader;
import :texture_loader;
import :point_light;
import :light_source_component;
import :material;
import :shader;

import gse.physics.math;
import gse.utility;
import gse.platform;

gse::camera g_camera;

export namespace gse::renderer3d {
	struct context {
		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		vk::raii::Pipeline lighting_pipeline = nullptr;
		vk::raii::PipelineLayout lighting_pipeline_layout = nullptr;
		vk::raii::DescriptorSet lighting_descriptor_set = nullptr;
		vk::raii::Sampler buffer_sampler = nullptr;
	};

	auto initialize(context& context, vulkan::config& config) -> void;
	auto initialize_objects(std::span<light_source_component> light_source_components) -> void;
	auto render_geometry(const context& context, const vulkan::config& config, std::span<render_component> render_components) -> void;
	auto render_lighting(const context& context, const vulkan::config& config, std::span<render_component> render_components) -> void;

	auto camera() -> camera&;
}

auto gse::renderer3d::camera() -> class camera& {
	return g_camera;
}

auto gse::renderer3d::initialize(context& context, vulkan::config& config) -> void {
	const auto cmd = begin_single_line_commands(config);

	vulkan::uploader::transition_image_layout(
		cmd, config.swap_chain_data.position_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eTopOfPipe, {},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
	);

	vulkan::uploader::transition_image_layout(
		cmd, config.swap_chain_data.normal_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eTopOfPipe, {},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
	);

	vulkan::uploader::transition_image_layout(
		cmd, config.swap_chain_data.albedo_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eTopOfPipe, {},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
	);

	vulkan::uploader::transition_image_layout(
		cmd, config.swap_chain_data.depth_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
		vk::ImageAspectFlagBits::eDepth,
		vk::PipelineStageFlagBits2::eTopOfPipe, {}, // Source: Nothing has happened yet
		vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite // Destination: Will be written during depth testing
	);

	end_single_line_commands(cmd, config);

	const auto& geometry_shader = shader_loader::shader("geometry_pass");
	auto descriptor_set_layouts = geometry_shader.layouts();

	std::vector ranges = {
		geometry_shader.push_constant_range("push_constants", vk::ShaderStageFlagBits::eVertex)
	};

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
		.pSetLayouts = descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size()),
		.pPushConstantRanges = ranges.data()
	};

	context.pipeline_layout = config.device_data.device.createPipelineLayout(pipeline_layout_info);

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
	context.buffer_sampler = config.device_data.device.createSampler(sampler_create_info);

	const auto& lighting_shader = shader_loader::shader("lighting_pass");
	auto lighting_layouts = lighting_shader.layouts();

	context.lighting_descriptor_set = lighting_shader.descriptor_set(config.device_data.device, config.descriptor.pool, shader::set::binding_type::persistent);

	const std::unordered_map<std::string, vk::DescriptorImageInfo> lighting_image_infos = {
		{
			"g_position",
			{
				.sampler = *context.buffer_sampler,
				.imageView = *config.swap_chain_data.position_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		},
		{
			"g_normal",
			{
				.sampler = *context.buffer_sampler,
				.imageView = *config.swap_chain_data.normal_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		},
		{
			"g_albedo_spec",
			{
				.sampler = *context.buffer_sampler,
				.imageView = *config.swap_chain_data.albedo_image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			}
		}
	};

	auto lighting_writes = lighting_shader.descriptor_writes(
		context.lighting_descriptor_set,
		{},
		lighting_image_infos
	);

	config.device_data.device.updateDescriptorSets(lighting_writes, nullptr);

	const vk::PipelineLayoutCreateInfo lighting_pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(lighting_layouts.size()),
		.pSetLayouts = lighting_layouts.data()
	};
	context.lighting_pipeline_layout = config.device_data.device.createPipelineLayout(lighting_pipeline_layout_info);

	auto lighting_stages = lighting_shader.shader_stages();

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
		.layout = context.lighting_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};
	context.lighting_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, lighting_pipeline_info);

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly2{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer2{
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

	std::array<vk::PipelineColorBlendAttachmentState, 3> color_blend_attachments;
	for (auto& attachment : color_blend_attachments) {
		attachment = {
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};
	}

	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = static_cast<std::uint32_t>(color_blend_attachments.size()),
		.pAttachments = color_blend_attachments.data()
	};

	auto shader_stages = geometry_shader.shader_stages();

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	auto vertex_input_info = geometry_shader.vertex_input_state();

	const std::array<vk::Format, 3> g_buffer_color_formats = {
		vk::Format::eR16G16B16A16Sfloat,
		vk::Format::eR16G16B16A16Sfloat,
		vk::Format::eR8G8B8A8Srgb
	};

	const vk::PipelineRenderingCreateInfoKHR geometry_rendering_info = {
		.colorAttachmentCount = static_cast<uint32_t>(g_buffer_color_formats.size()),
		.pColorAttachmentFormats = g_buffer_color_formats.data(),
		.depthAttachmentFormat = vk::Format::eD32Sfloat
	};

	const vk::GraphicsPipelineCreateInfo pipeline_info{
		.pNext = &geometry_rendering_info,
		.stageCount = static_cast<std::uint32_t>(shader_stages.size()),
		.pStages = shader_stages.data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly2,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer2,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = nullptr,
		.layout = context.pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	context.pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info);
}

auto gse::renderer3d::initialize_objects(std::span<light_source_component> const light_source_components) -> void {
	for (const auto& light_source_component : light_source_components) {
		for (const auto light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
			}
		}
	}
}

auto gse::renderer3d::render_geometry(const context& context, const vulkan::config& config, const std::span<render_component> render_components) -> void {
	const auto command = config.frame_context.command_buffer;

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_data.position_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
	);
	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_data.normal_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
	);
	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_data.albedo_image.image,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
	);

	std::array color_attachments{
		vk::RenderingAttachmentInfo{
			.imageView = *config.swap_chain_data.position_image.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }}}
		},
		vk::RenderingAttachmentInfo{
			.imageView = *config.swap_chain_data.normal_image.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }}}
		},
		vk::RenderingAttachmentInfo{
			.imageView = *config.swap_chain_data.albedo_image.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }}}
		}
	};

	vk::RenderingAttachmentInfo depth_attachment{
		.imageView = *config.swap_chain_data.depth_image.view,
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{.depth = 1.0f}}
	};

	const vk::RenderingInfo geometry_rendering_info{
		.renderArea = {{0, 0}, config.swap_chain_data.extent},
		.layerCount = 1,
		.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
		.pColorAttachments = color_attachments.data(),
		.pDepthAttachment = &depth_attachment
	};

	render(config, geometry_rendering_info, 
		[&] {
			g_camera.update_camera_vectors();
			if (!window::is_mouse_visible()) g_camera.process_mouse_movement(window::get_mouse_delta_rel_top_left());

			if (render_components.empty()) {
				return;
			}

			auto& geometry_shader = shader_loader::shader("geometry_pass");
			command.bindPipeline(vk::PipelineBindPoint::eGraphics, context.pipeline);

			const auto view = g_camera.view();
			const auto proj = g_camera.projection();

			std::unordered_map<std::string, std::span<const std::byte>> push_constants = {
				{"view", std::as_bytes(std::span{ &view, 1 })},
				{"proj", std::as_bytes(std::span{ &proj, 1 })}
			};

			for (const auto& component : render_components) {
				for (const auto& model_handle : component.models) {
					for (const auto& entry : model_handle.render_queue_entries()) {
						push_constants["model"] = std::as_bytes(std::span{ &entry.model_matrix, 1 });

						geometry_shader.push(
							command,
							context.pipeline_layout,
							"push_constants",
							push_constants,
							vk::ShaderStageFlagBits::eVertex
						);

						if (entry.mesh->material.exists()) {
							geometry_shader.push(
								command,
								context.pipeline_layout,
								"diffuse_sampler",
								texture_loader::texture(entry.mesh->material->diffuse_texture).get_descriptor_info()
							);

							entry.mesh->bind(command);
							entry.mesh->draw(command);
						}
					}
				}
			}
		}
	);

	vulkan::uploader::transition_image_layout(
		command, 
		config.swap_chain_data.position_image.image,
		vk::ImageLayout::eColorAttachmentOptimal, 
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderRead
	);

	vulkan::uploader::transition_image_layout(
		command, 
		config.swap_chain_data.normal_image.image,
		vk::ImageLayout::eColorAttachmentOptimal, 
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader, 
		vk::AccessFlagBits2::eShaderRead
	);

	vulkan::uploader::transition_image_layout(
		command, 
		config.swap_chain_data.albedo_image.image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderRead
	);
}

auto gse::renderer3d::render_lighting(const context& context, const vulkan::config& config, std::span<render_component> render_components) -> void {
	const auto command = config.frame_context.command_buffer;

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_data.image_views[config.frame_context.image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }}}
	};

	const vk::RenderingInfo lighting_rendering_info{
		.renderArea = {{0, 0}, config.swap_chain_data.extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr
	};

	render(config, lighting_rendering_info, [&] {
			command.bindPipeline(vk::PipelineBindPoint::eGraphics, context.lighting_pipeline);
			command.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				*context.lighting_pipeline_layout,
				0,
				1,
				&*context.lighting_descriptor_set,
				0,
				nullptr
			);
			command.draw(3, 1, 0, 0);
		}
	);
}