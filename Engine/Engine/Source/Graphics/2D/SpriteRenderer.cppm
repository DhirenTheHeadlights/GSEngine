export module gse.graphics:sprite_renderer;

import std;
import vulkan_hpp;

import :texture;
import :camera;
import :shader;
import :base_renderer;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::renderer {
	class sprite final : public base_renderer {
	public:
		struct command {
			rect_t<unitless::vec2> rect;
			unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
			resource::handle<texture> texture;
			unitless::vec4 uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f };
			std::optional<rect_t<unitless::vec2>> clip_rect = std::nullopt;
			angle rotation;
		};

		explicit sprite(context& context) : base_renderer(context) {}

		auto initialize() -> void override;
		auto render(std::span<const std::reference_wrapper<registry>> registries) -> void override;

		auto queue(const command& cmd) -> void;
	private:
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		vulkan::persistent_allocator::buffer_resource m_vertex_buffer;
		vulkan::persistent_allocator::buffer_resource m_index_buffer;

		resource::handle<shader> m_shader;

		std::vector<command> m_draw_commands;
	};
}

auto gse::renderer::sprite::initialize() -> void {
	auto& config = m_context.config();

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList
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
		{ 0, 0 },
		{ config.swap_chain_config().extent.width, config.swap_chain_config().extent.height }
	};

	const vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eNone,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.lineWidth = 1.0f
	};

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1
	};

	constexpr vk::PipelineColorBlendAttachmentState color_blend_attachment{
		.blendEnable = vk::True,
		.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
		.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};

	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment
	};

	constexpr vk::PipelineDepthStencilStateCreateInfo opaque_depth_stencil_state{
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthCompareOp = vk::CompareOp::eLess,
		.stencilTestEnable = vk::False
	};

	m_shader = m_context.get<shader>("ui_2d");
	m_context.instantly_load(m_shader);

	const auto& element_shader = m_shader.resolve();
	const auto& quad_dsl = element_shader->layouts();

	const auto quad_pc_range = element_shader->push_constant_range("push_constants");

	const vk::PipelineLayoutCreateInfo quad_pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(quad_dsl.size()),
		.pSetLayouts = quad_dsl.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &quad_pc_range
	};
	m_pipeline_layout = config.device_config().device.createPipelineLayout(quad_pipeline_layout_info);

	const auto vertex_input_info = element_shader->vertex_input_state();
	const vk::Format color_format = config.swap_chain_config().surface_format.format;

	const vk::PipelineRenderingCreateInfoKHR pipeline_rendering_info{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color_format
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state_info{
		.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	vk::GraphicsPipelineCreateInfo pipeline_info{
		.pNext = &pipeline_rendering_info,
		.stageCount = 2,
		.pStages = element_shader->shader_stages().data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &opaque_depth_stencil_state,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state_info,
		.layout = *m_pipeline_layout
	};
	m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

	struct vertex {
		vec2<length> pos;
		unitless::vec2 uv;
	};

	constexpr vertex vertices[4] = {
		{{0.0f,  0.0f}, {0.0f, 0.0f}},
		{{1.0f,  0.0f}, {1.0f, 0.0f}},
		{{1.0f, -1.0f}, {1.0f, 1.0f}},
		{{0.0f, -1.0f}, {0.0f, 1.0f}}
	};

	constexpr std::uint32_t indices[6] = { 0, 2, 1, 0, 3, 2 };

	m_vertex_buffer = vulkan::persistent_allocator::create_buffer(
		config.device_config(),
		{ .size = sizeof(vertices), .usage = vk::BufferUsageFlagBits::eVertexBuffer },
		vertices
	);

	m_index_buffer = vulkan::persistent_allocator::create_buffer(
		config.device_config(),
		{ .size = sizeof(indices), .usage = vk::BufferUsageFlagBits::eIndexBuffer },
		indices
	);
}

auto gse::renderer::sprite::render(std::span<const std::reference_wrapper<registry>> registries) -> void {
	if (m_draw_commands.empty()) {
		return;
	}

	auto& config = m_context.config();
	const auto& command = config.frame_context().command_buffer;
	const auto shader = m_shader.resolve();
	const auto [width, height] = config.swap_chain_config().extent;
	const unitless::vec2 window_size = { static_cast<float>(width), static_cast<float>(height) };

	const auto projection = orthographic(
		meters(0.0f), meters(static_cast<float>(width)),
		meters(0.0f), meters(static_cast<float>(height)),
		meters(-1.0f), meters(1.0f)
	);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_config().image_views[config.frame_context().image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{
			.color = vk::ClearColorValue{
				.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = {{0, 0}, config.swap_chain_config().extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr
	};

	vulkan::render(
		config,
		rendering_info,
		[&] {
			command.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
			command.bindVertexBuffers(0, { *m_vertex_buffer.buffer }, { 0 });
			command.bindIndexBuffer(*m_index_buffer.buffer, 0, vk::IndexType::eUint32);

			const vk::Rect2D default_scissor{ {0, 0}, {width, height} };
			command.setScissor(0, { default_scissor });
			auto current_scissor = default_scissor;

			for (auto& [rect, color, texture, uv_rect, clip_rect, rotation] : m_draw_commands) {
				if (!texture.valid()) {
					continue;
				}

				auto desired_scissor = default_scissor;
				if (clip_rect.has_value()) {
					desired_scissor = to_vulkan_scissor(clip_rect.value(), window_size);
				}

				if (std::memcmp(&desired_scissor, &current_scissor, sizeof(vk::Rect2D)) != 0) {
					command.setScissor(0, { desired_scissor });
					current_scissor = desired_scissor;
				}

				auto position = rect.top_left();
				auto rect_size = rect.size();
				auto angle_rad = rotation.as<radians>();

				shader->push(
					command,
					m_pipeline_layout,
					"push_constants",
					"projection", projection,
					"position", position,
					"size", rect_size,
					"color", color,
					"uv_rect", uv_rect,
					"rotation", angle_rad
				);

				shader->push_descriptor(command, m_pipeline_layout, "spriteTexture", texture->descriptor_info());
				command.drawIndexed(6, 1, 0, 0, 0);
			}
		}
	);

	m_draw_commands.clear();
}

auto gse::renderer::sprite::queue(const command& cmd) -> void {
	m_draw_commands.push_back(cmd);
}