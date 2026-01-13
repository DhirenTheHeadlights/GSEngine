export module gse.graphics:sprite_renderer;

import std;
import vulkan_hpp;

import :texture;
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

	class sprite final : public system {
	public:
		explicit sprite(
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
			resource::handle<texture> texture;
			std::uint32_t index_offset;
			std::uint32_t index_count;
			std::optional<rect_t<unitless::vec2>> clip_rect;
		};

		struct frame_data {
			std::vector<vertex> vertices;
			std::vector<std::uint32_t> indices;
			std::vector<draw_batch> batches;
		};

		static constexpr std::size_t max_sprites_per_frame = 16384;
		static constexpr std::size_t vertices_per_sprite = 4;
		static constexpr std::size_t indices_per_sprite = 6;
		static constexpr std::size_t max_vertices = max_sprites_per_frame * vertices_per_sprite;
		static constexpr std::size_t max_indices = max_sprites_per_frame * indices_per_sprite;
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

gse::renderer::sprite::sprite(context& context) : m_context(context) {}

auto gse::renderer::sprite::to_vulkan_scissor(const rect_t<unitless::vec2>& rect, const unitless::vec2& window_size) -> vk::Rect2D {
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

	m_shader = m_context.get<shader>("sprite");
	m_context.instantly_load(m_shader);

	const auto& dsl = m_shader->layouts();
	const auto pc_range = m_shader->push_constant_range("push_constants");

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(dsl.size()),
		.pSetLayouts = dsl.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pc_range
	};

	m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	const auto vertex_input_info = m_shader->vertex_input_state();
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
		.pDepthStencilState = &depth_stencil_state,
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

auto gse::renderer::sprite::update() -> void {
	const auto commands = channel_of<sprite_command>();

	auto& [vertices, indices, batches] = m_frame_data.write();
	vertices.clear();
	indices.clear();
	batches.clear();

	if (commands.empty()) {
		return;
	}

	std::vector<sprite_command> sorted_commands;
	sorted_commands.reserve(commands.size());

	for (const auto& cmd : commands) {
		if (cmd.texture.valid()) {
			sorted_commands.push_back(cmd);
		}
	}

	std::ranges::stable_sort(sorted_commands, [](const sprite_command& a, const sprite_command& b) {
		if (a.layer != b.layer) {
			return static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer);
		}
		
		if (a.z_order != b.z_order) {
			return a.z_order < b.z_order;
		}
		
		if (a.texture.id() != b.texture.id()) {
			return a.texture.id().number() < b.texture.id().number();
		}

		const bool a_has_clip = a.clip_rect.has_value();
		const bool b_has_clip = b.clip_rect.has_value();

		if (a_has_clip != b_has_clip) {
			return !a_has_clip;
		}

		if (a_has_clip && b_has_clip) {
			const auto& ar = *a.clip_rect;
			const auto& br = *b.clip_rect;

			if (ar.left() != br.left()) return ar.left() < br.left();
			if (ar.top() != br.top()) return ar.top() < br.top();
			if (ar.right() != br.right()) return ar.right() < br.right();
			return ar.bottom() < br.bottom();
		}

		return false;
	});

	resource::handle<texture> current_texture;
	std::optional<rect_t<unitless::vec2>> current_clip;
	std::uint32_t batch_index_start = 0;

	auto flush_batch = [&] {
		if (indices.size() > batch_index_start) {
			batches.push_back({
				.texture = current_texture,
				.index_offset = batch_index_start,
				.index_count = static_cast<std::uint32_t>(indices.size() - batch_index_start),
				.clip_rect = current_clip
			});
		}
		batch_index_start = static_cast<std::uint32_t>(indices.size());
	};

	for (const auto& [rect, color, texture, uv_rect, clip_rect, rotation, layer, z_order] : sorted_commands) {
		const bool texture_changed = texture.id() != current_texture.id();

		if (const bool clip_changed = clip_rect != current_clip; texture_changed || clip_changed) {
			flush_batch();
			current_texture = texture;
			current_clip = clip_rect;
		}

		if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
			break;
		}

		const auto base_index = static_cast<std::uint32_t>(vertices.size());

		const unitless::vec2 top_left = rect.top_left();
		const unitless::vec2 size = rect.size();
		const unitless::vec2 center = {
			top_left.x() + size.x() * 0.5f,
			top_left.y() - size.y() * 0.5f
		};

		const unitless::vec2 half = { size.x() * 0.5f, size.y() * 0.5f };
		unitless::vec2 o0 = { -half.x(),  half.y() };
		unitless::vec2 o1 = { half.x(),  half.y() };
		unitless::vec2 o2 = { half.x(), -half.y() };
		unitless::vec2 o3 = { -half.x(), -half.y() };

		o0 = rotate(o0, rotation);
		o1 = rotate(o1, rotation);
		o2 = rotate(o2, rotation);
		o3 = rotate(o3, rotation);

		const unitless::vec2 p0 = center + o0;
		const unitless::vec2 p1 = center + o1;
		const unitless::vec2 p2 = center + o2;
		const unitless::vec2 p3 = center + o3;

		const float u0 = uv_rect.x();
		const float v0 = uv_rect.y();
		const float u1 = uv_rect.x() + uv_rect.z();
		const float v1 = uv_rect.y() + uv_rect.w();

		vertices.push_back({ p0, { u0, v0 }, color });
		vertices.push_back({ p1, { u1, v0 }, color });
		vertices.push_back({ p2, { u1, v1 }, color });
		vertices.push_back({ p3, { u0, v1 }, color });

		indices.push_back(base_index + 0);
		indices.push_back(base_index + 2);
		indices.push_back(base_index + 1);
		indices.push_back(base_index + 0);
		indices.push_back(base_index + 3);
		indices.push_back(base_index + 2);
	}

	flush_batch();
}

auto gse::renderer::sprite::render() -> void {
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
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = {{ 0, 0 }, config.swap_chain_config().extent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
	};

	vulkan::render(config, rendering_info, [&] {
		command.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
		command.bindVertexBuffers(0, { *vertex_buffer.buffer }, { vk::DeviceSize{ 0 } });
		command.bindIndexBuffer(*index_buffer.buffer, 0, vk::IndexType::eUint32);

		const vk::Rect2D default_scissor{ { 0, 0 }, { width, height } };
		command.setScissor(0, { default_scissor });

		resource::handle<texture> bound_texture;

		m_shader->push(
			command, *m_pipeline_layout, "push_constants",
			"projection", projection
		);

		for (const auto& [texture, index_offset, index_count, clip_rect] : batches) {
			if (!texture.valid() || index_count == 0) {
				continue;
			}

			if (texture.id() != bound_texture.id()) {
				m_shader->push_descriptor(
					command, m_pipeline_layout,
					"spriteTexture",
					texture->descriptor_info()
				);
				bound_texture = texture;
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

	m_current_frame = (m_current_frame + 1) % frames_in_flight;
}