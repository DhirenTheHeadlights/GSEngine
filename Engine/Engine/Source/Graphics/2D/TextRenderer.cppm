module;

#include <oneapi/tbb.h>

export module gse.graphics:text_renderer;

import std;
import vulkan_hpp;

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
			unitless::vec2 position;
			float scale = 1.0f;
			unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
			std::optional<rect_t<unitless::vec2>> clip_rect = std::nullopt;
		};

		explicit text(
			context& context
		);

		auto initialize(
		) -> void override;

		auto update(
			std::span<const std::reference_wrapper<registry>> registries
		) -> void override;

		auto render(
			std::span<const std::reference_wrapper<registry>> registries
		) -> void override;

		auto draw_text(
			const command& cmd
		) -> void;
	private:
		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		vulkan::persistent_allocator::buffer_resource m_vertex_buffer;
		vulkan::persistent_allocator::buffer_resource m_index_buffer;

		resource::handle<shader> m_shader;
		std::vector<std::byte> m_pc_buffer;
		struct shader::uniform_block m_pc_layout; 

		std::mutex m_command_mutex;
		std::vector<command> m_pending_commands;

		struct draw_item {
            resource::handle<font> font;
            unitless::vec4 color;
            rect_t<unitless::vec2> screen_rect;
            unitless::vec4 uv_rect;
            std::optional<rect_t<unitless::vec2>> clip_rect;
        };

		double_buffer<std::vector<draw_item>> m_command_queue;
	};
}

gse::renderer::text::text(context& context): base_renderer(context) {}

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

	struct vertex {
		vec2<length> pos;
		unitless::vec2 uv;
	};

    // NOTE: Y goes 0 to -1. This means the sprite "hangs" down from the anchor position.
	constexpr vertex vertices[4] = {
		{{0.0f,  0.0f}, {0.0f, 0.0f}},
		{{1.0f,  0.0f}, {1.0f, 0.0f}},
		{{1.0f, -1.0f}, {1.0f, 1.0f}},
		{{0.0f, -1.0f}, {0.0f, 1.0f}}
	};

	constexpr std::uint32_t indices[6] = { 0, 2, 1, 0, 3, 2 };

	m_vertex_buffer = vulkan::persistent_allocator::create_buffer(
		config.device_config(), 
		{ 
			.size = sizeof(vertices),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer
		},
		vertices
	);

	m_index_buffer = vulkan::persistent_allocator::create_buffer(
		config.device_config(), 
		{
			.size = sizeof(indices),
			.usage = vk::BufferUsageFlagBits::eIndexBuffer
		}, 
		indices
	);

	frame_sync::on_end([this] {
		m_command_queue.flip();
	});
}

auto gse::renderer::text::update(std::span<const std::reference_wrapper<registry>>) -> void {
    std::vector<command> local;
    {
        std::scoped_lock lk(m_command_mutex);
        local.swap(m_pending_commands);
    }

    auto& out = m_command_queue.write();
    out.clear();
    if (local.empty()) {
        return;
    }

    struct group {
        resource::handle<font> f;
        std::vector<const command*> commands;
    };

    struct font_handle_hash {
        auto operator()(const resource::handle<font>& h) const noexcept -> size_t {
            return std::hash<id>{}(h.id());
        }
    };

    struct font_handle_eq {
        auto operator()(const resource::handle<font>& a, const resource::handle<font>& b) const noexcept -> bool {
            return a.id() == b.id();
        }
    };

    std::unordered_map<resource::handle<font>, std::size_t, font_handle_hash, font_handle_eq> idx;
    std::vector<group> groups;
    groups.reserve(local.size());

    for (const auto& c : local) {
        if (!c.font || c.text.empty()) continue;
        auto it = idx.find(c.font);
        if (it == idx.end()) {
            idx.emplace(c.font, groups.size());
            groups.push_back(group{ c.font, {} });
            groups.back().commands.reserve(16);
            it = std::prev(idx.end());
        }
        groups[it->second].commands.push_back(&c);
    }

    if (groups.empty()) {
        return;
    }

    tbb::enumerable_thread_specific<std::vector<draw_item>> tls_bins;

    tbb::parallel_for(std::size_t{0}, groups.size(), [&](const std::size_t gi){
        const auto& [f, commands] = groups[gi];
        auto& items = tls_bins.local();

        for (const command* pc : commands) {
            const auto& c = *pc;

            for (const auto glyphs = f->text_layout(c.text, c.position, c.scale); const auto& [screen_rect, uv_rect] : glyphs) {
                items.push_back(draw_item{
                    .font = f,
                    .color = c.color,
                    .screen_rect = screen_rect,
                    .uv_rect = uv_rect,
                    .clip_rect = c.clip_rect
                });
            }
        }
    });

    std::size_t total = out.size();
    for (auto it = tls_bins.begin(); it != tls_bins.end(); ++it) total += it->size();
    out.reserve(total);

    for (auto it = tls_bins.begin(); it != tls_bins.end(); ++it) {
        auto& v = *it;
        out.insert(out.end(), v.begin(), v.end());
        v.clear();
    }

    std::ranges::stable_sort(out, [](const draw_item& a, const draw_item& b) {
        return a.font.resolve() < b.font.resolve();
    });
}

