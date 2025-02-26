export module gse.graphics.renderer2d;

import std;
import vulkan_hpp;

import gse.core.config;
import gse.graphics.font;
import gse.graphics.texture;
import gse.physics.math;
import gse.graphics.shader;
import gse.graphics.renderer3d;
import gse.graphics.camera;
import gse.platform.glfw.window;
import gse.platform.context;
import gse.platform.assert;
#include "vulkan/vulkan_structs.hpp"

export namespace gse::renderer2d {
    auto initialize() -> void;
    auto shutdown() -> void;

    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const unitless::vec4& color) -> void;
    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const texture& texture) -> void;
    auto draw_quad(const vec2<length>& position, const vec2<length>& size, const texture& texture, const unitless::vec4& uv) -> void;

    auto draw_text(const font& font, const std::string& text, const vec2<length>& position, float scale, const unitless::vec4& color) -> void;
}

vk::Pipeline            g_pipeline;
vk::PipelineLayout      g_pipeline_layout;
vk::RenderPass          g_render_pass;
vk::CommandPool         g_command_pool;
vk::CommandBuffer       g_command_buffer;
vk::Framebuffer         g_framebuffer;
vk::Buffer              g_vertex_buffer;
vk::DeviceMemory        g_vertex_memory;
vk::Buffer              g_index_buffer;
vk::DeviceMemory        g_index_memory;

gse::shader             g_shader;
gse::shader             g_msdf_shader;
gse::mat4               g_projection;

auto gse::renderer2d::initialize() -> void {
	auto device = vulkan::get_device();

    vk::AttachmentDescription color_attachment{
        {},
        vk::Format::eB8G8R8A8Srgb,   // Image format for color buffer
        vk::SampleCountFlagBits::e1, // No multi-sampling
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR
    };

	vk::AttachmentReference color_attachment_ref{
		0, vk::ImageLayout::eColorAttachmentOptimal
	};

	vk::SubpassDescription sub_pass{
		{},
		vk::PipelineBindPoint::eGraphics,
		0, nullptr,
		1, &color_attachment_ref,
		nullptr, nullptr
	};

	g_render_pass = device.createRenderPass({
		{},
		1, &color_attachment,
		1, &sub_pass
		}
    );

	g_pipeline_layout = device.createPipelineLayout({});
    
    g_shader.create(
        config::resource_path / "Shaders/2D/ui_2d_shader.vert",
        config::resource_path / "Shaders/2D/ui_2d_shader.frag"
    );
    g_msdf_shader.create(
        config::resource_path / "Shaders/2D/msdf_shader.vert",
        config::resource_path / "Shaders/2D/msdf_shader.frag"
    );

    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
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

    vk::GraphicsPipelineCreateInfo pipeline_info(
        {},
		2,
        g_shader.get_shader_stages().data(),
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
        g_render_pass
	);

    g_pipeline = device.createGraphicsPipeline({}, pipeline_info).value;

    auto create_buffer = [device](const vk::DeviceSize size, const vk::BufferUsageFlags usage, const vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory) -> void {
        const vk::BufferCreateInfo buffer_info({}, size, usage, vk::SharingMode::eExclusive);
        buffer = device.createBuffer(buffer_info);

        const vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(buffer);

        const vk::MemoryAllocateInfo alloc_info(mem_requirements.size, gse::vulkan::find_memory_type(mem_requirements.memoryTypeBits, properties));
        buffer_memory = device.allocateMemory(alloc_info);

        device.bindBufferMemory(buffer, buffer_memory, 0);
        };

    struct vertex {
        vec::raw2f position;
        vec::raw2f texture_coordinate;
    };

    constexpr vertex vertices[4] = {
        {.position = {0.0f, 1.0f}, .texture_coordinate = {0.0f, 1.0f}}, // Top-left
        {.position = {1.0f, 1.0f}, .texture_coordinate = {1.0f, 1.0f}}, // Top-right
        {.position = {1.0f, 0.0f}, .texture_coordinate = {1.0f, 0.0f}}, // Bottom-right
        {.position = {0.0f, 0.0f}, .texture_coordinate = {0.0f, 0.0f}}  // Bottom-left
    };
	constexpr std::uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

	vk::DeviceSize vertex_buffer_size = sizeof(vertices);
	vk::DeviceSize index_buffer_size = sizeof(indices);

	create_buffer(vertex_buffer_size, vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, g_vertex_buffer, g_vertex_memory);
	create_buffer(index_buffer_size, vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, g_index_buffer, g_index_memory);

    void* data = device.mapMemory(g_vertex_memory, 0, vertex_size);
    memcpy(data, vertices, static_cast<size_t>(vertex_size));
    device.unmapMemory(g_vertex_memory);

    data = device.mapMemory(g_index_memory, 0, index_size);
    memcpy(data, indices, static_cast<size_t>(index_size));
    device.unmapMemory(g_index_memory);
}

