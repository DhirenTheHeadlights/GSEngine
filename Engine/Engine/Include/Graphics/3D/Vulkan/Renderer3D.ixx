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
	auto render(const vulkan::config& config) -> void;
	auto shutdown(const vulkan::config& config) -> void;

	auto get_camera() -> camera&;
}

vk::Pipeline g_pipeline;
vk::PipelineLayout g_pipeline_layout;

vk::Pipeline g_lighting_pipeline;
vk::PipelineLayout g_lighting_pipeline_layout;

vk::DescriptorSet g_vertex_descriptor_set;
vk::DescriptorSet g_lighting_descriptor_set;

std::unordered_map<std::string, gse::vulkan::persistent_allocator::buffer_resource> g_ubo_allocations;

auto gse::renderer3d::get_camera() -> camera& {
	return g_camera;
}

auto gse::renderer3d::initialize(vulkan::config& config) -> void {
	const vk::CommandBuffer cmd = begin_single_line_commands(config);

	vulkan::uploader::transition_image_layout(cmd, config.position_image.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, config.normal_image.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, config.albedo_image.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, config.depth_image.image, vk::Format::eD32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1, 1);

	end_single_line_commands(cmd, config);

	const auto& geometry_shader = shader_loader::get_shader("geometry_pass");
	const auto& layout = shader_loader::get_descriptor_layout(descriptor_layout::standard_3d);

	std::array descriptor_set_layouts = { *layout };

	const auto descriptor_pool = config.descriptor.pool;

	vk::DescriptorSetAllocateInfo descriptor_alloc_info = {
		descriptor_pool, static_cast<std::uint32_t>(descriptor_set_layouts.size()), descriptor_set_layouts.data()
	};

	std::vector descriptor_sets = config.device_data.device.allocateDescriptorSets(descriptor_alloc_info);

	g_vertex_descriptor_set = descriptor_sets[0];

	std::unordered_map<std::string, vk::Buffer> uniform_buffers;
	std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos;

	for (const auto& [block_name, block] : geometry_shader.get_uniform_blocks()) {
		auto size = block.size;

		if (block_name == "model_ubo") {
			size_t max_entries = 1024;
			vk::DeviceSize model_stride = block.size;
			size = model_stride * max_entries;
		}

		vk::BufferCreateInfo buffer_info(
			{},
			size,
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::SharingMode::eExclusive
		);

		auto buffer = vulkan::persistent_allocator::create_buffer(
			config.device_data,
			buffer_info,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		g_ubo_allocations[block_name] = buffer;
		uniform_buffers[block_name] = buffer.buffer;
		buffer_infos[block_name] = vk::DescriptorBufferInfo(buffer.buffer, 0, block.size);
	}

	std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

	auto writes = geometry_shader.get_descriptor_writes(g_vertex_descriptor_set, buffer_infos, image_infos);

	config.device_data.device.updateDescriptorSets(writes, nullptr);

	g_pipeline_layout = config.device_data.device.createPipelineLayout({ {}, static_cast<std::uint32_t>(descriptor_set_layouts.size()), descriptor_set_layouts.data() });

	const auto& lighting_shader = shader_loader::get_shader("lighting_pass");

	auto lighting_layout_ptr = lighting_shader.get_descriptor_set_layout();

	vk::DescriptorSetAllocateInfo lighting_alloc_info{
		config.descriptor.pool,
		1,
		lighting_layout_ptr
	};
	g_lighting_descriptor_set = config.device_data.device.allocateDescriptorSets(lighting_alloc_info)[0];

	vk::DescriptorImageInfo position_input_info{
		nullptr,
		config.position_image.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};
	vk::DescriptorImageInfo normal_input_info{
		nullptr,
		config.normal_image.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};
	vk::DescriptorImageInfo albedo_input_info{
		nullptr,
		config.albedo_image.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};

	std::vector<vk::WriteDescriptorSet> lighting_writes;

	auto bind_idx = lighting_shader.get_binding("g_position")->binding;
	lighting_writes.emplace_back(
		g_lighting_descriptor_set,   // dstSet
		bind_idx,                  // dstBinding
		0,                         // dstArrayElement
		1,                         // descriptorCount
		vk::DescriptorType::eInputAttachment,
		&position_input_info,      // pImageInfo
		nullptr,                   // pBufferInfo
		nullptr                    // pTexelBufferView
	);

	bind_idx = lighting_shader.get_binding("g_normal")->binding; // should be 1
	lighting_writes.emplace_back(
		g_lighting_descriptor_set,
		bind_idx,
		0,
		1,
		vk::DescriptorType::eInputAttachment,
		&normal_input_info,
		nullptr,
		nullptr
	);

	bind_idx = lighting_shader.get_binding("g_albedo_spec")->binding; // should be 2
	lighting_writes.emplace_back(
		g_lighting_descriptor_set,
		bind_idx,
		0,
		1,
		vk::DescriptorType::eInputAttachment,
		&albedo_input_info,
		nullptr,
		nullptr
	);

	config.device_data.device.updateDescriptorSets(lighting_writes, nullptr);

	vk::PipelineLayoutCreateInfo lighting_pipeline_layout_info{
		{},
		1,
		lighting_layout_ptr
	};
	g_lighting_pipeline_layout = config.device_data.device.createPipelineLayout(lighting_pipeline_layout_info);

	auto lighting_stages = lighting_shader.get_shader_stages();

	vk::PipelineVertexInputStateCreateInfo empty_vertex_input{
		{},
		0, 
		nullptr,
		0,
		nullptr
	};

	vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		{}, vk::PrimitiveTopology::eTriangleList, vk::False
	};

	vk::Viewport viewport(
		0.0f, 0.0f,
		static_cast<float>(config.swap_chain_data.extent.width),
		static_cast<float>(config.swap_chain_data.extent.height),
		0.0f, 1.0f
	);

	vk::Rect2D scissor({ 0,0 }, config.swap_chain_data.extent);
	vk::PipelineViewportStateCreateInfo viewport_state{
		{}, 1, &viewport, 1, &scissor
	};

	vk::PipelineRasterizationStateCreateInfo rasterizer{
		{}, vk::False, vk::False,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eCounterClockwise,
		vk::False, 0, 0, 0, 1.0f
	};

	vk::PipelineDepthStencilStateCreateInfo* depth_stencil_state = nullptr;

	vk::PipelineMultisampleStateCreateInfo multi_sample_state{
		{}, vk::SampleCountFlagBits::e1, vk::False, 1.0f,
		nullptr, vk::False, vk::False
	};

	vk::PipelineColorBlendAttachmentState color_blend_attachment{
		vk::False,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR |
		vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB |
		vk::ColorComponentFlagBits::eA
	};
	vk::PipelineColorBlendStateCreateInfo color_blend_state{
		{}, vk::False, vk::LogicOp::eCopy, 1, &color_blend_attachment
	};

	vk::GraphicsPipelineCreateInfo lighting_pipeline_info{
		{},
		static_cast<std::uint32_t>(lighting_stages.size()),
		lighting_stages.data(),
		&empty_vertex_input,
		&input_assembly,
		nullptr,
		&viewport_state,
		&rasterizer,
		&multi_sample_state,
		depth_stencil_state,           
		&color_blend_state,
		nullptr,                       
		g_lighting_pipeline_layout,    
		config.render_pass,                 
		1,                             
		nullptr,
		-1
	};

	auto result = config.device_data.device.createGraphicsPipeline(nullptr, lighting_pipeline_info);
	assert(result.result == vk::Result::eSuccess, "Failed to create lighting‐pass pipeline");

	g_lighting_pipeline = result.value;

	vk::VertexInputBindingDescription binding_description(0, sizeof(vertex), vk::VertexInputRate::eVertex);

	std::array attribute_descriptions{
		vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, position)),
		vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, normal)),
		vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, tex_coords))
	};

	vk::PipelineVertexInputStateCreateInfo vertex_input_info({}, 1, &binding_description, static_cast<std::uint32_t>(attribute_descriptions.size()), attribute_descriptions.data());
	vk::PipelineInputAssemblyStateCreateInfo input_assembly2({}, vk::PrimitiveTopology::eTriangleList, vk::False);

	vk::Viewport viewport2(0.f, 0.f, static_cast<float>(config.swap_chain_data.extent.width), static_cast<float>(config.swap_chain_data.extent.height), 0.f, 1.f);
	vk::Rect2D scissor2({ 0, 0 }, config.swap_chain_data.extent);
	vk::PipelineViewportStateCreateInfo viewport_state2({}, 1, &viewport2, 1, &scissor2);

	vk::PipelineRasterizationStateCreateInfo rasterizer2(
		{},                                 // flags
		vk::False,                          // depthClampEnable
		vk::False,                          // rasterizerDiscardEnable
		vk::PolygonMode::eFill,             // polygonMode
		vk::CullModeFlagBits::eBack,        // cullMode (or eNone for testing)
		vk::FrontFace::eCounterClockwise,   // frontFace
		vk::True,                           // depthBiasEnable 
		2.0f,                               // depthBiasConstantFactor (experiment with this)
		0.0f,                               // depthBiasClamp (usually 0.0)
		2.0f,                               // depthBiasSlopeFactor (experiment with this)
		1.0f                                // lineWidth (usually 1.0 for solid fill)
	);

	vk::PipelineDepthStencilStateCreateInfo depth_stencil({
		{},               // flags
		vk::True,         // depthTestEnable
		vk::True,         // depthWriteEnable 
		vk::CompareOp::eLess, // compareOp (pixels with smaller Z pass)
		vk::False,        // depthBoundsTestEnable
		vk::False,        // stencilTestEnable
		{},               // front
		{},               // back
		0.0f,             // minDepthBounds
		1.0f              // maxDepthBounds
		}
	);

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
		&input_assembly2,
		nullptr, 
		&viewport_state2, 
		&rasterizer2,
		&multisampling,
		&depth_stencil, 
		&color_blending, 
		nullptr, 
		g_pipeline_layout, 
		config.render_pass, 
		0
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

