﻿export module gse.graphics:geometry_renderer;

import std;

import :base_renderer;
import :camera;
import :mesh;
import :model;
import :render_component;
import :material;
import :shader;

import gse.physics.math;
import gse.utility;
import gse.platform;
import gse.physics;

export namespace gse::renderer {
	class geometry final : public base_renderer {
	public:
		explicit geometry(context& context) : base_renderer(context) {}
		auto initialize() -> void override;
		auto render(std::span<std::reference_wrapper<registry>> registries) -> void override;
	private:
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		vk::raii::DescriptorSet m_descriptor_set = nullptr;

		resource::handle<shader> m_shader;

		std::unordered_map<std::string, vulkan::persistent_allocator::buffer_resource> m_ubo_allocations;
	};
}

auto gse::renderer::geometry::initialize() -> void {
	auto& config = m_context.config();

	config.add_transient_work(
		[&](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::persistent_allocator::buffer_resource> {
			vulkan::uploader::transition_image_layout(
				cmd, config.swap_chain_config().albedo_image,
				vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor,
				vk::PipelineStageFlagBits2::eTopOfPipe,
				{}, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::AccessFlagBits2::eColorAttachmentWrite
			);

			vulkan::uploader::transition_image_layout(
				cmd, config.swap_chain_config().depth_image,
				vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth,
				vk::PipelineStageFlagBits2::eTopOfPipe,
				{}, vk::PipelineStageFlagBits2::eEarlyFragmentTests,
				vk::AccessFlagBits2::eDepthStencilAttachmentWrite
			);

			return {};
		}
	);

	m_shader = m_context.get<shader>("geometry_pass");
	m_context.instantly_load(m_shader);
	auto descriptor_set_layouts = m_shader->layouts();

	m_descriptor_set = m_shader->descriptor_set(
		config.device_config().device,
		config.descriptor_config().pool,
		shader::set::binding_type::persistent
	);

	const auto camera_ubo = m_shader->uniform_block("CameraUBO");

	vk::BufferCreateInfo camera_ubo_buffer_info{
		.size = camera_ubo.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	auto camera_ubo_buffer = vulkan::persistent_allocator::create_buffer(
		config.device_config(),
		camera_ubo_buffer_info
	);

	m_ubo_allocations["CameraUBO"] = std::move(camera_ubo_buffer);

	std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
		{
			"CameraUBO",
			{
				.buffer = m_ubo_allocations["CameraUBO"].buffer,
				.offset = 0,
				.range = camera_ubo.size
			}
		}
	};

	std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

	config.device_config().device.updateDescriptorSets(
		m_shader->descriptor_writes(m_descriptor_set, buffer_infos, image_infos),
		nullptr
	);

	std::vector ranges = {
		m_shader->push_constant_range("push_constants"),
	};

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
		.pSetLayouts = descriptor_set_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size()),
		.pPushConstantRanges = ranges.data()
	};

	m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eClockwise,
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

	const std::array g_buffer_color_formats = {
		config.swap_chain_config().albedo_image.format,
		config.swap_chain_config().normal_image.format,
	};

	std::array<vk::PipelineColorBlendAttachmentState, g_buffer_color_formats.size()> color_blend_attachments;
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

	auto shader_stages = m_shader->shader_stages();

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	auto vertex_input_info = m_shader->vertex_input_state();

	const vk::PipelineRenderingCreateInfoKHR geometry_rendering_info = {
		.colorAttachmentCount = static_cast<uint32_t>(g_buffer_color_formats.size()),
		.pColorAttachmentFormats = g_buffer_color_formats.data(),
		.depthAttachmentFormat = vk::Format::eD32Sfloat
	};

	const vk::Viewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(config.swap_chain_config().extent.width),
		.height = static_cast<float>(config.swap_chain_config().extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	const vk::Rect2D scissor{
		.offset = { 0, 0 },
		.extent = config.swap_chain_config().extent
	};

	const vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	const vk::GraphicsPipelineCreateInfo pipeline_info{
		.pNext = &geometry_rendering_info,
		.stageCount = static_cast<std::uint32_t>(shader_stages.size()),
		.pStages = shader_stages.data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = nullptr,
		.layout = m_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);
}

