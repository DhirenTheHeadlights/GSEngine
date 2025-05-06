module;

#include <cstddef>

export module gse.graphics.renderer3d;

import std;
import std.compat;
import vulkan_hpp;

import gse.core.config;
import gse.core.id;
import gse.core.object_registry;
import gse.graphics.camera;
import gse.graphics.mesh;
import gse.graphics.model;
import gse.graphics.render_component;
import gse.graphics.shader_loader;
import gse.platform;

gse::camera g_camera;

export namespace gse::renderer3d {
	auto initialize() -> void;
	auto initialize_objects() -> void;
	auto begin_frame() -> void;
	auto render() -> void;
	auto end_frame() -> void;
	auto shutdown() -> void;

	auto get_camera() -> camera&;
	auto get_render_pass() -> const vk::RenderPass&;
	auto get_current_command_buffer() -> vk::CommandBuffer;
}

vk::RenderPass g_render_pass;
vk::Pipeline g_pipeline;
vk::PipelineLayout g_pipeline_layout;

std::vector<vk::Framebuffer> g_deferred_frame_buffers;
std::vector<vk::CommandBuffer> g_command_buffers;

vk::Image g_position_image, g_normal_image, g_albedo_image, g_depth_image;
vk::DeviceMemory g_position_image_memory, g_normal_image_memory, g_albedo_image_memory, g_depth_image_memory;
vk::ImageView g_position_image_view, g_normal_image_view, g_albedo_image_view, g_depth_image_view;

auto gse::renderer3d::get_camera() -> camera& {
	return g_camera;
}

auto gse::renderer3d::get_render_pass() -> const vk::RenderPass& {
	return g_render_pass;
}

auto gse::renderer3d::get_current_command_buffer() -> vk::CommandBuffer {
	return g_command_buffers[vulkan::config::sync::current_frame];
}

