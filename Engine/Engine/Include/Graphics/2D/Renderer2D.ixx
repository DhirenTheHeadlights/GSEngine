module;

#include <cstddef>

export module gse.graphics.renderer2d;

import std;
import std.compat;
import vulkan_hpp;

import gse.core.config;
import gse.core.timer;
import gse.graphics.debug;
import gse.graphics.font;
import gse.graphics.texture;
import gse.physics.math;
import gse.graphics.shader_loader;
import gse.graphics.renderer3d;
import gse.graphics.camera;
import gse.platform;

export namespace gse::renderer2d {
    auto initialize(vulkan::config& config) -> void;
    auto render(const vulkan::config& config) -> void;
    auto shutdown(const vulkan::config::device_config& device_data) -> void;

    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const unitless::vec4& color) -> void;
    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const texture& texture) -> void;
    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const texture& texture, const unitless::vec4& uv) -> void;

    auto draw_text(const font& font, const std::string& text, const vec2<length>& position, float scale, const unitless::vec4& color) -> void;
}

vk::raii::Pipeline            g_pipeline = nullptr;
vk::raii::PipelineLayout      g_pipeline_layout = nullptr;

gse::vulkan::persistent_allocator::buffer_resource g_vertex_buffer;
gse::vulkan::persistent_allocator::buffer_resource g_index_buffer;

vk::raii::Pipeline             g_msdf_pipeline = nullptr;
vk::raii::PipelineLayout       g_msdf_pipeline_layout = nullptr;

gse::mat4 g_projection;

auto gse::renderer2d::initialize(vulkan::config& config) -> void {
    vk::PushConstantRange push_constant_range(
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, sizeof(unitless::vec2) * 2 + sizeof(unitless::vec4) * 2
    );

	auto layout = shader_loader::get_descriptor_layout(descriptor_layout::forward_2d);

    g_pipeline_layout = config.device_data.device.createPipelineLayout({ {}, 1, &layout, 1, &push_constant_range });

    const auto& element_shader = shader_loader::get_shader("ui_2d_shader");

    vk::PipelineInputAssemblyStateCreateInfo input_assembly({}, vk::PrimitiveTopology::eTriangleList, vk::False);

    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(window::get_window_size().x), static_cast<float>(window::get_window_size().y), 0.0f, 1.0f);
    vk::Rect2D scissor({ 0, 0 }, { static_cast<std::uint32_t>(window::get_window_size().x), static_cast<std::uint32_t>(window::get_window_size().y) });
    vk::PipelineViewportStateCreateInfo viewport_state({}, viewport, scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer({}, vk::False, vk::False, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, vk::False, 0.0f, 0.0f, 0.0f, 1.0f);
    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, vk::False, 1.0f, nullptr, vk::False, vk::False);

    vk::PipelineColorBlendAttachmentState color_blend_attachment(
        vk::True,
        vk::BlendFactor::eSrcAlpha,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    );

    vk::PipelineColorBlendStateCreateInfo color_blending({}, vk::False, {}, color_blend_attachment);

    struct vertex {
        vec::raw2f position;
        vec::raw2f texture_coordinate;
    };

    vk::VertexInputBindingDescription binding_description{
        0, sizeof(vertex), vk::VertexInputRate::eVertex
    };

    std::array attribute_descriptions{
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, position)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, texture_coordinate))
    };

    vk::PipelineVertexInputStateCreateInfo vertex_input_info(
        {},
        1, &binding_description,
        static_cast<std::uint32_t>(attribute_descriptions.size()), attribute_descriptions.data()
    );

    vk::GraphicsPipelineCreateInfo pipeline_info(
        {},
        2,
        element_shader.get_shader_stages().data(),
        &vertex_input_info,
        &input_assembly,
        nullptr,
        &viewport_state,
        &rasterizer,
        &multisampling,
        nullptr,
        &color_blending,
        nullptr,
        g_pipeline_layout,
        config.swap_chain_data.render_pass,
        2
    );

    g_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info);

    constexpr vertex vertices[4] = {
        {.position = {0.0f, 1.0f}, .texture_coordinate = {0.0f, 1.0f}}, // Top-left
        {.position = {1.0f, 1.0f}, .texture_coordinate = {1.0f, 1.0f}}, // Top-right
        {.position = {1.0f, 0.0f}, .texture_coordinate = {1.0f, 0.0f}}, // Bottom-right
        {.position = {0.0f, 0.0f}, .texture_coordinate = {0.0f, 0.0f}}  // Bottom-left
    };
    constexpr std::uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

    vk::DeviceSize vertex_buffer_size = sizeof(vertices);
    vk::DeviceSize index_buffer_size = sizeof(indices);

    vk::BufferCreateInfo vertex_buffer_info(
        {},
        vertex_buffer_size,
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::SharingMode::eExclusive
    );

    g_vertex_buffer =
        vulkan::persistent_allocator::create_buffer(
            config.device_data,
            vertex_buffer_info,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            vertices
        );

    vk::BufferCreateInfo index_buffer_info(
        {},
        index_buffer_size,
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::SharingMode::eExclusive
    );

    g_index_buffer =
        vulkan::persistent_allocator::create_buffer(
            config.device_data,
            index_buffer_info,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            indices
        );

    /// Generate MSDF pipeline

    const auto& msdf_shader = shader_loader::get_shader("msdf_shader");

    binding_description = {
        0, sizeof(vertex), vk::VertexInputRate::eVertex
    };

    attribute_descriptions = {
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, position)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(vertex, texture_coordinate))
    };

    vk::PushConstantRange msdf_push_constant_range(
        vk::ShaderStageFlagBits::eFragment,
        0, sizeof(unitless::vec4)
    );

    vk::PipelineVertexInputStateCreateInfo msdf_vertex_input_info({}, 1, &binding_description, static_cast<std::uint32_t>(attribute_descriptions.size()), attribute_descriptions.data());

    vk::PipelineLayoutCreateInfo pipeline_layout_info({}, 1, &layout, 1, &msdf_push_constant_range);
    g_msdf_pipeline_layout = config.device_data.device.createPipelineLayout(pipeline_layout_info);

    vk::PipelineColorBlendAttachmentState blend_state(
        vk::True,
        vk::BlendFactor::eSrcAlpha,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    );

    vk::PipelineColorBlendStateCreateInfo msdf_color_blending(
        {},
        vk::False,
        {},
        1,
        &blend_state
    );

    vk::GraphicsPipelineCreateInfo msdf_pipeline_info(
        {},
        2,
        msdf_shader.get_shader_stages().data(),
        &msdf_vertex_input_info,
        &input_assembly,
        nullptr,
        &viewport_state,
        &rasterizer,
        &multisampling,
        nullptr,
        &msdf_color_blending,
        nullptr,
        g_msdf_pipeline_layout,
        config.swap_chain_data.render_pass
    );
	msdf_pipeline_info.subpass = 2;

    g_msdf_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, msdf_pipeline_info);

    debug::initialize_imgui(config);
}