auto gse::renderer::text::render(std::span<const std::reference_wrapper<registry>> registries) -> void {
	auto& config = m_context.config();
	const auto& command = config.frame_context().command_buffer;
	auto [width, height] = config.swap_chain_config().extent;
	const unitless::vec2 window_size = { static_cast<float>(width), static_cast<float>(height) };

	const auto projection = orthographic(
		meters(0.0f), 
		meters(static_cast<float>(width)),
		meters(0.0f), // Bottom
		meters(static_cast<float>(height)),
		meters(-1.0f), 
		meters(1.0f)
	);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_config().image_views[config.frame_context().image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{.color = vk::ClearColorValue{.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }}}
	};

	const vk::RenderingInfo rendering_info{
		.renderArea = {{0, 0}, config.swap_chain_config().extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr
	};

	vulkan::render(config, rendering_info, [&] {
		const auto& draw_items = m_command_queue.read();
        if (draw_items.empty()) {
            return;
        }

        command.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
        command.bindVertexBuffers(0, { *m_vertex_buffer.buffer }, { 0 });
        command.bindIndexBuffer(*m_index_buffer.buffer, 0, vk::IndexType::eUint32);

        const vk::Rect2D default_scissor{{0,0}, {width, height}};
        command.setScissor(0, { default_scissor });
        auto current_scissor = default_scissor;

        resource::handle<font> bound_font;

        for (const auto& [font, color, screen_rect, uv_rect, clip_rect] : draw_items) {
            if (!font.valid()) {
                continue;
            }

            if (font != bound_font) {
                m_shader->push_descriptor(
                    command, m_pipeline_layout,
                    "spriteTexture",
                    font->texture()->descriptor_info()
                );
                bound_font = font;
            }

            vk::Rect2D desired_scissor = default_scissor;
            if (clip_rect) {
                desired_scissor = to_vulkan_scissor(*clip_rect, window_size);
            }
            if (std::memcmp(&desired_scissor, &current_scissor, sizeof(vk::Rect2D)) != 0) {
                command.setScissor(0, { desired_scissor });
                current_scissor = desired_scissor;
            }

            const auto rect_position = screen_rect.top_left();
            const auto size = screen_rect.size();

            m_shader->push(
                command, *m_pipeline_layout, "push_constants",
                "projection", projection,
                "position", rect_position,
                "size", size,
                "color", color,
                "uv_rect", uv_rect
            );

            command.drawIndexed(6, 1, 0, 0, 0);
        }

        if (std::memcmp(&default_scissor, &current_scissor, sizeof(vk::Rect2D)) != 0) {
            command.setScissor(0, { default_scissor });
        }
    });
}

auto gse::renderer::text::draw_text(const command& cmd) -> void {
	if (!cmd.font || cmd.text.empty()) {
		return;
	}
    std::scoped_lock lk(m_command_mutex);
    m_pending_commands.push_back(cmd);
}