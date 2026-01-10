module;

#include <oneapi/tbb.h>

export module gse.graphics:text_renderer;

import std;
import vulkan_hpp;

import :font;
import :texture;
import :camera;
import :shader;
import :rendering_context;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::renderer {
	struct text_command {
		resource::handle<font> font;
		std::string text;
		unitless::vec2 position;
		float scale = 1.0f;
		unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		std::optional<rect_t<unitless::vec2>> clip_rect = std::nullopt;
	};

	class text final : public system {
	public:
		explicit text(
			context& context
		);

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;

	private:
		struct vertex {
			unitless::vec2 position;
			unitless::vec2 uv;
			unitless::vec4 color;
		};

		struct draw_batch {
			resource::handle<font> font;
			std::uint32_t vertex_offset;
			std::uint32_t index_offset;
			std::uint32_t index_count;
			std::optional<rect_t<unitless::vec2>> clip_rect;
		};

		struct frame_data {
			std::vector<vertex> vertices;
			std::vector<std::uint32_t> indices;
			std::vector<draw_batch> batches;
		};

		static constexpr std::size_t max_glyphs_per_frame = 16384;
		static constexpr std::size_t vertices_per_glyph = 4;
		static constexpr std::size_t indices_per_glyph = 6;
		static constexpr std::size_t max_vertices = max_glyphs_per_frame * vertices_per_glyph;
		static constexpr std::size_t max_indices = max_glyphs_per_frame * indices_per_glyph;
		static constexpr std::size_t frames_in_flight = 2;

		static auto to_vulkan_scissor(
			const rect_t<unitless::vec2>& rect,
			const unitless::vec2& window_size
		) -> vk::Rect2D;

		context& m_context;

		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;

		resource::handle<shader> m_shader;

		struct frame_resources {
			vulkan::persistent_allocator::buffer_resource vertex_buffer;
			vulkan::persistent_allocator::buffer_resource index_buffer;
		};

		std::array<frame_resources, frames_in_flight> m_frame_resources;
		std::uint32_t m_current_frame = 0;

		double_buffer<frame_data> m_frame_data;
	};
}

gse::renderer::text::text(context& context) : m_context(context) {}