auto gse::renderer2d::shutdown() -> void {
    glDeleteVertexArrays(1, &g_vao);
    glDeleteBuffers(1, &g_vbo);
    glDeleteBuffers(1, &g_ebo);
}

auto render_quad(const gse::vec2<gse::length>& position, const gse::vec2<gse::length>& size, const gse::unitless::vec4* color, const gse::texture* texture, const gse::unitless::vec4& uv_rect = gse::unitless::vec4(0.0f, 0.0f, 1.0f, 1.0f)) -> void {
	const auto device = gse::vulkan::get_device();
    const auto queue_family = gse::vulkan::get_queue_families();

	const vk::CommandPoolCreateInfo pool_info(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family.graphics_family.value());

	g_command_pool = device.createCommandPool(pool_info);

	const vk::CommandBufferAllocateInfo alloc_info(g_command_pool, vk::CommandBufferLevel::ePrimary, 1);

    g_command_buffer = device.allocateCommandBuffers(alloc_info).front();

	constexpr vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    g_command_buffer.begin(begin_info);

    const vk::RenderPassBeginInfo render_pass_info(
        g_render_pass,
        g_framebuffer,
		vk::Rect2D({ 0, 0 }, { static_cast<std::uint32_t>(gse::window::get_window_size().x), static_cast<std::uint32_t>(gse::window::get_window_size().y) }),
        1,
        &vk::ClearValue(vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f}))
    );

    g_command_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    g_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

    const vk::Buffer vertex_buffers[] = { g_vertex_buffer };
	constexpr vk::DeviceSize offsets[] = {};

    g_command_buffer.bindVertexBuffers(0, vertex_buffers, offsets);
    g_command_buffer.bindIndexBuffer(g_index_buffer, 0, vk::IndexType::eUint16);

    g_command_buffer.drawIndexed(6, 1, 0, 0, 0);

    g_command_buffer.endRenderPass();
    g_command_buffer.end();

	constexpr vk::SubmitInfo submit_info(
		0, nullptr, nullptr,
		1, &g_command_buffer,
		0, nullptr
	);

	constexpr vk::FenceCreateInfo fence_info({});
    const vk::Fence fence = device.createFence(fence_info);

    gse::vulkan::get_graphics_queue().submit(submit_info, fence);
	perma_assert(device.waitForFences(fence, vk::True, std::numeric_limits<std::uint64_t>::max()) == vk::Result::eSuccess, "Failed to wait for fence!");

    device.destroyFence(fence);

	auto present_queue = gse::vulkan::get_present_queue();

	vk::PresentInfoKHR present_info(
		1, &gse::renderer3d::get_swap_chain(),
		&gse::vulkan::get_next_image(gse::renderer3d::get_swap_chain())
	);
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
    if (text.empty()) return;

    g_msdf_shader.use();
    g_msdf_shader.set_int("uMSDF", 0);
    g_msdf_shader.set_vec4("uColor", color);
    g_msdf_shader.set_float("uRange", 4.0f);

    const texture& font_texture = font.get_texture();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_texture.get_texture_id());

    float start_x = position.x.as_default_unit();
    const float start_y = position.y.as_default_unit();

    for (const char c : text) {
        const auto& [u0, v0, u1, v1, width, height, x_offset, y_offset, x_advance] = font.get_character(c);

        if (width == 0.0f || height == 0.0f)
            continue;

        const float x_pos = start_x + x_offset * scale;
        const float y_pos = start_y - (height - y_offset) * scale;

        const float w = width * scale;
        const float h = height * scale;

        unitless::vec4 uv_rect(u0, v0, u1 - u0, v1 - v0);

        draw_quad(unitless::vec2(x_pos, y_pos), unitless::vec2(w, h), font_texture, uv_rect);

        start_x += x_advance * scale;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}