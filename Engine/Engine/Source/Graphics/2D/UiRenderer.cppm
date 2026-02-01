export module gse.graphics:ui_renderer;

import std;

import :texture;
import :font;
import :camera;
import :shader;
import :rendering_context;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::renderer {
	struct sprite_command {
		rect_t<unitless::vec2> rect;
		unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		resource::handle<texture> texture;
		unitless::vec4 uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f };
		std::optional<rect_t<unitless::vec2>> clip_rect = std::nullopt;
		angle rotation;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
	};

	struct text_command {
		resource::handle<font> font;
		std::string text;
		unitless::vec2 position;
		float scale = 1.0f;
		unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		std::optional<rect_t<unitless::vec2>> clip_rect = std::nullopt;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
	};

	class ui final : public system {
	public:
		explicit ui(
			context& context
		);

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;

	private:
		enum class command_type : std::uint8_t {
			sprite,
			text
		};

		struct vertex {
			unitless::vec2 position;
			unitless::vec2 uv;
			unitless::vec4 color;
		};

		struct draw_batch {
			command_type type;
			std::uint32_t index_offset;
			std::uint32_t index_count;
			std::optional<rect_t<unitless::vec2>> clip_rect;
			resource::handle<texture> texture;
			resource::handle<font> font;
		};

		struct frame_data {
			std::vector<vertex> vertices;
			std::vector<std::uint32_t> indices;
			std::vector<draw_batch> batches;
		};

		struct unified_command {
			command_type type;
			render_layer layer;
			std::uint32_t z_order;
			std::optional<rect_t<unitless::vec2>> clip_rect;
			
			resource::handle<texture> texture;
			rect_t<unitless::vec2> rect;
			unitless::vec4 color;
			unitless::vec4 uv_rect;
			angle rotation;
			
			resource::handle<font> font;
			std::string text;
			unitless::vec2 position;
			float scale;
		};

		static constexpr std::size_t max_quads_per_frame = 32768;
		static constexpr std::size_t vertices_per_quad = 4;
		static constexpr std::size_t indices_per_quad = 6;
		static constexpr std::size_t max_vertices = max_quads_per_frame * vertices_per_quad;
		static constexpr std::size_t max_indices = max_quads_per_frame * indices_per_quad;
		static constexpr std::size_t frames_in_flight = 2;

		static auto to_vulkan_scissor(
			const rect_t<unitless::vec2>& rect,
			const unitless::vec2& window_size
		) -> vk::Rect2D;

		static auto add_sprite_quad(
			std::vector<vertex>& vertices,
			std::vector<std::uint32_t>& indices,
			const unified_command& cmd
		) -> void;

		static auto add_text_quads(
			std::vector<vertex>& vertices,
			std::vector<std::uint32_t>& indices,
			const unified_command& cmd
		) -> void;

		context& m_context;

		vk::raii::Pipeline m_sprite_pipeline = nullptr;
		vk::raii::PipelineLayout m_sprite_pipeline_layout = nullptr;
		resource::handle<shader> m_sprite_shader;

		vk::raii::Pipeline m_text_pipeline = nullptr;
		vk::raii::PipelineLayout m_text_pipeline_layout = nullptr;
		resource::handle<shader> m_text_shader;

		struct frame_resources {
			vulkan::buffer_resource vertex_buffer;
			vulkan::buffer_resource index_buffer;
		};

		std::array<frame_resources, frames_in_flight> m_frame_resources;
		triple_buffer<frame_data> m_frame_data;
	};
}

gse::renderer::ui::ui(context& context) : m_context(context) {}

auto gse::renderer::ui::to_vulkan_scissor(const rect_t<unitless::vec2>& rect, const unitless::vec2& window_size) -> vk::Rect2D {
	const float left = std::max(0.0f, rect.left());
	const float right = std::min(window_size.x(), rect.right());
	const float bottom = std::max(0.0f, rect.bottom());
	const float top = std::min(window_size.y(), rect.top());

	const float width = std::max(0.0f, right - left);
	const float height = std::max(0.0f, top - bottom);

	return {
		.offset = {
			static_cast<std::int32_t>(left),
			static_cast<std::int32_t>(window_size.y() - top)
		},
		.extent = {
			static_cast<std::uint32_t>(width),
			static_cast<std::uint32_t>(height)
		}
	};
}

