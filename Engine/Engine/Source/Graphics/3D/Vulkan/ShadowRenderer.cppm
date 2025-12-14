export module gse.graphics:shadow_renderer;

import std;

import :base_renderer;
import :render_component;
import :mesh;
import :model;
import :shader;
import :spot_light;
import :directional_light;
import :point_light;

import gse.physics;
import gse.physics.math;
import gse.utility;

export namespace gse::renderer {
    struct shadow_light_entry {
        mat4 view;
        mat4 proj;
        std::vector<id> ignore_list_ids;
        int shadow_index = -1;
    };

    struct shadow_draw_entry {
        id owner_id;
        render_queue_entry entry;
    };

    class shadow final : public base_renderer {
    public:
        explicit shadow(
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

        auto shadow_map_view(
            std::size_t index
        ) const -> vk::ImageView;
    private:
        vk::raii::Pipeline m_pipeline = nullptr;
        vk::raii::PipelineLayout m_pipeline_layout = nullptr;

        resource::handle<shader> m_shader;

        double_buffer<std::vector<shadow_draw_entry>> m_render_queue;
        double_buffer<std::vector<shadow_light_entry>> m_shadow_lights;

        unitless::vec2u m_shadow_extent = { 1024, 1024 };

        std::array<vulkan::persistent_allocator::image_resource, 10> m_shadow_maps;
    };
}

namespace gse::renderer {
    constexpr std::size_t max_shadow_lights = 10;

    auto ensure_non_collinear_up(
        const unitless::vec3& direction,
        const unitless::vec3& up
    ) -> unitless::vec3;
}

gse::renderer::shadow::shadow(context& context)
    : base_renderer(context) {
}

auto gse::renderer::shadow::initialize() -> void {
    auto& config = m_context.config();

    m_shader = m_context.get<shader>("shadow_pass");
    m_context.instantly_load(m_shader);

    auto layouts = m_shader->layouts();

    auto range = m_shader->push_constant_range("push_constants");

    const vk::PipelineLayoutCreateInfo pipeline_layout_info{
        .setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = 1u,
        .pPushConstantRanges = &range
    };

    m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

    auto shader_stages = m_shader->shader_stages();
    auto vertex_input = m_shader->vertex_input_state();

    constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False
    };

    constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::True,
        .depthBiasConstantFactor = 1.25f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 1.75f,
        .lineWidth = 1.0f
    };

    constexpr vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = vk::False,
        .alphaToOneEnable = vk::False
    };

    constexpr vk::PipelineColorBlendStateCreateInfo color_blend_state{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 0u,
        .pAttachments = nullptr,
        .blendConstants = std::array{ 0.0f, 0.0f, 0.0f, 0.0f }
    };

    constexpr auto depth_format = vk::Format::eD32Sfloat;

    const vk::PipelineRenderingCreateInfoKHR rendering_info{
        .colorAttachmentCount = 0u,
        .pColorAttachmentFormats = nullptr,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = vk::Format::eUndefined
    };

    const vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_shadow_extent.x()),
        .height = static_cast<float>(m_shadow_extent.y()),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    const vk::Rect2D scissor{
        .offset = { 0, 0 },
        .extent = { m_shadow_extent.x(), m_shadow_extent.y() }
    };

    const vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1u,
        .pViewports = &viewport,
        .scissorCount = 1u,
        .pScissors = &scissor
    };

    const vk::GraphicsPipelineCreateInfo pipeline_info{
        .pNext = &rendering_info,
        .stageCount = static_cast<std::uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = nullptr,
        .layout = m_pipeline_layout,
        .basePipelineHandle = nullptr,
        .basePipelineIndex = -1
    };

    m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

    const vk::ImageCreateInfo image_info{
        .flags = {},
        .imageType = vk::ImageType::e2D,
        .format = vk::Format::eD32Sfloat,
        .extent = {
            .width = m_shadow_extent.x(),
            .height = m_shadow_extent.y(),
            .depth = 1u
        },
        .mipLevels = 1u,
        .arrayLayers = 1u,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined
    };

    const vk::ImageViewCreateInfo view_info{
        .flags = {},
        .image = nullptr,
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eD32Sfloat,
        .components = {},
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eDepth,
            .baseMipLevel = 0u,
            .levelCount = 1u,
            .baseArrayLayer = 0u,
            .layerCount = 1u
        }
    };

    for (auto& img : m_shadow_maps) {
        img = vulkan::persistent_allocator::create_image(
            config.device_config(),
            image_info,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            view_info
        );
    }

    frame_sync::on_end([this] {
        m_render_queue.flip();
		m_shadow_lights.flip();
    });
}

