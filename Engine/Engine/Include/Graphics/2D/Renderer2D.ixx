module;

#include <cstddef>

export module gse.graphics.renderer2d;

import std;

import gse.core.config;
import gse.core.timer;
import gse.graphics.debug;
import gse.graphics.font;
import gse.graphics.texture;
import gse.physics.math;
import gse.graphics.shader_loader;
import gse.graphics.renderer3d;
import gse.graphics.camera;
import gse.graphics.shader;
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

vk::raii::Pipeline            g_msdf_pipeline = nullptr;
vk::raii::PipelineLayout      g_msdf_pipeline_layout = nullptr;

gse::mat4 g_projection;

auto gse::renderer2d::initialize(vulkan::config& config) -> void {
    const vk::PushConstantRange element_push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(unitless::vec2) * 2 + sizeof(unitless::vec4) * 2
    };

    const auto& element_shader = shader_loader::get_shader("ui_2d_shader");
    const auto& element_layout = element_shader.layout(shader::set::binding_type::persistent);

    const vk::PipelineLayoutCreateInfo element_pipeline_layout_info{
        .flags = {},
        .setLayoutCount = 1,
        .pSetLayouts = &element_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &element_push_constant_range
    };
    g_pipeline_layout = config.device_data.device.createPipelineLayout(element_pipeline_layout_info);

    const vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .flags = {},
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False
    };

    const vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(window::get_window_size().x),
        .height = static_cast<float>(window::get_window_size().y),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    const vk::Rect2D scissor{
        .offset = { 0, 0 },
        .extent = {
            .width = static_cast<std::uint32_t>(window::get_window_size().x),
            .height = static_cast<std::uint32_t>(window::get_window_size().y)
        }
    };
    const vk::PipelineViewportStateCreateInfo viewport_state{
        .flags = {},
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    const vk::PipelineRasterizationStateCreateInfo rasterizer{
        .flags = {},
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };

    const vk::PipelineMultisampleStateCreateInfo multisampling{
        .flags = {},
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False
    };

    const vk::PipelineColorBlendAttachmentState color_blend_attachment{
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
        .flags = {},
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    struct vertex {
        vec::raw2f position;
        vec::raw2f texture_coordinate;
    };

    const vk::VertexInputBindingDescription binding_description{
        .binding = 0,
        .stride = sizeof(vertex),
        .inputRate = vk::VertexInputRate::eVertex
    };

    const std::array attribute_descriptions{
        vk::VertexInputAttributeDescription{
            .location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(vertex, position)
        },
        vk::VertexInputAttributeDescription{
            .location = 1, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(vertex, texture_coordinate)
        }
    };

    const vk::PipelineVertexInputStateCreateInfo vertex_input_info{
        .flags = {},
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attribute_descriptions.size()),
        .pVertexAttributeDescriptions = attribute_descriptions.data()
    };

    const vk::GraphicsPipelineCreateInfo pipeline_info{
        .flags = {},
        .stageCount = 2,
        .pStages = element_shader.shader_stages().data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &color_blending,
        .pDynamicState = nullptr,
        .layout = *g_pipeline_layout,
        .renderPass = *config.swap_chain_data.render_pass,
        .subpass = 2
    };
    g_pipeline = config.device_data.device.createGraphicsPipeline(nullptr, pipeline_info);

    constexpr vertex vertices[4] = {
        {.position = {0.0f, 1.0f}, .texture_coordinate = {0.0f, 1.0f}},
        {.position = {1.0f, 1.0f}, .texture_coordinate = {1.0f, 1.0f}},
        {.position = {1.0f, 0.0f}, .texture_coordinate = {1.0f, 0.0f}},
        {.position = {0.0f, 0.0f}, .texture_coordinate = {0.0f, 0.0f}}
    };
    constexpr std::uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

    const vk::BufferCreateInfo vertex_buffer_info{
        .flags = {},
        .size = sizeof(vertices),
        .usage = vk::BufferUsageFlagBits::eVertexBuffer,
        .sharingMode = vk::SharingMode::eExclusive
    };
    g_vertex_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_data, vertex_buffer_info,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        vertices
    );

    const vk::BufferCreateInfo index_buffer_info{
        .flags = {},
        .size = sizeof(indices),
        .usage = vk::BufferUsageFlagBits::eIndexBuffer,
        .sharingMode = vk::SharingMode::eExclusive
    };
    g_index_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_data, index_buffer_info,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        indices
    );

    const auto& msdf_shader = shader_loader::get_shader("msdf_shader");

    const vk::PushConstantRange msdf_push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(unitless::vec4)
    };

    const vk::PipelineLayoutCreateInfo msdf_pipeline_layout_info{
        .flags = {},
        .setLayoutCount = 1,
        .pSetLayouts = &element_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &msdf_push_constant_range
    };
    g_msdf_pipeline_layout = config.device_data.device.createPipelineLayout(msdf_pipeline_layout_info);

    const vk::PipelineColorBlendStateCreateInfo msdf_color_blending{
        .flags = {},
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    vk::GraphicsPipelineCreateInfo msdf_pipeline_info{
        .flags = {},
        .stageCount = 2,
        .pStages = msdf_shader.shader_stages().data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &msdf_color_blending,
        .pDynamicState = nullptr,
        .layout = *g_msdf_pipeline_layout,
        .renderPass = *config.swap_chain_data.render_pass,
        .subpass = 2
    };
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
    //  if (text.empty()) return;

    //const auto& command_buffer = vulkan::config::command::buffers[0];
    //  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, g_msdf_pipeline);

    //  const vk::Buffer vertex_buffers[] = { g_vertex_buffer.buffer };
    //  command_buffer.bindVertexBuffers(0, vertex_buffers, {});
    //  command_buffer.bindIndexBuffer(g_index_buffer.buffer, 0, vk::IndexType::eUint16);

    //  const struct msdf_push_constants {
    //      unitless::vec4 color;
    //  } push_constants{ color };

    //  command_buffer.pushConstants(g_msdf_pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(msdf_push_constants), &push_constants);

    //  // TODO: Ideally allocate this once, not every draw call
    //  const vk::DescriptorSet text_descriptor_set =
    //      vulkan::config::device::device.allocateDescriptorSets(
    //          vk::DescriptorSetAllocateInfo(g_msdf_descriptor_pool, 1, &g_msdf_descriptor_set_layout))[0];

    //  command_buffer.bindDescriptorSets(
    //      vk::PipelineBindPoint::eGraphics,
    //      g_msdf_pipeline_layout,
    //      0, 1,
    //      &text_descriptor_set,
    //      0, nullptr
    //  );

    //  float start_x = position.x.as_default_unit();
    //  const float start_y = position.y.as_default_unit();

    //  for (const char c : text) {
    //      const auto& [u0, v0, u1, v1, width, height, x_offset, y_offset, x_advance] = font.get_character(c);

    //      if (width == 0.0f || height == 0.0f)
    //          continue;

    //      const float x_pos = start_x + x_offset * scale;
    //      const float y_pos = start_y - (height - y_offset) * scale;
    //      const float w = width * scale;
    //      const float h = height * scale;

    //      unitless::vec4 uv_rect(u0, v0, u1 - u0, v1 - v0);

    //      draw_quad(unitless::vec2(x_pos, y_pos), unitless::vec2(w, h), font.get_texture(), uv_rect);
    //      start_x += x_advance * scale;
    //  }
}