auto gse::renderer::ui::add_sprite_quad(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, const unified_command& cmd) -> void {
	if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
		return;
	}

	const auto base_index = static_cast<std::uint32_t>(vertices.size());

	const unitless::vec2 top_left = cmd.rect.top_left();
	const unitless::vec2 size = cmd.rect.size();
	const unitless::vec2 center = {
		top_left.x() + size.x() * 0.5f,
		top_left.y() - size.y() * 0.5f
	};

	const unitless::vec2 half = { size.x() * 0.5f, size.y() * 0.5f };
	unitless::vec2 o0 = { -half.x(),  half.y() };
	unitless::vec2 o1 = { half.x(),  half.y() };
	unitless::vec2 o2 = { half.x(), -half.y() };
	unitless::vec2 o3 = { -half.x(), -half.y() };

	o0 = rotate(o0, cmd.rotation);
	o1 = rotate(o1, cmd.rotation);
	o2 = rotate(o2, cmd.rotation);
	o3 = rotate(o3, cmd.rotation);

	const unitless::vec2 p0 = center + o0;
	const unitless::vec2 p1 = center + o1;
	const unitless::vec2 p2 = center + o2;
	const unitless::vec2 p3 = center + o3;

	const float u0 = cmd.uv_rect.x();
	const float v0 = cmd.uv_rect.y();
	const float u1 = cmd.uv_rect.x() + cmd.uv_rect.z();
	const float v1 = cmd.uv_rect.y() + cmd.uv_rect.w();

	vertices.push_back({ p0, { u0, v0 }, cmd.color });
	vertices.push_back({ p1, { u1, v0 }, cmd.color });
	vertices.push_back({ p2, { u1, v1 }, cmd.color });
	vertices.push_back({ p3, { u0, v1 }, cmd.color });

	indices.push_back(base_index + 0);
	indices.push_back(base_index + 2);
	indices.push_back(base_index + 1);
	indices.push_back(base_index + 0);
	indices.push_back(base_index + 3);
	indices.push_back(base_index + 2);
}

auto gse::renderer::ui::add_text_quads(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, const unified_command& cmd) -> void {
	const auto layout = cmd.font->text_layout(cmd.text, cmd.position, cmd.scale);

	for (const auto& [screen_rect, uv_rect] : layout) {
		if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
			break;
		}

		const auto base_index = static_cast<std::uint32_t>(vertices.size());

		const unitless::vec2 top_left = screen_rect.top_left();
		const unitless::vec2 sz = screen_rect.size();

		const unitless::vec2 p0 = top_left;
		const unitless::vec2 p1 = { top_left.x() + sz.x(), top_left.y() };
		const unitless::vec2 p2 = { top_left.x() + sz.x(), top_left.y() - sz.y() };
		const unitless::vec2 p3 = { top_left.x(), top_left.y() - sz.y() };

		const float u0 = uv_rect.x();
		const float v0 = uv_rect.y();
		const float u1 = uv_rect.x() + uv_rect.z();
		const float v1 = uv_rect.y() + uv_rect.w();

		vertices.push_back({ p0, { u0, v1 }, cmd.color });
		vertices.push_back({ p1, { u1, v1 }, cmd.color });
		vertices.push_back({ p2, { u1, v0 }, cmd.color });
		vertices.push_back({ p3, { u0, v0 }, cmd.color });

		indices.push_back(base_index + 0);
		indices.push_back(base_index + 2);
		indices.push_back(base_index + 1);
		indices.push_back(base_index + 0);
		indices.push_back(base_index + 3);
		indices.push_back(base_index + 2);
	}
}

