export module gse.graphics.renderer2d;

import std;
import std.compat;
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
vk::Image               g_render_target;
vk::DeviceMemory        g_render_target_memory;
vk::ImageView           g_render_target_view;

vk::Pipeline 		    g_msdf_pipeline;
vk::PipelineLayout 	    g_msdf_pipeline_layout;
vk::DescriptorSetLayout g_msdf_descriptor_set_layout;
vk::DescriptorPool      g_msdf_descriptor_pool;

gse::shader             g_shader;
gse::shader             g_msdf_shader;
gse::mat4               g_projection;

auto gse::renderer2d::initialize() -> void {
	auto [physical_device, device] = vulkan::get_device_config();
	auto swap_chain_extent = vulkan::get_swap_chain_config().extent;

    vk::ImageCreateInfo image_info(
        {}, 
        vk::ImageType::e2D, 
        vk::Format::eB8G8R8A8Srgb,
        { swap_chain_extent.width, swap_chain_extent.height, 1 },
        1, 1, 
        vk::SampleCountFlagBits::e1, 
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive
    );

    g_render_target = device.createImage(image_info);

    vk::MemoryRequirements mem_reqs = device.getImageMemoryRequirements(g_render_target);
    vk::MemoryAllocateInfo alloc_info(mem_reqs.size, vulkan::find_memory_type(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    g_render_target_memory = device.allocateMemory(alloc_info);

    device.bindImageMemory(g_render_target, g_render_target_memory, 0);

    vk::ImageViewCreateInfo view_info(
        {}, 
        g_render_target, 
        vk::ImageViewType::e2D, 
        vk::Format::eB8G8R8A8Srgb,
        {}, 
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );

    g_render_target_view = device.createImageView(view_info);

    vk::FramebufferCreateInfo framebuffer_info(
        {}, 
        g_render_pass, 
        1, 
        &g_render_target_view, 
        swap_chain_extent.width, 
        swap_chain_extent.height, 
        1
    );
    g_framebuffer = device.createFramebuffer(framebuffer_info);

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

    vk::PushConstantRange push_constant_range(
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, sizeof(unitless::vec2) * 2 + sizeof(unitless::vec4) * 2
    );

	g_pipeline_layout = device.createPipelineLayout({{}, 1, nullptr, 1, &push_constant_range });
    
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

        const vk::MemoryAllocateInfo memory_allocate_info(mem_requirements.size, vulkan::find_memory_type(mem_requirements.memoryTypeBits, properties));
        buffer_memory = device.allocateMemory(memory_allocate_info);

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

    void* data = device.mapMemory(g_vertex_memory, 0, vertex_buffer_size);
    memcpy(data, vertices, vertex_buffer_size);
    device.unmapMemory(g_vertex_memory);

    data = device.mapMemory(g_index_memory, 0, index_buffer_size);
    memcpy(data, indices, index_buffer_size);
    device.unmapMemory(g_index_memory);

	/// Generate MSDF pipeline

    vk::PushConstantRange msdf_push_constant_range(
        vk::ShaderStageFlagBits::eFragment,
        0, sizeof(unitless::vec4)
    );

	vk::PipelineLayoutCreateInfo pipeline_layout_info({}, 1, nullptr, 1, &msdf_push_constant_range);
    g_msdf_pipeline_layout = device.createPipelineLayout(pipeline_layout_info);

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
        g_msdf_shader.get_shader_stages().data(),
        &vertex_input_info,
        &input_assembly,
        nullptr,
        &viewport_state,
        &rasterizer,
        &multisampling,
        nullptr,
        &msdf_color_blending,
        nullptr,
        g_msdf_pipeline_layout,
        g_render_pass
    );

    g_msdf_pipeline = device.createGraphicsPipeline({}, msdf_pipeline_info).value;

    vk::DescriptorSetLayoutBinding texture_binding(
        0, vk::DescriptorType::eCombinedImageSampler, 1,
        vk::ShaderStageFlagBits::eFragment
    );

    vk::DescriptorSetLayoutCreateInfo layout_info({}, 1, & texture_binding);
    g_msdf_descriptor_set_layout = device.createDescriptorSetLayout(layout_info);

    vk::DescriptorPoolSize pool_size(vk::DescriptorType::eCombinedImageSampler, 100);  // Adjust pool size as needed
    vk::DescriptorPoolCreateInfo pool_info({}, 100, 1, & pool_size);
    g_msdf_descriptor_pool = device.createDescriptorPool(pool_info);
}

auto gse::renderer2d::shutdown() -> void {
    auto [physical_device, device] = vulkan::get_device_config();

    device.destroyPipeline(g_pipeline);
    device.destroyPipelineLayout(g_pipeline_layout);
    device.destroyRenderPass(g_render_pass);
    device.destroyFramebuffer(g_framebuffer);
    device.destroyImageView(g_render_target_view);
    device.destroyImage(g_render_target);
    device.freeMemory(g_render_target_memory);
    device.destroyBuffer(g_vertex_buffer);
    device.freeMemory(g_vertex_memory);
    device.destroyBuffer(g_index_buffer);
    device.freeMemory(g_index_memory);
    device.destroyCommandPool(g_command_pool);
}

auto render_quad(const gse::vec2<gse::length>& position, const gse::vec2<gse::length>& size, const gse::unitless::vec4* color, const gse::texture* texture, const gse::unitless::vec4& uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f }) -> void {
    auto [physical_device, device] = gse::vulkan::get_device_config();
    auto [graphics_queue, present_queue] = gse::vulkan::get_queue_config();

    const vk::Semaphore image_available_semaphore = device.createSemaphore({});
    const vk::Semaphore render_finished_semaphore = device.createSemaphore({});

    std::uint32_t image_index = gse::vulkan::get_next_image();

    constexpr vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    g_command_buffer.begin(begin_info);

    constexpr vk::ClearValue clear_value(vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f}));
    const vk::RenderPassBeginInfo render_pass_info(g_render_pass, g_framebuffer, { {0, 0}, { static_cast<std::uint32_t>(gse::window::get_window_size().x), static_cast<std::uint32_t>(gse::window::get_window_size().y) } }, 1, &clear_value);

    g_command_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    g_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, g_pipeline);

    const vk::Buffer vertex_buffers[] = { g_vertex_buffer };
    constexpr vk::DeviceSize offsets[] = {};
    g_command_buffer.bindVertexBuffers(0, vertex_buffers, offsets);
    g_command_buffer.bindIndexBuffer(g_index_buffer, 0, vk::IndexType::eUint16);

    const struct push_constants {
        gse::unitless::vec2 position;
        gse::unitless::vec2 size;
		gse::unitless::vec4 color;
		gse::unitless::vec4 uv_rect;
    } push_constants {
        { position.x.as_default_unit(), position.y.as_default_unit() },
        { size.x.as_default_unit(), size.y.as_default_unit() },
		color ? *color : gse::unitless::vec4(1.0f),
		uv_rect
    };

    g_command_buffer.pushConstants(g_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(push_constants), &push_constants);
    g_command_buffer.drawIndexed(6, 1, 0, 0, 0);

    g_command_buffer.endRenderPass();
    g_command_buffer.end();

    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    const vk::SubmitInfo submit_info(1, &image_available_semaphore, wait_stages, 1, &g_command_buffer, 1, &render_finished_semaphore);

    graphics_queue.submit(submit_info, nullptr);

	const auto& swap_chain = gse::vulkan::get_swap_chain_config().swap_chain;

    const vk::PresentInfoKHR present_info(1, &render_finished_semaphore, 1, &swap_chain, &image_index);
	perma_assert(present_queue.presentKHR(present_info) != vk::Result::eSuccess, "Failed to present image!");

    device.destroySemaphore(image_available_semaphore);
    device.destroySemaphore(render_finished_semaphore);
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

    constexpr vk::ClearValue clear_value = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f});

    const vk::RenderPassBeginInfo render_pass_info(
        g_render_pass,
        g_framebuffer,
        { {0, 0}, { static_cast<std::uint32_t>(window::get_window_size().x), static_cast<std::uint32_t>(window::get_window_size().y) } }, // Render area
        1, &clear_value
    );

    g_command_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    g_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, g_msdf_pipeline);

    const vk::Buffer vertex_buffers[] = { g_vertex_buffer };
    constexpr vk::DeviceSize offsets[] = {};
    g_command_buffer.bindVertexBuffers(0, vertex_buffers, offsets);
    g_command_buffer.bindIndexBuffer(g_index_buffer, 0, vk::IndexType::eUint16);

    const struct msdf_push_constants {
        unitless::vec4 color;
    } push_constants{ color };

    g_command_buffer.pushConstants(g_msdf_pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(msdf_push_constants), &push_constants);

    const vk::DescriptorSet text_descriptor_set = font.get_texture().get_descriptor_set();
    g_command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g_msdf_pipeline_layout, 0, 1, &text_descriptor_set, 0, nullptr);

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

        draw_quad(unitless::vec2(x_pos, y_pos), unitless::vec2(w, h), font.get_texture(), uv_rect);
        start_x += x_advance * scale;
    }

    g_command_buffer.endRenderPass();
    g_command_buffer.end();

    constexpr vk::SubmitInfo submit_info({}, {}, {}, 1, &g_command_buffer);
    auto [graphics_queue, present_queue] = vulkan::get_queue_config();
    graphics_queue.submit(submit_info, nullptr);
}
