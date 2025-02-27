export module gse.graphics.renderer3d;

import std;
import vulkan_hpp;

import gse.core.config;
import gse.core.id;
import gse.core.object_registry;
import gse.graphics.camera;
import gse.graphics.debug;
import gse.graphics.mesh;
import gse.graphics.model;
import gse.graphics.render_component;
import gse.graphics.shader;
import gse.platform.glfw.window;
import gse.platform.assert;
import gse.platform.context;

gse::camera g_camera;

export namespace gse::renderer3d {
	auto initialize() -> void;
	auto initialize_objects() -> void;
	auto render() -> void;

	auto get_camera() -> const camera&;
}

vk::RenderPass g_render_pass;
vk::Pipeline g_pipeline;
vk::PipelineLayout g_pipeline_layout;

std::vector<vk::Framebuffer> g_frame_buffers;
std::vector<vk::CommandBuffer> g_command_buffers;

vk::Image g_position_image, g_normal_image, g_albedo_image, g_depth_image;
vk::DeviceMemory g_position_image_memory, g_normal_image_memory, g_albedo_image_memory, g_depth_image_memory;
vk::ImageView g_position_image_view, g_normal_image_view, g_albedo_image_view, g_depth_image_view;

auto gse::renderer3d::get_camera() -> const camera& {
	return g_camera;
}