auto gse::renderer::geometry::render(const std::span<std::reference_wrapper<registry>> registries) -> void {
	auto& config = m_context.config();
	const auto command = config.frame_context().command_buffer;

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_config().albedo_image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite
	);

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_config().normal_image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageAspectFlagBits::eColor,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite
	);

	vulkan::uploader::transition_image_layout(
		command, config.swap_chain_config().depth_image,
		vk::ImageLayout::eDepthStencilAttachmentOptimal,
		vk::ImageAspectFlagBits::eDepth,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderSampledRead,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite
	);

	std::array color_attachments{
		vk::RenderingAttachmentInfo{
			.imageView = config.swap_chain_config().albedo_image.view,
			.imageLayout = config.swap_chain_config().albedo_image.current_layout,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f } } }
		},
		vk::RenderingAttachmentInfo{
			.imageView = config.swap_chain_config().normal_image.view,
			.imageLayout = config.swap_chain_config().normal_image.current_layout,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f } } }
		}
	};

	vk::RenderingAttachmentInfo depth_attachment{
		.imageView = config.swap_chain_config().depth_image.view,
		.imageLayout = config.swap_chain_config().depth_image.current_layout,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{.depth = 1.0f } }
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = { { 0, 0 }, config.swap_chain_config().extent },
		.layerCount = 1,
		.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
		.pColorAttachments = color_attachments.data(),
		.pDepthAttachment = &depth_attachment
	};

	vulkan::render(
		config, 
		rendering_info,
		[&] {
			if (!registry::any_components<render_component>(registries)) {
				return;
			}

			m_shader->set_uniform("CameraUBO.view", m_context.camera().view(), m_ubo_allocations.at("CameraUBO").allocation);
			m_shader->set_uniform("CameraUBO.proj", m_context.camera().projection(m_context.window().viewport()), m_ubo_allocations.at("CameraUBO").allocation);

			command.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

			const vk::DescriptorSet descriptor_set[] = { m_descriptor_set };

			command.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				m_pipeline_layout,
				0,
				vk::ArrayProxy<const vk::DescriptorSet>(1, descriptor_set),
				{}
			);

			std::unordered_map<std::string, std::span<const std::byte>> push_constants = {};

			auto calculate_center_of_mass = [](
				const std::vector<model_instance>& models
				) -> vec3<length> {
					vec3<length> sum;

					for (const auto& instance : models) {
						sum += instance.handle()->center_of_mass();
					}

					return models.size() > 0 ? sum / static_cast<float>(models.size()) : vec3<length>{ 0, 0, 0 };
				};

			for (const auto& registry : registries) {
				for (auto& component : registry.get().linked_objects<render_component>()) {
					const auto pos = registry.get().linked_object<physics::motion_component>(component.owner_id()).current_position;

					if (!std::ranges::any_of(
						component.models,
						[](const model_instance& model) {
							return model.handle().valid();
						}
					)) {
						continue;
					}

					if (!component.has_calculated_com) {
						component.center_of_mass = calculate_center_of_mass(component.models);
						component.has_calculated_com = true;
					}

					for (auto& model_handle : component.models) {
						if (!model_handle.handle().valid()) {
							continue;
						}

						model_handle.set_position(pos);

						for (const auto& entry : model_handle.render_queue_entries()) {
							push_constants["model"] = std::as_bytes(std::span{ &entry.model_matrix, 1 });
							push_constants["normal_matrix"] = std::as_bytes(std::span{ &entry.normal_matrix, 1 });

							std::println("model: {}, normal: {}", entry.model_matrix, entry.normal_matrix);

							m_shader->push(
								command,
								m_pipeline_layout,
								"push_constants",
								push_constants
							);

							if (const auto& mesh = entry.model->meshes()[entry.index]; mesh.material().valid()) {
								m_shader->push_descriptor(
									command,
									m_pipeline_layout,
									"diffuseSampler",
									mesh.material()->diffuse_texture->descriptor_info()
								);

								mesh.bind(command);
								mesh.draw(command);
							}
						}
					}
				}
			}
		}
	);
}