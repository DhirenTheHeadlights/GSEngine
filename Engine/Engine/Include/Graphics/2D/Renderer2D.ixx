module;

#include <imgui.h>

export module gse.graphics:renderer2d;

import std;

import :debug;
import :font;
import :texture;
import :shader_loader;
import :texture_loader;
import :renderer3d;
import :camera;
import :shader;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::renderer2d {
    struct context {
        vk::raii::Pipeline pipeline = nullptr;
        vk::raii::PipelineLayout pipeline_layout = nullptr;
        vk::raii::Pipeline msdf_pipeline = nullptr;
		vk::raii::PipelineLayout msdf_pipeline_layout = nullptr;
        vulkan::persistent_allocator::buffer_resource vertex_buffer;
		vulkan::persistent_allocator::buffer_resource index_buffer;
    };

    auto initialize(context& context, vulkan::config& config) -> void;
    auto render(context& context, const vulkan::config& config) -> void;

    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const unitless::vec4& color) -> void;
    auto draw_quad(const vec2<length>& position, const vec2<length>& size, texture& texture) -> void;
    auto draw_quad(const vec2<length>& position, const vec2<length>& size, texture& texture, const unitless::vec4& uv) -> void;

    auto draw_text(font& font, const std::string& text, const vec2<length>& position, float scale, const unitless::vec4& color) -> void;
}

struct quad_command {
    gse::vec2<gse::length> position;
    gse::vec2<gse::length> size;
    gse::unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    gse::texture* texture = nullptr;
    gse::unitless::vec4 uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f };
};

struct text_command {
    gse::font* font = nullptr;
    std::string text;
    gse::vec2<gse::length> position;
    float scale = 1.0f;
    gse::unitless::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

std::vector<quad_command> g_quad_draw_commands;
std::vector<text_command> g_text_draw_commands;

auto gse::renderer2d::initialize(context& context, vulkan::config& config) -> void {
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

    constexpr vk::PipelineDepthStencilStateCreateInfo opaque_depth_stencil_state{
        .depthTestEnable = vk::False,
        .depthWriteEnable = vk::False,
        .depthCompareOp = vk::CompareOp::eLess,
        .stencilTestEnable = vk::False
    };

    constexpr vk::PipelineDepthStencilStateCreateInfo transparent_depth_stencil_state{
        .depthTestEnable = vk::False,
        .depthWriteEnable = vk::False,
        .depthCompareOp = vk::CompareOp::eLess,
        .stencilTestEnable = vk::False
    };

    const auto& element_shader = shader_loader::shader("ui_2d_shader");
    const auto& quad_dsl = element_shader.layouts();

    const auto quad_pc_range = element_shader.push_constant_range("push_constants", vk::ShaderStageFlagBits::eVertex);

    const vk::PipelineLayoutCreateInfo quad_pipeline_layout_info{
        .setLayoutCount = static_cast<std::uint32_t>(quad_dsl.size()),
        .pSetLayouts = quad_dsl.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &quad_pc_range
    };
    context.pipeline_layout = config.device_data.device.createPipelineLayout(quad_pipeline_layout_info);

    const auto vertex_input_info = element_shader.vertex_input_state();

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .stageCount = 2,
        .pStages = element_shader.shader_stages().data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &opaque_depth_stencil_state,
        .pColorBlendState = &color_blending,
        .layout = context.pipeline_layout,
        .renderPass = *config.swap_chain_data.render_pass,
        .subpass = 2
    };
    context.pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info);

    const auto& msdf_shader = shader_loader::shader("msdf_shader");

    const auto& msdf_dsl = msdf_shader.layouts();
    const auto msdf_pc_range = msdf_shader.push_constant_range(
        "pc",
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
    );

    const vk::PipelineLayoutCreateInfo msdf_pipeline_layout_info{
        .setLayoutCount = static_cast<std::uint32_t>(msdf_dsl.size()),
        .pSetLayouts = msdf_dsl.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &msdf_pc_range
    };
    context.msdf_pipeline_layout = config.device_data.device.createPipelineLayout(msdf_pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo msdf_pipeline_info = pipeline_info;
    msdf_pipeline_info.pStages = msdf_shader.shader_stages().data();
    msdf_pipeline_info.layout = context.msdf_pipeline_layout;
    msdf_pipeline_info.pDepthStencilState = &transparent_depth_stencil_state;
    context.msdf_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, msdf_pipeline_info);

    struct vertex { raw2f pos; raw2f uv; };

    constexpr vertex vertices[4] = {
	    {{0.0f,  0.0f}, {0.0f, 0.0f}}, // Top-left corner (pos & uv)
	    {{1.0f,  0.0f}, {1.0f, 0.0f}}, // Top-right corner (pos & uv)
	    {{1.0f, -1.0f}, {1.0f, 1.0f}}, // Bottom-right corner (pos & uv)
	    {{0.0f, -1.0f}, {0.0f, 1.0f}}  // Bottom-left corner (pos & uv)
    };
    constexpr std::uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

    context.vertex_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_data, 
        {
        	.size = sizeof(vertices),
        	.usage = vk::BufferUsageFlagBits::eVertexBuffer
        }, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
        vertices
    );

    context.index_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_data, 
        {
        	.size = sizeof(indices),
        	.usage = vk::BufferUsageFlagBits::eIndexBuffer
        }, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
        indices
    );

    debug::initialize_imgui(config);
}