auto gse::renderer2d::render(const vulkan::config& config) -> void {
    debug::update_imgui();
    display_timers();
    debug::render_imgui(config.frame_context.command_buffer);
}

auto gse::renderer2d::shutdown(const vulkan::config::device_config& device_data) -> void {
}

auto render_quad(const gse::vec2<gse::length>& position, const gse::vec2<gse::length>& size, const gse::unitless::vec4* color, const gse::texture* texture, const gse::unitless::vec4& uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f }) -> void {
	/*const auto& command_buffer = gse::vulkan::config::command::buffers[0];
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

    const vk::Buffer vertex_buffers[] = { g_vertex_buffer.buffer };
    command_buffer.bindVertexBuffers(0, vertex_buffers, {});
    command_buffer.bindIndexBuffer(g_index_buffer.buffer, 0, vk::IndexType::eUint16);

    const struct push_constants {
        gse::unitless::vec2 position;
        gse::unitless::vec2 size;
        gse::unitless::vec4 color;
        gse::unitless::vec4 uv_rect;
    } pc{
        { position.x.as_default_unit(), position.y.as_default_unit() },
        { size.x.as_default_unit(), size.y.as_default_unit() },
        color ? *color : gse::unitless::vec4(1.0f),
        uv_rect
    };

    command_buffer.pushConstants(g_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(pc), &pc);
    command_buffer.drawIndexed(6, 1, 0, 0, 0);*/
}

auto gse::renderer2d::draw_quad(const vec2<length>& position, const vec2<length>& size, const unitless::vec4& color) -> void {
    render_quad(position, size, &color, nullptr);
}

auto gse::renderer2d::draw_quad(const vec2<length>& position, const vec2<length>& size, const texture& texture) -> void {
    render_quad(position, size, nullptr, &texture);
}

auto gse::renderer2d::draw_quad(const vec2<length>& position, const vec2<length>& size, const texture& texture, const unitless::vec4& uv) -> void {
    render_quad(position, size, nullptr, &texture, uv);
}

auto gse::renderer2d::draw_text(const font& font, const std::string& text, const vec2<length>& position, const float scale, const unitless::vec4& color) -> void {
 //   if (text.empty()) return;

	//const auto& command_buffer = vulkan::config::command::buffers[0];
 //   command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, g_msdf_pipeline);

 //   const vk::Buffer vertex_buffers[] = { g_vertex_buffer.buffer };
 //   command_buffer.bindVertexBuffers(0, vertex_buffers, {});
 //   command_buffer.bindIndexBuffer(g_index_buffer.buffer, 0, vk::IndexType::eUint16);

 //   const struct msdf_push_constants {
 //       unitless::vec4 color;
 //   } push_constants{ color };

 //   command_buffer.pushConstants(g_msdf_pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(msdf_push_constants), &push_constants);

 //   // TODO: Ideally allocate this once, not every draw call
 //   const vk::DescriptorSet text_descriptor_set =
 //       vulkan::config::device::device.allocateDescriptorSets(
 //           vk::DescriptorSetAllocateInfo(g_msdf_descriptor_pool, 1, &g_msdf_descriptor_set_layout))[0];

 //   command_buffer.bindDescriptorSets(
 //       vk::PipelineBindPoint::eGraphics,
 //       g_msdf_pipeline_layout,
 //       0, 1,
 //       &text_descriptor_set,
 //       0, nullptr
 //   );

 //   float start_x = position.x.as_default_unit();
 //   const float start_y = position.y.as_default_unit();

 //   for (const char c : text) {
 //       const auto& [u0, v0, u1, v1, width, height, x_offset, y_offset, x_advance] = font.get_character(c);

 //       if (width == 0.0f || height == 0.0f)
 //           continue;

 //       const float x_pos = start_x + x_offset * scale;
 //       const float y_pos = start_y - (height - y_offset) * scale;
 //       const float w = width * scale;
 //       const float h = height * scale;

 //       unitless::vec4 uv_rect(u0, v0, u1 - u0, v1 - v0);

 //       draw_quad(unitless::vec2(x_pos, y_pos), unitless::vec2(w, h), font.get_texture(), uv_rect);
 //       start_x += x_advance * scale;
 //   }
}
