export module gse.graphics:text_renderer;

import std;

import :debug;
import :font;
import :texture;
import :camera;
import :shader;
import :base_renderer;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::renderer {
	class text final : public base_renderer {
	public:
		struct command {
			resource::handle<font> font;
			std::string text;
			vec2<length> position;
			float scale = 1.0f;
			unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
			std::optional<length> max_width = std::nullopt;
		};

		explicit text(context& context, const std::span<std::reference_wrapper<registry>> registries) : base_renderer(context, registries) {}

		auto initialize() -> void override;
		auto render() -> void override;

		auto draw_text(const command& cmd) -> void;

	private:
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		vulkan::persistent_allocator::buffer_resource m_vertex_buffer;
		vulkan::persistent_allocator::buffer_resource m_index_buffer;

		resource::handle<shader> m_shader;

		std::vector<command> m_draw_commands;
	};
}

auto gse::renderer::text::initialize() -> void {
	auto& config = m_context.config();

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList
	};

	const vk::Viewport viewport{
		.x = 0.0f,
		.y = static_cast<float>(config.swap_chain_data.extent.height),
		.width = static_cast<float>(config.swap_chain_data.extent.width),
		.height = -static_cast<float>(config.swap_chain_data.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	const vk::Rect2D scissor{
		{ 0, 0 },
		{ config.swap_chain_data.extent.width, config.swap_chain_data.extent.height }
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
		.frontFace = vk::FrontFace::eClockwise,
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

	constexpr vk::PipelineDepthStencilStateCreateInfo transparent_depth_stencil_state{
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthCompareOp = vk::CompareOp::eLess,
		.stencilTestEnable = vk::False
	};

	m_shader = m_context.get<shader>("msdf_shader");
	m_context.instantly_load(m_shader);

	const auto& msdf_dsl = m_shader->layouts();
	const auto msdf_pc_range = m_shader->push_constant_range("pc", vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

	const vk::PipelineLayoutCreateInfo msdf_pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(msdf_dsl.size()),
		.pSetLayouts = msdf_dsl.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &msdf_pc_range
	};
	m_pipeline_layout = config.device_data.device.createPipelineLayout(msdf_pipeline_layout_info);

	const auto vertex_input_info = m_shader->vertex_input_state();
	const vk::Format color_format = config.swap_chain_data.surface_format.format;

	const vk::PipelineRenderingCreateInfoKHR pipeline_rendering_info{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color_format
	};

	vk::GraphicsPipelineCreateInfo pipeline_info{
		.pNext = &pipeline_rendering_info,
		.stageCount = 2,
		.pStages = m_shader->shader_stages().data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &transparent_depth_stencil_state,
		.pColorBlendState = &color_blending,
		.layout = *m_pipeline_layout
	};
	m_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info);

	struct vertex { raw2f pos; raw2f uv; };
	constexpr vertex vertices[4] = {
		{{0.0f,  0.0f}, {0.0f, 0.0f}},
		{{1.0f,  0.0f}, {1.0f, 0.0f}},
		{{1.0f, -1.0f}, {1.0f, 1.0f}},
		{{0.0f, -1.0f}, {0.0f, 1.0f}}
	};
	constexpr std::uint32_t indices[6] = { 0, 2, 1, 0, 3, 2 };

	m_vertex_buffer = vulkan::persistent_allocator::create_buffer(config.device_data, { .size = sizeof(vertices), .usage = vk::BufferUsageFlagBits::eVertexBuffer }, vertices);
	m_index_buffer = vulkan::persistent_allocator::create_buffer(config.device_data, { .size = sizeof(indices), .usage = vk::BufferUsageFlagBits::eIndexBuffer }, indices);
}

auto gse::renderer::text::render() -> void {
	if (m_draw_commands.empty()) {
		return;
	}

	const auto& config = m_context.config();
	const auto& command = config.frame_context.command_buffer;
	const auto [width, height] = config.swap_chain_data.extent;

	const auto projection = orthographic(
		meters(0.0f), meters(static_cast<float>(width)),
		meters(0.0f), meters(static_cast<float>(height)),
		meters(-1.0f), meters(1.0f)
	);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_data.image_views[config.frame_context.image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }}}
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = {{0, 0}, config.swap_chain_data.extent},
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

			for (const auto& [font, text, position, scale, color, max_width] : m_draw_commands) {
				if (!font || text.empty()) continue;

				m_shader->push(command, m_pipeline_layout, "msdf_texture", font->texture()->descriptor_info());
				const auto glyphs = font->text_layout(text, position.as<units::meters>(), scale);

				for (const auto& [glyph_position, size, uv] : glyphs) {
					if (max_width && glyph_position.x > *max_width) continue;

					std::unordered_map<std::string, std::span<const std::byte>> push_constants = {
						{ "projection", std::as_bytes(std::span(&projection, 1)) },
						{ "position",   std::as_bytes(std::span(&glyph_position, 1)) },
						{ "size",       std::as_bytes(std::span(&size, 1)) },
						{ "color",      std::as_bytes(std::span(&color, 1)) },
						{ "uv_rect",    std::as_bytes(std::span(&uv, 1)) }
					};

					m_shader->push(command, m_pipeline_layout, "pc", push_constants, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
					command.drawIndexed(6, 1, 0, 0, 0);
				}
			}

			debug::update_imgui();
			debug::render_imgui(command);
		}
	);

	m_draw_commands.clear();
}

auto gse::renderer::text::draw_text(const command& cmd) -> void {
	if (!cmd.font || cmd.text.empty()) {
		return;
	}

	m_draw_commands.push_back(cmd);
}