auto gse::renderer2d::render(context& context, const vulkan::config& config) -> void {
    if (g_quad_draw_commands.empty() && g_text_draw_commands.empty()) {
        debug::update_imgui();
        debug::render_imgui(config.frame_context.command_buffer);
        return;
    }

    const auto& command = config.frame_context.command_buffer;
    const auto [width, height] = config.swap_chain_data.extent;

    const auto projection = orthographic(
        meters(0.0f),
        meters(width),
        meters(0.0f),
        meters(height),
        meters(-1.0f),
        meters(1.0f)
    );

    command.bindVertexBuffers(
        0,
        { context.vertex_buffer.buffer },
        { 0 }
    );

    command.bindIndexBuffer(
        context.index_buffer.buffer,
        0,
        vk::IndexType::eUint32
    );

    if (!g_quad_draw_commands.empty()) {
        command.bindPipeline(vk::PipelineBindPoint::eGraphics, context.pipeline);

        const auto& shader = shader_loader::shader("ui_2d_shader");

        for (auto& [position, size, color, texture, uv_rect] : g_quad_draw_commands) {
            std::unordered_map<std::string, std::span<const std::byte>> push_constants = {
                { "projection", std::as_bytes(std::span(&projection, 1)) },
                { "position", std::as_bytes(std::span(&position, 1)) },
                { "size", std::as_bytes(std::span(&size, 1)) },
                { "color", std::as_bytes(std::span(&color, 1)) },
                { "uv_rect", std::as_bytes(std::span(&uv_rect, 1)) }
            };

            shader.push(
                command,
                context.pipeline_layout,
                "push_constants",
                push_constants,
                vk::ShaderStageFlagBits::eVertex
            );

            shader.push(
                command,
                context.pipeline_layout,
                "ui_texture",
                texture->get_descriptor_info()
            );

            command.drawIndexed(6, 1, 0, 0, 0);
        }
    }

    if (!g_text_draw_commands.empty()) {
        command.bindPipeline(vk::PipelineBindPoint::eGraphics, context.msdf_pipeline);
        const auto& shader = shader_loader::shader("msdf_shader");

        for (const auto& [font, text, position, scale, color] : g_text_draw_commands) {
            if (!font || text.empty()) continue;

            shader.push(
                command,
                context.msdf_pipeline_layout,
                "msdf_texture",
                font->texture().get_descriptor_info()
            );

            const auto text_height = font->line_height(scale);
            const vec2<length> baseline_start_pos = {
	            position.x,
	            position.y - meters(text_height)
            };

            const auto glyphs = font->text_layout(text, baseline_start_pos.as<units::meters>(), scale);

            for (const auto& [position, size, uv] : glyphs) {
                const vec2<length> adjusted_pos = { position.x, position.y + size.y };

                std::unordered_map<std::string, std::span<const std::byte>> push_constants = {
                    { "projection", std::as_bytes(std::span(&projection, 1)) },
                    { "position",   std::as_bytes(std::span(&adjusted_pos, 1)) },
                    { "size",       std::as_bytes(std::span(&size, 1)) },
                    { "color",      std::as_bytes(std::span(&color, 1)) },
                    { "uv_rect",    std::as_bytes(std::span(&uv, 1)) }
                };

                shader.push(
                    command,
                    context.msdf_pipeline_layout,
                    "pc",
                    push_constants,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                );

                command.drawIndexed(6, 1, 0, 0, 0);
            }
        }
    }

    g_quad_draw_commands.clear();
    g_text_draw_commands.clear();

    debug::add_imgui_callback(
        [] {
            auto& timers = get_timers();
            ImGui::Begin("Timers");
            for (auto it = timers.begin(); it != timers.end();) {
                const auto& timer = it->second;
                debug::print_value(timer.name(), timer.elapsed().as<units::milliseconds>(), units::milliseconds::unit_name);
                if (timer.completed()) {
                    it = timers.erase(it); // Remove completed timers
                }
                else {
                    ++it;
                }
            }
            ImGui::End();
        }
    );

    debug::update_imgui();
    debug::render_imgui(config.frame_context.command_buffer);
}

auto render_quad(const gse::vec2<gse::length>& position, const gse::vec2<gse::length>& size, const gse::unitless::vec4& color, gse::texture& texture, const gse::unitless::vec4& uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f }) -> void {
    g_quad_draw_commands.emplace_back(
        quad_command{
            .position = position,
            .size = size,
            .color = color,
            .texture = &texture,
            .uv_rect = {0.0f, 0.0f, 1.0f, 1.0f}
        }
    );
}

auto gse::renderer2d::draw_quad(const vec2<length>& position, const vec2<length>& size, const unitless::vec4& color) -> void {
    render_quad(position, size, color, texture_loader::blank());
}

auto gse::renderer2d::draw_quad(const vec2<length>& position, const vec2<length>& size, texture& texture = texture_loader::blank()) -> void {
    render_quad(position, size, {}, texture);
}

auto gse::renderer2d::draw_quad(const vec2<length>& position, const vec2<length>& size, texture& texture, const unitless::vec4& uv) -> void {
    render_quad(position, size, {}, texture, uv);
}

auto gse::renderer2d::draw_text(font& font, const std::string& text, const vec2<length>& position, const float scale, const unitless::vec4& color) -> void {
    if (text.empty()) return;

    g_text_draw_commands.emplace_back(
        text_command{
            .font = &font,
            .text = text,
            .position = position,
            .scale = scale,
            .color = color
        }
    );
}