auto gse::renderer::shadow::update(const std::span<const std::reference_wrapper<registry>> registries) -> void {
    if (registries.empty()) {
        return;
    }

    auto& lights_out = m_shadow_lights.write();
    lights_out.clear();
    std::size_t next_shadow_index = 0;

    for (auto& reg_ref : registries) {
        auto& reg = reg_ref.get();

        for (const auto& comp : reg.linked_objects_read<directional_light_component>()) {
            if (next_shadow_index >= max_shadow_lights) {
                break;
            }

            shadow_light_entry entry;
            entry.shadow_index = static_cast<int>(next_shadow_index);

            auto dir = comp.direction;
            vec3<length> light_pos = -dir * 10.0f;

            entry.proj = orthographic(
                meters(-10000.0f),
                meters(10000.0f),
                meters(-10000.0f),
                meters(1000.0f),
                comp.near_plane,
                comp.far_plane
            );

            auto up = ensure_non_collinear_up(dir, { 0.0f, 1.0f, 0.0f });

            entry.view = look_at(
                light_pos,
                light_pos + vec3<length>(dir),
                up
            );

            entry.ignore_list_ids = comp.ignore_list_ids;

            lights_out.push_back(std::move(entry));
            ++next_shadow_index;
        }

        for (const auto& comp : reg.linked_objects_read<spot_light_component>()) {
            if (next_shadow_index >= max_shadow_lights) {
                break;
            }

            shadow_light_entry entry;
            entry.shadow_index = static_cast<int>(next_shadow_index);

            auto pos = comp.position;
            auto dir = comp.direction;
            const auto cutoff = comp.cut_off;

            entry.proj = perspective(
                cutoff,
                1.0f,
                comp.near_plane,
                comp.far_plane
            );

            auto up = ensure_non_collinear_up(dir, { 0.0f, 1.0f, 0.0f });

            entry.view = look_at(
                pos,
                pos + vec3<length>(dir),
                up
            );

            entry.ignore_list_ids = comp.ignore_list_ids;

            lights_out.push_back(std::move(entry));
            ++next_shadow_index;
        }
    }

    auto& out = m_render_queue.write();
    out.clear();

    for (auto& reg_ref : registries) {
        for (auto& reg = reg_ref.get(); auto& component : reg.linked_objects_write<render_component>()) {
            if (!component.render) {
                continue;
            }

            for (auto& model_handle : component.model_instances) {
                if (!model_handle.handle().valid()) {
                    continue;
                }

                for (const auto& rq : model_handle.render_queue_entries()) {
                    out.push_back(shadow_draw_entry{
                        component.owner_id(),
                        rq
                    });
                }
            }
        }
    }
}