auto gse::renderer::ui::initialize() -> void {
	auto& config = m_context.config();

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList
	};

	// Use dynamic viewport and scissor for resize handling
	const vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
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
		.colorWriteMask = vk::ColorComponentFlagBits::eR |
		                  vk::ColorComponentFlagBits::eG |
		                  vk::ColorComponentFlagBits::eB |
		                  vk::ColorComponentFlagBits::eA
	};

	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment
	};

	constexpr vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthCompareOp = vk::CompareOp::eLess,
		.stencilTestEnable = vk::False
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state_info{
		.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	const vk::Format color_format = config.swap_chain_config().surface_format.format;

	const vk::PipelineRenderingCreateInfoKHR pipeline_rendering_info{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color_format
	};

	m_sprite_shader = m_context.get<shader>("Shaders/Standard2D/sprite");
	m_context.instantly_load(m_sprite_shader);

	{
		const auto& dsl = m_sprite_shader->layouts();
		const auto pc_range = m_sprite_shader->push_constant_range("push_constants");

		const vk::PipelineLayoutCreateInfo layout_info{
			.setLayoutCount = static_cast<std::uint32_t>(dsl.size()),
			.pSetLayouts = dsl.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pc_range
		};

		m_sprite_pipeline_layout = config.device_config().device.createPipelineLayout(layout_info);

		const auto vertex_input_info = m_sprite_shader->vertex_input_state();

		vk::GraphicsPipelineCreateInfo pipeline_info{
			.pNext = &pipeline_rendering_info,
			.stageCount = 2,
			.pStages = m_sprite_shader->shader_stages().data(),
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depth_stencil_state,
			.pColorBlendState = &color_blending,
			.pDynamicState = &dynamic_state_info,
			.layout = *m_sprite_pipeline_layout
		};

		m_sprite_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);
	}

	m_text_shader = m_context.get<shader>("Shaders/Standard2D/msdf");
	m_context.instantly_load(m_text_shader);

	{
		const auto& dsl = m_text_shader->layouts();
		const auto pc_range = m_text_shader->push_constant_range("push_constants");

		const vk::PipelineLayoutCreateInfo layout_info{
			.setLayoutCount = static_cast<std::uint32_t>(dsl.size()),
			.pSetLayouts = dsl.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pc_range
		};

		m_text_pipeline_layout = config.device_config().device.createPipelineLayout(layout_info);

		const auto vertex_input_info = m_text_shader->vertex_input_state();

		vk::GraphicsPipelineCreateInfo pipeline_info{
			.pNext = &pipeline_rendering_info,
			.stageCount = 2,
			.pStages = m_text_shader->shader_stages().data(),
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depth_stencil_state,
			.pColorBlendState = &color_blending,
			.pDynamicState = &dynamic_state_info,
			.layout = *m_text_pipeline_layout
		};

		m_text_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);
	}

	constexpr std::size_t vertex_buffer_size = max_vertices * sizeof(vertex);
	constexpr std::size_t index_buffer_size = max_indices * sizeof(std::uint32_t);

	for (auto& [vertex_buffer, index_buffer] : m_frame_resources) {
		vertex_buffer = config.allocator().create_buffer(
			{
				.size = vertex_buffer_size,
				.usage = vk::BufferUsageFlagBits::eVertexBuffer
			}
		);

		index_buffer = config.allocator().create_buffer(
			{
				.size = index_buffer_size,
				.usage = vk::BufferUsageFlagBits::eIndexBuffer
			}
		);
	}

	auto& [vertices, indices, batches] = m_frame_data.write();
	vertices.reserve(max_vertices);
	indices.reserve(max_indices);
	batches.reserve(512);
}

auto gse::renderer::ui::update() -> void {
	const auto sprite_commands = channel_of<sprite_command>();
	const auto text_commands = channel_of<text_command>();

	if (sprite_commands.empty() && text_commands.empty()) {
		return;
	}

	auto& [vertices, indices, batches] = m_frame_data.write();
	vertices.clear();
	indices.clear();
	batches.clear();

	std::vector<unified_command> unified;
	unified.reserve(sprite_commands.size() + text_commands.size());

	for (const auto& [rect, color, texture, uv_rect, clip_rect, rotation, layer, z_order] : sprite_commands) {
		if (!texture.valid()) {
			continue;
		}

		unified.push_back({
			.type = command_type::sprite,
			.layer = layer,
			.z_order = z_order,
			.clip_rect = clip_rect,
			.texture = texture,
			.rect = rect,
			.color = color,
			.uv_rect = uv_rect,
			.rotation = rotation,
			.font = {},
			.text = {},
			.position = {},
			.scale = 1.0f
		});
	}

	for (const auto& [font, text, position, scale, color, clip_rect, layer, z_order] : text_commands) {
		if (!font.valid() || text.empty()) {
			continue;
		}

		unified.push_back({
			.type = command_type::text,
			.layer = layer,
			.z_order = z_order,
			.clip_rect = clip_rect,
			.texture = {},
			.rect = {},
			.color = color,
			.uv_rect = {},
			.rotation = {},
			.font = font,
			.text = text,
			.position = position,
			.scale = scale
		});
	}

	std::ranges::stable_sort(unified, [](const unified_command& a, const unified_command& b) {
		if (a.layer != b.layer) {
			return static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer);
		}

		if (a.z_order != b.z_order) {
			return a.z_order < b.z_order;
		}

		if (a.type != b.type) {
			return a.type < b.type;
		}

		if (a.type == command_type::sprite) {
			return a.texture.id().number() < b.texture.id().number();
		} else {
			return a.font.id().number() < b.font.id().number();
		}
	});

	auto current_type = command_type::sprite;
	resource::handle<texture> current_texture;
	resource::handle<font> current_font;
	std::optional<rect_t<unitless::vec2>> current_clip;
	std::uint32_t batch_index_start = 0;

	auto flush_batch = [&] {
		if (indices.size() > batch_index_start) {
			batches.push_back({
				.type = current_type,
				.index_offset = batch_index_start,
				.index_count = static_cast<std::uint32_t>(indices.size() - batch_index_start),
				.clip_rect = current_clip,
				.texture = current_texture,
				.font = current_font
			});
		}
		batch_index_start = static_cast<std::uint32_t>(indices.size());
	};

	for (const auto& cmd : unified) {
		bool needs_flush = false;

		if (cmd.type != current_type) {
			needs_flush = true;
		} else if (cmd.clip_rect != current_clip) {
			needs_flush = true;
		} else if (cmd.type == command_type::sprite && cmd.texture.id() != current_texture.id()) {
			needs_flush = true;
		} else if (cmd.type == command_type::text && cmd.font.id() != current_font.id()) {
			needs_flush = true;
		}

		if (needs_flush) {
			flush_batch();
			current_type = cmd.type;
			current_clip = cmd.clip_rect;
			current_texture = cmd.texture;
			current_font = cmd.font;
		}

		if (cmd.type == command_type::sprite) {
			add_sprite_quad(vertices, indices, cmd);
		} else {
			add_text_quads(vertices, indices, cmd);
		}
	}

	flush_batch();

	m_frame_data.publish();
}