auto gse::renderer3d::initialize() -> void {
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

	g_render_pass = vulkan::config::device::device.createRenderPass(render_pass_info);

	std::cout << "Render Pass Created Successfully!\n";

	// Position

	g_position_image = vulkan::config::device::device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eR16G16B16A16Sfloat,														// format (for position, high precision)
		vk::Extent3D{vulkan::config::swap_chain::extent.width, vulkan::config::swap_chain::extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements position_memory_requirements = vulkan::config::device::device.getImageMemoryRequirements(g_position_image);

	vk::MemoryAllocateInfo position_memory_alloc_info{
		position_memory_requirements.size,
		vulkan::find_memory_type(position_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory position_image_memory = vulkan::config::device::device.allocateMemory(position_memory_alloc_info);
	vulkan::config::device::device.bindImageMemory(g_position_image, position_image_memory, 0);

	g_position_image_view = vulkan::config::device::device.createImageView({
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

	g_normal_image = vulkan::config::device::device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eR16G16B16A16Sfloat,														// format (for normal, high precision)
		vk::Extent3D{vulkan::config::swap_chain::extent.width, vulkan::config::swap_chain::extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements normal_memory_requirements = vulkan::config::device::device.getImageMemoryRequirements(g_normal_image);

	vk::MemoryAllocateInfo normal_memory_alloc_info{
		normal_memory_requirements.size,
		vulkan::find_memory_type(normal_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory normal_image_memory = vulkan::config::device::device.allocateMemory(normal_memory_alloc_info);
	vulkan::config::device::device.bindImageMemory(g_normal_image, normal_image_memory, 0);

	g_normal_image_view = vulkan::config::device::device.createImageView({
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

	g_albedo_image = vulkan::config::device::device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eR16G16B16A16Sfloat,														// format (for albedo, high precision)
		vk::Extent3D{vulkan::config::swap_chain::extent.width, vulkan::config::swap_chain::extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements albedo_memory_requirements = vulkan::config::device::device.getImageMemoryRequirements(g_albedo_image);

	vk::MemoryAllocateInfo albedo_memory_alloc_info{
		albedo_memory_requirements.size,
		vulkan::find_memory_type(albedo_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory albedo_image_memory = vulkan::config::device::device.allocateMemory(albedo_memory_alloc_info);
	vulkan::config::device::device.bindImageMemory(g_albedo_image, albedo_image_memory, 0);

	g_albedo_image_view = vulkan::config::device::device.createImageView({
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

	g_depth_image = vulkan::config::device::device.createImage({
		{},																						// flags
		vk::ImageType::e2D,																		// type
		vk::Format::eD32Sfloat,																	// format (for depth, high precision)
		vk::Extent3D{vulkan::config::swap_chain::extent.width, vulkan::config::swap_chain::extent.height, 1 },
		1,																						// mipLevels
		1,																						// arrayLayers
		vk::SampleCountFlagBits::e1,															// samples
		vk::ImageTiling::eOptimal,																// tiling
		vk::ImageUsageFlagBits::eDepthStencilAttachment,										// usage flags
		vk::SharingMode::eExclusive,
		0, nullptr,
		vk::ImageLayout::eUndefined																// initial layout
		});

	vk::MemoryRequirements depth_memory_requirements = vulkan::config::device::device.getImageMemoryRequirements(g_depth_image);

	vk::MemoryAllocateInfo depth_memory_alloc_info{
		depth_memory_requirements.size,
		vulkan::find_memory_type(depth_memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	vk::DeviceMemory depth_image_memory = vulkan::config::device::device.allocateMemory(depth_memory_alloc_info);
	vulkan::config::device::device.bindImageMemory(g_depth_image, depth_image_memory, 0);

	g_depth_image_view = vulkan::config::device::device.createImageView({
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

	g_deferred_frame_buffers.resize(vulkan::config::swap_chain::frame_buffers.size());

	for (size_t i = 0; i < g_deferred_frame_buffers.size(); i++) {
		std::array all_attachments = {
			g_position_image_view, g_normal_image_view,
			g_albedo_image_view, g_depth_image_view
		};

		vk::FramebufferCreateInfo framebuffer_info(
			{}, g_render_pass, static_cast<std::uint32_t>(all_attachments.size()), all_attachments.data(),
			vulkan::config::swap_chain::extent.width, vulkan::config::swap_chain::extent.height, 1
		);

		g_deferred_frame_buffers[i] = vulkan::config::device::device.createFramebuffer(framebuffer_info);
	}

	std::cout << "G-Buffer Framebuffers Created Successfully!\n";

	g_command_buffers.resize(vulkan::config::swap_chain::frame_buffers.size());

	vk::CommandBufferAllocateInfo alloc_info(
		vulkan::config::command::pool, vk::CommandBufferLevel::ePrimary,
		static_cast<std::uint32_t>(g_command_buffers.size())
	);

	g_command_buffers = vulkan::config::device::device.allocateCommandBuffers(alloc_info);

	std::cout << "Command Buffers Created Successfully!\n";

	auto transition_image_layout = [](const vk::Image image, const vk::ImageLayout old_layout, const vk::ImageLayout new_layout, const bool depth = false) -> void {
		const vk::CommandBufferAllocateInfo cmd_buffer_alloc_info(
			vulkan::config::command::pool, vk::CommandBufferLevel::ePrimary, 1
		);

		const vk::CommandBuffer command_buffer = vulkan::config::device::device.allocateCommandBuffers(cmd_buffer_alloc_info)[0];

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
				depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
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
		else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eColorAttachmentOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		}
		else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
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

		vulkan::config::queue::graphics.submit(submit_info, nullptr);
		};

	transition_image_layout(g_position_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	transition_image_layout(g_normal_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	transition_image_layout(g_albedo_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	transition_image_layout(g_depth_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, true);

	const auto& geometry_shader = shader_loader::get_shader("geometry_pass");
	const auto& layout = shader_loader::get_descriptor_layout(descriptor_layout::standard_3d);

	std::array descriptor_set_layouts = { *layout };

	const auto descriptor_pool = vulkan::config::descriptor::pool;

	vk::DescriptorSetAllocateInfo descriptor_alloc_info = {
		descriptor_pool, static_cast<std::uint32_t>(descriptor_set_layouts.size()), descriptor_set_layouts.data()
	};

	std::vector descriptor_sets = vulkan::config::device::device.allocateDescriptorSets(descriptor_alloc_info);

	auto vertex_descriptor_set = descriptor_sets[0];

	struct camera_ubo {
		mat4 view;
		mat4 projection;
	};

	vk::DeviceMemory camera_ubo_memory;
	vk::Buffer camera_buffer = vulkan::create_buffer(
		sizeof(camera_ubo), 
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		camera_ubo_memory
	);

	vk::DescriptorBufferInfo camera_buffer_info(camera_buffer, 0, sizeof(camera_ubo));

	struct model_ubo {
		mat4 model;
	};

	vk::DeviceMemory model_ubo_memory;
	vk::Buffer model_buffer = vulkan::create_buffer(
		sizeof(model_ubo),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		model_ubo_memory
	);

	vk::DescriptorBufferInfo model_buffer_info(model_buffer, 0, sizeof(model_ubo));

	std::array<vk::WriteDescriptorSet, 2> descriptor_writes = { {
		vk::WriteDescriptorSet(
			vertex_descriptor_set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &camera_buffer_info, nullptr
		),
		vk::WriteDescriptorSet(
			vertex_descriptor_set, 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &model_buffer_info, nullptr
		)
	} };

	vulkan::config::device::device.updateDescriptorSets(descriptor_writes, nullptr);

	g_pipeline_layout = vulkan::config::device::device.createPipelineLayout({ {}, static_cast<std::uint32_t>(descriptor_set_layouts.size()), descriptor_set_layouts.data() });

	vk::VertexInputBindingDescription binding_description(0, sizeof(vertex), vk::VertexInputRate::eVertex);

	std::array attribute_descriptions{
		vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, position)),
		vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, normal)),
		vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, tex_coords))
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_info({}, 1, &binding_description, static_cast<std::uint32_t>(attribute_descriptions.size()), attribute_descriptions.data());
	vk::PipelineInputAssemblyStateCreateInfo input_assembly({}, vk::PrimitiveTopology::eTriangleList, vk::False);

	vk::Viewport viewport(0.f, 0.f, static_cast<float>(vulkan::config::swap_chain::extent.width), static_cast<float>(vulkan::config::swap_chain::extent.height), 0.f, 1.f);
	vk::Rect2D scissor({ 0, 0 }, vulkan::config::swap_chain::extent);
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

	vk::PipelineMultisampleStateCreateInfo multisampling(
		{},
		vk::SampleCountFlagBits::e1,   // No multi-sampling, but still needs to be valid
		vk::False,
		1.0f,
		nullptr,
		vk::False,
		vk::False
	);

	vk::GraphicsPipelineCreateInfo pipeline_info(
		{}, 
		static_cast<std::uint32_t>(shader_stages.size()), 
		shader_stages.data(), 
		&vertex_input_info, 
		&input_assembly,
		nullptr, 
		&viewport_state, 
		&rasterizer,
		&multisampling,
		&depth_stencil, 
		&color_blending, 
		nullptr, 
		g_pipeline_layout, 
		g_render_pass, 0
	);
	g_pipeline = vulkan::config::device::device.createGraphicsPipeline(nullptr, pipeline_info).value;
}

auto gse::renderer3d::initialize_objects() -> void {
	
}

auto gse::renderer3d::begin_frame() -> void {
	assert(
		vulkan::config::device::device.waitForFences(1, &vulkan::config::sync::in_flight_fence, vk::True, std::numeric_limits<std::uint64_t>::max()) == vk::Result::eSuccess,
		"Failed to wait for fence!"
	);

	assert(vulkan::config::device::device.resetFences(1, &vulkan::config::sync::in_flight_fence) == vk::Result::eSuccess, "Failed to reset fence!");

	const auto image_index = vulkan::get_next_image(window::get_window());

	constexpr vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	g_command_buffers[image_index].begin(begin_info);

	std::array<vk::ClearValue, 4> clear_values;
	clear_values[0].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
	clear_values[1].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
	clear_values[2].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
	clear_values[3].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

	const vk::RenderPassBeginInfo render_pass_info(
		g_render_pass,
		g_deferred_frame_buffers[image_index],
		{ {0, 0}, vulkan::config::swap_chain::extent },
		static_cast<std::uint32_t>(clear_values.size()), 
		clear_values.data()
	);

	g_command_buffers[image_index].beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
}

auto gse::renderer3d::render() -> void {
	for (const auto& components = registry::get_components<render_component>(); const auto& component : components) {
		for (const auto& model_handle : component.models) {
			for (const auto& entry : model_handle.get_render_queue_entries()) {
				g_command_buffers[0].bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

				entry.mesh->bind(g_command_buffers[0]);
				entry.mesh->draw(g_command_buffers[0]);
			}
		}
	}
}

auto gse::renderer3d::end_frame() -> void {
	g_command_buffers[vulkan::config::sync::current_frame].endRenderPass();
	g_command_buffers[vulkan::config::sync::current_frame].end();

	vk::Semaphore wait_semaphores[] = { vulkan::config::sync::image_available_semaphore };
	vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	const vk::SubmitInfo info(
		1, wait_semaphores, wait_stages,
		1, &g_command_buffers[vulkan::config::sync::current_frame],
		1, &vulkan::config::sync::render_finished_semaphore
	);

	vulkan::config::queue::graphics.submit(info, vulkan::config::sync::in_flight_fence);

	const vk::PresentInfoKHR present_info(
		1, &vulkan::config::sync::render_finished_semaphore,
		1, &vulkan::config::swap_chain::swap_chain,
		&vulkan::config::sync::current_frame
	);

	const vk::Result present_result = vulkan::config::queue::present.presentKHR(present_info);
	assert(present_result == vk::Result::eSuccess || present_result == vk::Result::eSuboptimalKHR, "Failed to present image!");
}

auto gse::renderer3d::shutdown() -> void {
	vulkan::config::device::device.destroyPipeline(g_pipeline);
	vulkan::config::device::device.destroyPipelineLayout(g_pipeline_layout);
	vulkan::config::device::device.destroyRenderPass(g_render_pass);
	for (const auto& framebuffer : g_deferred_frame_buffers) {
		vulkan::config::device::device.destroyFramebuffer(framebuffer);
	}
}