auto gse::renderer3d::initialize() -> void {
	const auto [physical_device, device] = vulkan::get_device_config();
	const auto [swap_chain, surface_format, present_mode, extent, swap_chain_frame_buffers, swap_chain_images, swap_chain_image_views, swap_chain_image_format, details] = vulkan::get_swap_chain_config(window::get_window());

	constexpr std::array attachments{
		vk::AttachmentDescription{ // Position
			{},
			vk::Format::eR16G16B16A16Sfloat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal
		},
		vk::AttachmentDescription{ // Normal
			{},
			vk::Format::eR16G16B16A16Sfloat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal
		},
		vk::AttachmentDescription{ // Albedo Specular
			{},
			vk::Format::eR16G16B16A16Sfloat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal
		},
		vk::AttachmentDescription{ // Depth
			{},
			vk::Format::eD32Sfloat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal
		}
	};

	constexpr std::array color_attachment_refs{
		vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal},
		vk::AttachmentReference{1, vk::ImageLayout::eColorAttachmentOptimal},
		vk::AttachmentReference{2, vk::ImageLayout::eColorAttachmentOptimal},
	};
	constexpr vk::AttachmentReference depth_attachment_ref{ 3, vk::ImageLayout::eDepthStencilAttachmentOptimal };

	const vk::SubpassDescription sub_pass(
		{}, vk::PipelineBindPoint::eGraphics,
		0, nullptr,
		static_cast<std::uint32_t>(color_attachment_refs.size()), color_attachment_refs.data(),
		nullptr, &depth_attachment_ref
	);

	constexpr vk::SubpassDependency dependency(
		vk::SubpassExternal, 0,
		vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
		{}, vk::AccessFlagBits::eColorAttachmentWrite
	);

	const vk::RenderPassCreateInfo render_pass_info(
		{}, static_cast<std::uint32_t>(attachments.size()), attachments.data(),
		1, &sub_pass, 1, &dependency
	);

	g_render_pass = device.createRenderPass(render_pass_info);

	std::cout << "Render Pass Created Successfully!\n";

	g_frame_buffers.resize(g_swap_chain_image_views.size());

	for (size_t i = 0; i < g_swap_chain_image_views.size(); i++) {
		std::array all_attachments = {
			g_position_image_view, g_normal_image_view,
			g_albedo_image_view, g_depth_image_view
		};

		vk::FramebufferCreateInfo framebuffer_info(
			{}, g_render_pass, static_cast<std::uint32_t>(all_attachments.size()), all_attachments.data(),
			g_swap_chain_extent.width, g_swap_chain_extent.height, 1
		);

		g_swap_chain_frame_buffers[i] = device.createFramebuffer(framebuffer_info);
	}

	std::cout << "G-Buffer Framebuffers Created Successfully!\n";

	g_command_buffers.resize(g_swap_chain_frame_buffers.size());

	vk::CommandBufferAllocateInfo alloc_info(
		vulkan::get_command_pool(), vk::CommandBufferLevel::ePrimary,
		static_cast<std::uint32_t>(g_command_buffers.size())
	);

	g_command_buffers = device.allocateCommandBuffers(alloc_info);

	std::cout << "Command Buffers Created Successfully!\n";

	// Position

	g_position_image = device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eR16G16B16A16Sfloat,														// format (for position, high precision)
		vk::Extent3D{ g_swap_chain_extent.width, g_swap_chain_extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements position_memory_requirements = device.getImageMemoryRequirements(g_position_image);

	vk::MemoryAllocateInfo position_memory_alloc_info{
		position_memory_requirements.size,
		vulkan::find_memory_type(position_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory position_image_memory = device.allocateMemory(position_memory_alloc_info);
	device.bindImageMemory(g_position_image, position_image_memory, 0);

	g_position_image_view = device.createImageView({
		{},
		g_position_image,
		vk::ImageViewType::e2D,
		vk::Format::eR16G16B16A16Sfloat,
		{}, // components (default mapping)
		vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eColor, // aspect
			0, 1, 0, 1                       // mipLevels and arrayLayers
			}
		}
	);

	// Normal

	g_normal_image = device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eR16G16B16A16Sfloat,														// format (for normal, high precision)
		vk::Extent3D{ g_swap_chain_extent.width, g_swap_chain_extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements normal_memory_requirements = device.getImageMemoryRequirements(g_normal_image);

	vk::MemoryAllocateInfo normal_memory_alloc_info{
		normal_memory_requirements.size,
		vulkan::find_memory_type(normal_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory normal_image_memory = device.allocateMemory(normal_memory_alloc_info);
	device.bindImageMemory(g_normal_image, normal_image_memory, 0);

	g_normal_image_view = device.createImageView({
		{},
		g_normal_image,
		vk::ImageViewType::e2D,
		vk::Format::eR16G16B16A16Sfloat,
		{}, // components (default mapping)
		vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eColor, // aspect
			0, 1, 0, 1                       // mipLevels and arrayLayers
			}
		}
	);

	// Albedo

	g_albedo_image = device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eR16G16B16A16Sfloat,														// format (for albedo, high precision)
		vk::Extent3D{ g_swap_chain_extent.width, g_swap_chain_extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements albedo_memory_requirements = device.getImageMemoryRequirements(g_albedo_image);

	vk::MemoryAllocateInfo albedo_memory_alloc_info{
		albedo_memory_requirements.size,
		vulkan::find_memory_type(albedo_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory albedo_image_memory = device.allocateMemory(albedo_memory_alloc_info);
	device.bindImageMemory(g_albedo_image, albedo_image_memory, 0);

	g_albedo_image_view = device.createImageView({
		{},
		g_albedo_image,
		vk::ImageViewType::e2D,
		vk::Format::eR16G16B16A16Sfloat,
		{}, // components (default mapping)
		vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eColor, // aspect
			0, 1, 0, 1                       // mipLevels and arrayLayers
			}
		}
	);

	// Depth

	g_depth_image = device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eD32Sfloat,																	// format (for depth, high precision)
		vk::Extent3D{ g_swap_chain_extent.width, g_swap_chain_extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eDepthStencilAttachment,										// usage flags
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements depth_memory_requirements = device.getImageMemoryRequirements(g_depth_image);

	vk::MemoryAllocateInfo depth_memory_alloc_info{
		depth_memory_requirements.size,
		vulkan::find_memory_type(depth_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory depth_image_memory = device.allocateMemory(depth_memory_alloc_info);
	device.bindImageMemory(g_depth_image, depth_image_memory, 0);

	g_depth_image_view = device.createImageView({
		{},
		g_depth_image,
		vk::ImageViewType::e2D,
		vk::Format::eD32Sfloat,
		{}, // components (default mapping)
		vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eDepth, // aspect
			0, 1, 0, 1                       // mipLevels and arrayLayers
			}
		}
	);

	auto transition_image_layout = [device](const vk::Image image, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout) -> void {
		const vk::CommandBufferAllocateInfo cmd_buffer_alloc_info(
			vulkan::get_command_pool(), vk::CommandBufferLevel::ePrimary, 1
		);

		const vk::CommandBuffer command_buffer = device.allocateCommandBuffers(cmd_buffer_alloc_info)[0];

		constexpr vk::CommandBufferBeginInfo begin_info(
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit
		);

		command_buffer.begin(begin_info);

		vk::ImageMemoryBarrier barrier(
			{},
			{},
			old_layout,
			new_layout,
			vk::QueueFamilyIgnored,
			vk::QueueFamilyIgnored,
			image,
			vk::ImageSubresourceRange{
				vk::ImageAspectFlagBits::eColor,
				0, 1, 0, 1
			}
		);

		vk::PipelineStageFlags source_stage;
		vk::PipelineStageFlags destination_stage;

		if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			destination_stage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			source_stage = vk::PipelineStageFlagBits::eTransfer;
			destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else {
			throw std::invalid_argument("Unsupported layout transition!");
		}

		command_buffer.pipelineBarrier(
			source_stage, destination_stage,
			{}, 0, nullptr, 0, nullptr, 1, &barrier
		);

		command_buffer.end();

		const vk::SubmitInfo submit_info(
			0, nullptr, nullptr, 1, &command_buffer, 0, nullptr
		);

		vulkan::get_graphics_queue().submit(submit_info, nullptr);
		};

	transition_image_layout(g_position_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	transition_image_layout(g_normal_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	transition_image_layout(g_albedo_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	transition_image_layout(g_depth_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	g_pipeline_layout = device.createPipelineLayout({});

	shader geometry_shader(config::resource_path / "DeferredRendering/geometry_pass.vert", config::resource_path / "DeferredRendering/geometry_pass.frag");

	vk::PipelineVertexInputStateCreateInfo vertex_input_info({}, 0, nullptr, 0, nullptr);
	vk::PipelineInputAssemblyStateCreateInfo input_assembly({}, vk::PrimitiveTopology::eTriangleList, vk::False);

	vk::Viewport viewport(0.f, 0.f, static_cast<float>(g_swap_chain_extent.width), static_cast<float>(g_swap_chain_extent.height), 0.f, 1.f);
	vk::Rect2D scissor({ 0, 0 }, g_swap_chain_extent);
	vk::PipelineViewportStateCreateInfo viewport_state({}, 1, & viewport, 1, & scissor);

	vk::PipelineRasterizationStateCreateInfo rasterizer(
		{}, vk::False, vk::False, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise,
		vk::False, 0.f, 0.f, 0.f, 1.f
	);

	vk::PipelineDepthStencilStateCreateInfo depth_stencil({}, vk::True, vk::True, vk::CompareOp::eLess);

	std::array<vk::PipelineColorBlendAttachmentState, 3> color_blend_attachments;
	for (auto& attachment : color_blend_attachments) {
		attachment = vk::PipelineColorBlendAttachmentState(vk::False, {}, {}, {}, {}, {}, {},
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	}
	vk::PipelineColorBlendStateCreateInfo color_blending({}, vk::False, vk::LogicOp::eCopy, static_cast<std::uint32_t>(color_blend_attachments.size()), color_blend_attachments.data());

	auto shader_stages = geometry_shader.get_shader_stages();

	vk::GraphicsPipelineCreateInfo pipeline_info(
		{}, static_cast<std::uint32_t>(shader_stages.size()), shader_stages.data(), &vertex_input_info, &input_assembly,
		nullptr, &viewport_state, &rasterizer, nullptr, &depth_stencil, &color_blending, nullptr, g_pipeline_layout, g_render_pass, 0
	);
	g_pipeline = device.createGraphicsPipeline(nullptr, pipeline_info).value;

	debug::set_up_imgui(g_render_pass);
}

auto gse::renderer3d::render() -> void {
	for (const auto components = registry::get_components<render_component>(); const auto component : components) {
		for (const auto model_handle : component.models) {
			for (const auto entry : model_handle.get_render_queue_entries()) {
				g_command_buffers[0].bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

				entry.mesh->bind(g_command_buffers[0]);
				entry.mesh->draw(g_command_buffers[0]);
			}
		}
	}

	debug::render_imgui(g_command_buffers[0]);
}