auto gse::renderer::text::to_vulkan_scissor(const rect_t<unitless::vec2>& rect, const unitless::vec2& window_size) -> vk::Rect2D {
	const float left = std::max(0.0f, rect.left());
	const float top = std::max(0.0f, rect.top());
	const float right = std::min(window_size.x(), rect.right());
	const float bottom = std::min(window_size.y(), rect.bottom());

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

auto gse::renderer::text::initialize() -> void {
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
		.colorWriteMask = vk::ColorComponentFlagBits::eR |
		                  vk::ColorComponentFlagBits::eG |
		                  vk::ColorComponentFlagBits::eB |
		                  vk::ColorComponentFlagBits::eA
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

	m_shader = m_context.get<shader>("msdf");
	m_context.instantly_load(m_shader);

	const auto& msdf_dsl = m_shader->layouts();
	const auto msdf_pc_range = m_shader->push_constant_range("push_constants");

	const vk::PipelineLayoutCreateInfo msdf_pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(msdf_dsl.size()),
		.pSetLayouts = msdf_dsl.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &msdf_pc_range
	};

	m_pipeline_layout = config.device_config().device.createPipelineLayout(msdf_pipeline_layout_info);

	const auto vertex_input_info = m_shader->vertex_input_state();
	std::println("[text] Vertex bindings: {}", vertex_input_info.vertexBindingDescriptionCount);
	for (std::uint32_t i = 0; i < vertex_input_info.vertexBindingDescriptionCount; ++i) {
		const auto& b = vertex_input_info.pVertexBindingDescriptions[i];
		std::println("  binding[{}]: stride={}", i, b.stride);
	}

	std::println("[text] Vertex attributes: {}", vertex_input_info.vertexAttributeDescriptionCount);
	for (std::uint32_t i = 0; i < vertex_input_info.vertexAttributeDescriptionCount; ++i) {
		const auto& a = vertex_input_info.pVertexAttributeDescriptions[i];
		std::println("  attr[{}]: location={}, offset={}, format={}",
			i, a.location, a.offset, vk::to_string(a.format));
	}

	std::println("[text] Expected vertex size: {} bytes", sizeof(vertex));
	const vk::Format color_format = config.swap_chain_config().surface_format.format;

	const vk::PipelineRenderingCreateInfoKHR pipeline_rendering_info{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color_format
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state_info{
		.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
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
		.pDynamicState = &dynamic_state_info,
		.layout = *m_pipeline_layout
	};

	m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

	constexpr std::size_t vertex_buffer_size = max_vertices * sizeof(vertex);
	constexpr std::size_t index_buffer_size = max_indices * sizeof(std::uint32_t);

	for (auto& [vertex_buffer, index_buffer] : m_frame_resources) {
		vertex_buffer = vulkan::persistent_allocator::create_buffer(
			config.device_config(),
			{
				.size = vertex_buffer_size,
				.usage = vk::BufferUsageFlagBits::eVertexBuffer
			}
		);

		index_buffer = vulkan::persistent_allocator::create_buffer(
			config.device_config(),
			{
				.size = index_buffer_size,
				.usage = vk::BufferUsageFlagBits::eIndexBuffer
			}
		);
	}

	m_frame_data.write().vertices.reserve(max_vertices);
	m_frame_data.write().indices.reserve(max_indices);
	m_frame_data.write().batches.reserve(256);

	frame_sync::on_end([this] {
		m_frame_data.flip();
	});
}

auto gse::renderer::text::update() -> void {
	const auto commands = channel_of<text_command>();

	auto& [vertices, indices, batches] = m_frame_data.write();
	vertices.clear();
	indices.clear();
	batches.clear();

	if (commands.empty()) {
		return;
	}

	std::vector<text_command> sorted_commands;
	sorted_commands.reserve(commands.size());

	for (const auto& cmd : commands) {
		if (cmd.font.valid() && !cmd.text.empty()) {
			sorted_commands.push_back(cmd);
		}
	}

	std::ranges::stable_sort(sorted_commands, [](const text_command& a, const text_command& b) {
		return a.font.id().number() < b.font.id().number();
	});

	resource::handle<font> current_font;
	std::optional<rect_t<unitless::vec2>> current_clip;
	std::uint32_t batch_vertex_start = 0;
	std::uint32_t batch_index_start = 0;
	std::size_t flush_count = 0;

	auto flush_batch = [&](const char* reason) {
		if (indices.size() > batch_index_start) {
			std::println("[flush #{}] reason={}, vertices={}, indices={}",
				flush_count++, reason,
				vertices.size() - batch_vertex_start,
				indices.size() - batch_index_start);

			batches.push_back({
				.font = current_font,
				.vertex_offset = batch_vertex_start,
				.index_offset = batch_index_start,
				.index_count = static_cast<std::uint32_t>(indices.size() - batch_index_start),
				.clip_rect = current_clip
				});
		}
		batch_vertex_start = static_cast<std::uint32_t>(vertices.size());
		batch_index_start = static_cast<std::uint32_t>(indices.size());
	};

	for (std::size_t i = 0; i < sorted_commands.size(); ++i) {
		const auto& cmd = sorted_commands[i];

		const bool font_changed = cmd.font.id() != current_font.id();
		const bool clip_changed = cmd.clip_rect != current_clip;

		std::println("[cmd {}] text='{}', font_changed={}, clip_changed={}, has_clip={}",
			i, cmd.text, font_changed, clip_changed, cmd.clip_rect.has_value());

		if (font_changed || clip_changed) {
			flush_batch(font_changed ? "font" : "clip");
			current_font = cmd.font;
			current_clip = cmd.clip_rect;
		}

		const auto glyphs = cmd.font->text_layout(cmd.text, cmd.position, cmd.scale);
		std::println("  -> {} glyphs, vertices before={}", glyphs.size(), vertices.size());

		for (const auto& [screen_rect, uv_rect] : glyphs) {
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

		std::println("  -> vertices after={}", vertices.size());
	}

	flush_batch("end");

	std::println("[update done] total: {} vertices, {} indices, {} batches",
		vertices.size(), indices.size(), batches.size());
}

auto gse::renderer::text::render() -> void {
	const auto& [vertices, indices, batches] = m_frame_data.read();

	if (batches.empty()) {
		m_current_frame = (m_current_frame + 1) % frames_in_flight;
		return;
	}

	auto& [vertex_buffer, index_buffer] = m_frame_resources[m_current_frame];

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

	auto& config = m_context.config();
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
		.clearValue = vk::ClearValue{
			.color = vk::ClearColorValue{
				.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = {{ 0, 0 }, config.swap_chain_config().extent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr
	};

	vulkan::render(config, rendering_info, [&] {
		command.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
		command.bindVertexBuffers(0, { *vertex_buffer.buffer }, { vk::DeviceSize{ 0 } });
		command.bindIndexBuffer(*index_buffer.buffer, 0, vk::IndexType::eUint32);

		const vk::Rect2D default_scissor{ { 0, 0 }, { width, height } };
		command.setScissor(0, { default_scissor });
		vk::Rect2D current_scissor = default_scissor;

		resource::handle<font> bound_font;

		m_shader->push(
			command, *m_pipeline_layout, "push_constants",
			"projection", projection
		);

		for (const auto& [font, vertex_offset, index_offset, index_count, clip_rect] : batches) {
			if (!font.valid() || index_count == 0) {
				continue;
			}

			if (font.id() != bound_font.id()) {
				m_shader->push_descriptor(
					command, m_pipeline_layout,
					"spriteTexture",
					font->texture()->descriptor_info()
				);
				bound_font = font;
			}

			/*vk::Rect2D desired_scissor = default_scissor;
			if (clip_rect) {
				desired_scissor = to_vulkan_scissor(*clip_rect, window_size);
			}

			if (std::memcmp(&desired_scissor, &current_scissor, sizeof(vk::Rect2D)) != 0) {
				command.setScissor(0, { desired_scissor });
				current_scissor = desired_scissor;
			}*/

			command.drawIndexed(
				index_count,
				1,
				index_offset,
				static_cast<std::int32_t>(vertex_offset),
				0
			);
		}
	});

	m_current_frame = (m_current_frame + 1) % frames_in_flight;
}