auto gse::renderer3d::render(const vulkan::config& config) -> void {
	g_camera.update_camera_vectors();
	if (!window::is_mouse_visible()) g_camera.process_mouse_movement(window::get_mouse_delta_rel_top_left());

	const auto components = registry::get_components<render_component>();
	const auto command = config.frame_context.command_buffer;

	if (components.empty()) {
		command.nextSubpass(vk::SubpassContents::eInline);
		return;
	}

	const auto& geometry_shader = shader_loader::get_shader("geometry_pass");
	const auto model_size = geometry_shader.get_uniform_blocks().at("model_ubo").size;

	geometry_shader.set_uniform("camera_ubo.view", g_camera.get_view_matrix(), g_ubo_allocations.at("camera_ubo").allocation);
	geometry_shader.set_uniform("camera_ubo.proj", g_camera.get_projection_matrix(), g_ubo_allocations.at("camera_ubo").allocation);

	command.bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

	vulkan::persistent_allocator::mapped_buffer_view<mat4> model_view{
		.base = g_ubo_allocations.at("model_ubo").allocation.mapped,
		.stride = model_size,
	};

	size_t model_index = 0;
	for (const auto& component : components) {
		for (const auto& model_handle : component.models) {
			for (const auto& entry : model_handle.get_render_queue_entries()) {
				model_view[model_index] = entry.model_matrix;

				model_index++;

				const vk::DescriptorSet sets[] = { g_vertex_descriptor_set };

				command.bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics,
					g_pipeline_layout,
					0,
					vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
					{}
				);

				entry.mesh->bind(command);
				entry.mesh->draw(command);
			}
		}
	}

	command.nextSubpass(vk::SubpassContents::eInline);
	command.bindPipeline(vk::PipelineBindPoint::eGraphics, g_lighting_pipeline);
	command.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		g_lighting_pipeline_layout,
		0, 1, &g_lighting_descriptor_set,
		0, nullptr
	);
	command.draw(3, 1, 0, 0);
}

auto gse::renderer3d::shutdown(const vulkan::config& config) -> void {
	config.device_data.device.destroyPipeline(g_pipeline);
	config.device_data.device.destroyPipelineLayout(g_pipeline_layout);
	config.device_data.device.destroyRenderPass(config.render_pass);

	for (const auto& light_source_components = registry::get_components<light_source_component>(); const auto& light_source_component : light_source_components) {
		for (const auto& light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
				point_light_ptr->get_shadow_map().destroy({});
			}
		}
	}

	for (auto& resource : g_ubo_allocations | std::views::values) {
		free(config.device_data, resource);
	}
}