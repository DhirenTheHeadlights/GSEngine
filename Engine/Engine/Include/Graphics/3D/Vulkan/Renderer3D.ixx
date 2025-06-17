module;

#include <cstddef>
#include "GLFW/glfw3.h"

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
import gse.graphics.texture_loader;
import gse.graphics.point_light;
import gse.graphics.light_source_component;
import gse.graphics.shader;
import gse.physics.math;
import gse.platform;

gse::camera g_camera;

export namespace gse::renderer3d {
	auto initialize(vulkan::config& config) -> void;
	auto initialize_objects() -> void;
	auto render(const vulkan::config& config) -> void;
	auto shutdown(const vulkan::config& config) -> void;

	auto get_camera() -> camera&;
}

vk::raii::Pipeline g_pipeline = nullptr;
vk::raii::PipelineLayout g_pipeline_layout = nullptr;

vk::raii::Pipeline g_lighting_pipeline = nullptr;
vk::raii::PipelineLayout g_lighting_pipeline_layout = nullptr;

vk::raii::DescriptorSet g_vertex_descriptor_set = nullptr;
vk::raii::DescriptorSet g_lighting_descriptor_set = nullptr;

std::unordered_map<std::string, gse::vulkan::persistent_allocator::buffer_resource> g_ubo_allocations;

auto gse::renderer3d::get_camera() -> camera& {
	return g_camera;
}

auto gse::renderer3d::initialize(vulkan::config& config) -> void {
	const auto cmd = begin_single_line_commands(config);

	vulkan::uploader::transition_image_layout(cmd, config.swap_chain_data.position_image.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, config.swap_chain_data.normal_image.image, vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, config.swap_chain_data.albedo_image.image, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 1, 1);
	vulkan::uploader::transition_image_layout(cmd, config.swap_chain_data.depth_image.image, vk::Format::eD32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1, 1);

	end_single_line_commands(cmd, config);

	const auto& geometry_shader = shader_loader::get_shader("geometry_pass");
	auto descriptor_set_layouts = geometry_shader.layouts();
	g_vertex_descriptor_set = geometry_shader.descriptor_set(config.device_data.device, config.descriptor.pool, shader::set::binding_type::persistent);

	std::unordered_map<std::string, vk::Buffer> uniform_buffers;
	std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos;

	for (auto [name, binding, set, size, members] : geometry_shader.uniform_blocks()) {
		if (set != static_cast<uint32_t>(shader::set::binding_type::persistent)) {
			continue;
		}

		if (name == "model_ubo") {
			std::size_t max_entries = 1024;
			vk::DeviceSize model_stride = size;
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

		uniform_buffers[name] = buffer.buffer;
		buffer_infos[name] = vk::DescriptorBufferInfo(buffer.buffer, 0, size);
		g_ubo_allocations[name] = std::move(buffer);
	}

	std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

	config.device_data.device.updateDescriptorSets(
		geometry_shader.descriptor_writes(g_vertex_descriptor_set, buffer_infos, image_infos), 
		nullptr
	);

	g_pipeline_layout = config.device_data.device.createPipelineLayout(
		{
			{},
			descriptor_set_layouts
		}
	);

	const auto& lighting_shader = shader_loader::get_shader("lighting_pass");
	auto lighting_layout = lighting_shader.layout(shader::set::binding_type::persistent);

	vk::DescriptorSetAllocateInfo lighting_alloc_info{
		config.descriptor.pool,
		1,
		&lighting_layout
	};
	g_lighting_descriptor_set = std::move(config.device_data.device.allocateDescriptorSets(lighting_alloc_info)[0]);

	vk::DescriptorImageInfo position_input_info{
		nullptr,
		config.swap_chain_data.position_image.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};
	vk::DescriptorImageInfo normal_input_info{
		nullptr,
		config.swap_chain_data.normal_image.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};
	vk::DescriptorImageInfo albedo_input_info{
		nullptr,
		config.swap_chain_data.albedo_image.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};

	std::vector<vk::WriteDescriptorSet> lighting_writes;

	auto bind_idx = lighting_shader.binding("g_position")->binding;
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

	bind_idx = lighting_shader.binding("g_normal")->binding; // should be 1
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

	bind_idx = lighting_shader.binding("g_albedo_spec")->binding;
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
		&lighting_layout
	};
	g_lighting_pipeline_layout = config.device_data.device.createPipelineLayout(lighting_pipeline_layout_info);

	auto lighting_stages = lighting_shader.shader_stages();

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
		config.swap_chain_data.render_pass,
		1,                             
		nullptr,
		-1
	};

	g_lighting_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, lighting_pipeline_info);

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

	auto shader_stages = geometry_shader.shader_stages();

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
		config.swap_chain_data.render_pass, 
		0
	);
	g_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info);
}

auto gse::renderer3d::initialize_objects() -> void {
	const auto& light_source_components = registry::get_components<light_source_component>();

	for (const auto& light_source_component : light_source_components) {
		for (const auto light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
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

	auto& geometry_shader = shader_loader::get_shader("geometry_pass");
	const auto model_size = geometry_shader.uniform_block("model_ubo").size;

	if (input::get_keyboard().keys[GLFW_KEY_F12].pressed) {
		std::print("Camera view matrix: {}\n", g_camera.get_view_matrix());
		std::print("Camera projection matrix: {}\n", g_camera.get_projection_matrix());
	}

	geometry_shader.set_uniform("camera_ubo.view", g_camera.get_view_matrix(), g_ubo_allocations.at("camera_ubo").allocation);
	geometry_shader.set_uniform("camera_ubo.proj", g_camera.get_projection_matrix(), g_ubo_allocations.at("camera_ubo").allocation);

	command.bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

	vulkan::persistent_allocator::mapped_buffer_view<mat4> model_view{
		.base = g_ubo_allocations.at("model_ubo").allocation.mapped(),
		.stride = model_size,
	};

	std::size_t model_index = 0;
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

				if (entry.mesh->material.exists()) {
					geometry_shader.push(
						command,
						g_pipeline_layout,
						"diffuse_sampler",
						texture_loader::get_texture(entry.mesh->material->diffuse_texture).get_descriptor_info()
					);

					entry.mesh->bind(command);
					entry.mesh->draw(command);
				}
			}
		}
	}

	command.nextSubpass(vk::SubpassContents::eInline);
	command.bindPipeline(vk::PipelineBindPoint::eGraphics, g_lighting_pipeline);
	command.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		g_lighting_pipeline_layout,
		0, 1, &*g_lighting_descriptor_set,
		0, nullptr
	);
	command.draw(3, 1, 0, 0);
}

auto gse::renderer3d::shutdown(const vulkan::config& config) -> void {

}