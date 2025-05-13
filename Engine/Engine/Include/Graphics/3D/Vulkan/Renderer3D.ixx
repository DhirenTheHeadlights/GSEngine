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
import gse.graphics.point_light;
import gse.graphics.light_source_component;
import gse.platform;

gse::camera g_camera;

export namespace gse::renderer3d {
	auto initialize(vulkan::config& config) -> void;
	auto initialize_objects() -> void;
	auto begin_frame(const vulkan::config& config) -> void;
	auto render(vulkan::config& config) -> void;
	auto end_frame(const vulkan::config& config) -> void;
	auto shutdown(const vulkan::config& config) -> void;

	auto get_camera() -> camera&;
	auto get_render_pass() -> const vk::RenderPass&;
	auto get_current_command_buffer() -> vk::CommandBuffer;
}

vk::RenderPass g_render_pass;
vk::Pipeline g_pipeline;
vk::PipelineLayout g_pipeline_layout;

std::vector<vk::Framebuffer> g_deferred_frame_buffers;

gse::vulkan::persistent_allocator::image_resource g_position_image_resource;
gse::vulkan::persistent_allocator::image_resource g_normal_image_resource;
gse::vulkan::persistent_allocator::image_resource g_albedo_image_resource;
gse::vulkan::persistent_allocator::image_resource g_depth_image_resource;

auto gse::renderer3d::get_camera() -> camera& {
	return g_camera;
}

auto gse::renderer3d::get_render_pass() -> const vk::RenderPass& {
	return g_render_pass;
}

auto gse::renderer3d::initialize(vulkan::config& config) -> void {
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
		vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal },
		vk::AttachmentReference{ 1, vk::ImageLayout::eColorAttachmentOptimal },
		vk::AttachmentReference{ 2, vk::ImageLayout::eColorAttachmentOptimal },
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

	g_render_pass = config.device_data.device.createRenderPass(render_pass_info);

	std::cout << "Render Pass Created Successfully!\n";

	// Position

	vk::ImageViewCreateInfo position_image_view_info(
		{},
		nullptr,
		vk::ImageViewType::e2D,
		vk::Format::eR16G16B16A16Sfloat,
		{}, // components (default mapping)
		vk::ImageSubresourceRange{
			vk::ImageAspectFlagBits::eColor, // aspect
			0, 1, 0, 1                       // mipLevels and arrayLayers
			}
	);

	g_position_image_resource = vulkan::persistent_allocator::create_image(
		config.device_data,
		{
			{},																						// flags
			vk::ImageType::e2D,																		// type
			vk::Format::eR16G16B16A16Sfloat,														// format (for position, high precision)
			vk::Extent3D{ config.swap_chain_data.extent.width, config.swap_chain_data.extent.height, 1 },
			1,																						// mipLevels
			1,																						// arrayLayers
			vk::SampleCountFlagBits::e1,															// samples
			vk::ImageTiling::eOptimal,																// tiling
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,	// usage flags (input for later shader stages if needed)
			vk::SharingMode::eExclusive,
			0, nullptr,
			vk::ImageLayout::eUndefined																// initial layout
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		position_image_view_info
	);

	// Normal

	vk::ImageViewCreateInfo normal_view_info(
		{}, nullptr,
		vk::ImageViewType::e2D,
		vk::Format::eR16G16B16A16Sfloat,
		{},
		{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
	);

	g_normal_image_resource = vulkan::persistent_allocator::create_image(
		config.device_data,
		{
			{}, vk::ImageType::e2D,
			vk::Format::eR16G16B16A16Sfloat,
			{ config.swap_chain_data.extent.width, config.swap_chain_data.extent.height, 1 },
			1, 1,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::SharingMode::eExclusive,
			0, nullptr,
			vk::ImageLayout::eUndefined
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		normal_view_info
	);

	// Albedo

	vk::ImageViewCreateInfo albedo_view_info(
		{}, nullptr,
		vk::ImageViewType::e2D,
		vk::Format::eR16G16B16A16Sfloat,
		{},
		{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
	);

	g_albedo_image_resource = vulkan::persistent_allocator::create_image(
		config.device_data,
		{
			{}, vk::ImageType::e2D,
			vk::Format::eR16G16B16A16Sfloat,
			{ config.swap_chain_data.extent.width, config.swap_chain_data.extent.height, 1 },
			1, 1,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::SharingMode::eExclusive,
			0, nullptr,
			vk::ImageLayout::eUndefined
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		albedo_view_info
	);

	// Depth

	vk::ImageViewCreateInfo depth_view_info(
		{}, nullptr,
		vk::ImageViewType::e2D,
		vk::Format::eD32Sfloat,
		{},
		{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 }
	);

	g_depth_image_resource = vulkan::persistent_allocator::create_image(
		config.device_data,
		{
			{}, vk::ImageType::e2D,
			vk::Format::eD32Sfloat,
			{ config.swap_chain_data.extent.width, config.swap_chain_data.extent.height, 1 },
			1, 1,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::SharingMode::eExclusive,
			0, nullptr,
			vk::ImageLayout::eUndefined
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		depth_view_info
	);

	g_deferred_frame_buffers.resize(config.swap_chain_data.frame_buffers.size());

	for (size_t i = 0; i < g_deferred_frame_buffers.size(); i++) {
		std::array all_attachments = {
			g_position_image_resource.view,
			g_normal_image_resource.view,
			g_albedo_image_resource.view,
			g_depth_image_resource.view
		};

		vk::FramebufferCreateInfo framebuffer_info(
			{}, g_render_pass, static_cast<std::uint32_t>(all_attachments.size()), all_attachments.data(),
			config.swap_chain_data.extent.width, config.swap_chain_data.extent.height, 1
		);

		g_deferred_frame_buffers[i] = config.device_data.device.createFramebuffer(framebuffer_info);
	}

	std::cout << "G-Buffer Framebuffers Created Successfully!\n";


	const vk::CommandBuffer cmd = begin_single_line_commands(config);

	vulkan::uploader::transition_image_layout(cmd, g_position_image_resource.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, g_normal_image_resource.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, g_albedo_image_resource.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, g_depth_image_resource.image, vk::Format::eD32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1, 1);

	end_single_line_commands(cmd, config);

	const auto& geometry_shader = shader_loader::get_shader("geometry_pass");
	const auto& layout = shader_loader::get_descriptor_layout(descriptor_layout::standard_3d);

	std::array descriptor_set_layouts = { *layout };

	const auto descriptor_pool = config.descriptor.pool;

	vk::DescriptorSetAllocateInfo descriptor_alloc_info = {
		descriptor_pool, static_cast<std::uint32_t>(descriptor_set_layouts.size()), descriptor_set_layouts.data()
	};

	std::vector descriptor_sets = config.device_data.device.allocateDescriptorSets(descriptor_alloc_info);

	auto vertex_descriptor_set = descriptor_sets[0];

	struct camera_ubo {
		mat4 view;
		mat4 projection;
	};

	vk::BufferCreateInfo camera_info(
		{},                  
		sizeof(camera_ubo),          
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive
	);

	auto [camera_buffer, camera_alloc] = vulkan::persistent_allocator::create_buffer(
		config.device_data,
		camera_info,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	vk::DescriptorBufferInfo camera_buffer_info(camera_buffer, 0, sizeof(camera_ubo));

	struct model_ubo {
		mat4 model;
	};

	vk::BufferCreateInfo model_info(
		{},
		sizeof(model_ubo),              
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive
	);

	auto [model_buffer, model_alloc] = vulkan::persistent_allocator::create_buffer(
		config.device_data,
		model_info,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
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

	config.device_data.device.updateDescriptorSets(descriptor_writes, nullptr);

	g_pipeline_layout = config.device_data.device.createPipelineLayout({ {}, static_cast<std::uint32_t>(descriptor_set_layouts.size()), descriptor_set_layouts.data() });

	vk::VertexInputBindingDescription binding_description(0, sizeof(vertex), vk::VertexInputRate::eVertex);

	std::array attribute_descriptions{
		vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, position)),
		vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, normal)),
		vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, tex_coords))
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_info({}, 1, &binding_description, static_cast<std::uint32_t>(attribute_descriptions.size()), attribute_descriptions.data());
	vk::PipelineInputAssemblyStateCreateInfo input_assembly({}, vk::PrimitiveTopology::eTriangleList, vk::False);

	vk::Viewport viewport(0.f, 0.f, static_cast<float>(config.swap_chain_data.extent.width), static_cast<float>(config.swap_chain_data.extent.height), 0.f, 1.f);
	vk::Rect2D scissor({ 0, 0 }, config.swap_chain_data.extent);
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
	g_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info).value;
}

auto gse::renderer3d::initialize_objects() -> void {
	const auto& light_source_components = registry::get_components<light_source_component>();

	for (const auto& light_source_component : light_source_components) {
		for (const auto light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
				point_light_ptr->get_shadow_map().create({}, {});
			}
		}
	}
}