auto gse::renderer::ui::render() -> void {
	auto& config = m_context.config();

	if (!config.frame_in_progress()) {
		return;
	}

	const auto& [vertices, indices, batches] = m_frame_data.read();

	if (batches.empty()) {
		return;
	}
	const auto frame_index = config.current_frame();
	auto& [vertex_buffer, index_buffer] = m_frame_resources[frame_index];

	std::memcpy(
		vertex_buffer.allocation.mapped(),
		vertices.data(),
		vertices.size() * sizeof(vertex)
	);

	std::memcpy(
		index_buffer.allocation.mapped(),
		indices.data(),
		indices.size() * sizeof(std::uint32_t)
	);

	const auto& command = config.frame_context().command_buffer;
	const auto [width, height] = config.swap_chain_config().extent;
	const unitless::vec2 window_size = { static_cast<float>(width), static_cast<float>(height) };

	const auto projection = orthographic(
		meters(0.0f),
		meters(static_cast<float>(width)),
		meters(0.0f),
		meters(static_cast<float>(height)),
		meters(-1.0f),
		meters(1.0f)
	);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_config().image_views[config.frame_context().image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore,
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = {{ 0, 0 }, config.swap_chain_config().extent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
	};

	vulkan::render(config, rendering_info, [&] {
		command.bindVertexBuffers(0, { vertex_buffer.buffer }, { vk::DeviceSize{ 0 } });
		command.bindIndexBuffer(index_buffer.buffer, 0, vk::IndexType::eUint32);

		// Set dynamic viewport
		const vk::Viewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(width),
			.height = static_cast<float>(height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};
		command.setViewport(0, viewport);

		const vk::Rect2D default_scissor{ { 0, 0 }, { width, height } };
		command.setScissor(0, { default_scissor });

		auto bound_type = command_type::sprite;
		resource::handle<texture> bound_texture;
		resource::handle<font> bound_font;
		bool first_batch = true;

		for (const auto& [type, index_offset, index_count, clip_rect, texture, font] : batches) {
			if (index_count == 0) {
				continue;
			}

			if (first_batch || type != bound_type) {
				if (type == command_type::sprite) {
					command.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_sprite_pipeline);
					m_sprite_shader->push(
						command, *m_sprite_pipeline_layout, "push_constants",
						"projection", projection
					);
				} else {
					command.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_text_pipeline);
					m_text_shader->push(
						command, *m_text_pipeline_layout, "push_constants",
						"projection", projection
					);
				}
				bound_type = type;
				bound_texture = {};
				bound_font = {};
				first_batch = false;
			}

			if (type == command_type::sprite) {
				if (texture.valid() && texture.id() != bound_texture.id()) {
					m_sprite_shader->push_descriptor(
						command, m_sprite_pipeline_layout,
						"spriteTexture",
						texture->descriptor_info()
					);
					bound_texture = texture;
				}
			} else {
				if (font.valid() && font.id() != bound_font.id()) {
					m_text_shader->push_descriptor(
						command, m_text_pipeline_layout,
						"spriteTexture",
						font->texture()->descriptor_info()
					);
					bound_font = font;
				}
			}

			if (clip_rect) {
				command.setScissor(0, { to_vulkan_scissor(*clip_rect, window_size) });
			} else {
				command.setScissor(0, { default_scissor });
			}

			command.drawIndexed(
				index_count,
				1,
				index_offset,
				0,
				0
			);
		}
	});
}