auto gse::renderer::shadow::render(std::span<const std::reference_wrapper<registry>> registries) -> void {
    auto& config = m_context.config();
    auto command = config.frame_context().command_buffer;

    std::array<mat4, max_shadow_lights> shadow_view_proj{};
    std::size_t shadow_light_count = 0;

    for (auto& reg_ref : registries) {
        auto& reg = reg_ref.get();
        if (shadow_light_count >= max_shadow_lights) {
            break;
        }

        for (const auto& comp : reg.linked_objects_read<directional_light_component>()) {
            if (shadow_light_count >= max_shadow_lights) {
                break;
            }

            auto dir = comp.direction;
            vec3<length> light_pos = -dir * 10.0f;
            auto up = ensure_non_collinear_up(dir, { 0.0f, 1.0f, 0.0f });

            auto light_proj = orthographic(
                meters(-10000.0f),
                meters(10000.0f),
                meters(-10000.0f),
                meters(1000.0f),
                comp.near_plane,
                comp.far_plane
            );

            auto light_view = look_at(
                light_pos,
                light_pos + vec3<length>(dir),
                up
            );

            shadow_view_proj[shadow_light_count] = light_proj * light_view;
            ++shadow_light_count;
        }

        for (const auto& comp : reg.linked_objects_read<point_light_component>()) {
            if (shadow_light_count >= max_shadow_lights) {
                break;
            }

            auto pos = comp.position;
            auto dir = unitless::vec3(0.0f, -1.0f, 0.0f);
            auto up = unitless::vec3(0.0f, 0.0f, 1.0f);

            auto light_proj = perspective(
                radians(90.0f),
                1.0f,
                comp.near_plane,
                comp.far_plane
            );

            auto light_view = look_at(
                pos,
                pos + vec3<length>(dir),
                up
            );

            shadow_view_proj[shadow_light_count] = light_proj * light_view;
            ++shadow_light_count;
        }

        for (const auto& comp : reg.linked_objects_read<spot_light_component>()) {
            if (shadow_light_count >= max_shadow_lights) {
                break;
            }

            auto pos = comp.position;
            auto dir = comp.direction;
            auto up = ensure_non_collinear_up(dir, { 0.0f, 1.0f, 0.0f });

            auto light_proj = perspective(
                comp.cut_off,
                1.0f,
                comp.near_plane,
                comp.far_plane
            );

            auto light_view = look_at(
                pos,
                pos + vec3<length>(dir),
                up
            );

            shadow_view_proj[shadow_light_count] = light_proj * light_view;
            ++shadow_light_count;
        }
    }

    for (std::size_t i = 0; i < shadow_light_count; ++i) {
        auto& depth_image = m_shadow_maps[i];

        vulkan::uploader::transition_image_layout(
            command,
            depth_image,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::ImageAspectFlagBits::eDepth,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderSampledRead,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        );

        vk::RenderingAttachmentInfo depth_attachment{
            .imageView = *depth_image.view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{.depthStencil = { 1.0f, 0 } }
        };

        const vk::RenderingInfo rendering_info{
            .renderArea = { { 0, 0 }, m_shadow_extent.x(), m_shadow_extent.y() },
            .layerCount = 1,
            .colorAttachmentCount = 0,
            .pColorAttachments = nullptr,
            .pDepthAttachment = &depth_attachment
        };

        vulkan::render(config, rendering_info, [&] {
            command.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

            for (auto& reg_ref : registries) {
	            for (auto& reg = reg_ref.get(); auto& render : reg.linked_objects_read<render_component>()) {
                    mat4 model = transform.matrix();

                    struct {
                        mat4 light_view_proj;
                        mat4 model;
                    } pc{ light_view_proj, model };

                    command.pushConstants(
                        *m_pipeline_layout,
                        vk::ShaderStageFlagBits::eVertex,
                        0,
                        sizeof(pc),
                        &pc
                    );

                    mesh.draw(command);
                }
            }
        });

        vulkan::uploader::transition_image_layout(
            command,
            depth_image,
            vk::ImageLayout::eDepthReadOnlyOptimal,
            vk::ImageAspectFlagBits::eDepth,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderSampledRead
        );
    }
}

auto gse::renderer::shadow::shadow_map_view(const std::size_t index) const -> vk::ImageView {
    return m_shadow_maps[index].view;
}

auto gse::renderer::ensure_non_collinear_up(const unitless::vec3& direction, const unitless::vec3& up) -> unitless::vec3 {
    auto normalized_direction = normalize(direction);
    auto normalized_up = normalize(up);
    if (const float dot_product = dot(normalized_direction, normalized_up); std::abs(dot_product) > 1.0f - std::numeric_limits<float>::epsilon()) {
        if (std::abs(normalized_direction.y()) > 0.9f) {
            normalized_up = unitless::vec3(0.0f, 0.0f, 1.0f);
        } else {
            normalized_up = unitless::vec3(0.0f, 1.0f, 0.0f);
        }
    }
    return normalized_up;
}