auto gse::renderer3d::begin_frame(const vulkan::config& config) -> void {
	std::array<vk::ClearValue, 4> clear_values;
	clear_values[0].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
	clear_values[1].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
	clear_values[2].color = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
	clear_values[3].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

	const vk::RenderPassBeginInfo render_pass_info(
		g_render_pass,
		g_deferred_frame_buffers[config.frame_context.image_index],
		{ {0, 0}, config.swap_chain_data.extent },
		static_cast<std::uint32_t>(clear_values.size()), 
		clear_values.data()
	);

	config.command.buffers[config.frame_context.image_index].beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
}

auto gse::renderer3d::render(vulkan::config& config) -> void {
	for (const auto& components = registry::get_components<render_component>(); const auto& component : components) {
		for (const auto& model_handle : component.models) {
			for (const auto& entry : model_handle.get_render_queue_entries()) {

			}
		}
	}
}

auto gse::renderer3d::end_frame(const vulkan::config& config) -> void {
	config.frame_context.command_buffer.endRenderPass();
}

auto gse::renderer3d::shutdown(const vulkan::config& config) -> void {
	config.device_data.device.destroyPipeline(g_pipeline);
	config.device_data.device.destroyPipelineLayout(g_pipeline_layout);
	config.device_data.device.destroyRenderPass(g_render_pass);
	for (const auto& framebuffer : g_deferred_frame_buffers) {
		config.device_data.device.destroyFramebuffer(framebuffer);
	}

	for (const auto& light_source_components = registry::get_components<light_source_component>(); const auto& light_source_component : light_source_components) {
		for (const auto& light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
				point_light_ptr->get_shadow_map().destroy({});
			}
		}
	}
}