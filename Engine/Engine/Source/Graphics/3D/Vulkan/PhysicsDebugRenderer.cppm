export module gse.graphics:physics_debug_renderer;

import std;

import gse.physics;
import gse.physics.math;
import gse.utility;
import gse.platform;

import :camera;
import :shader;

export namespace gse::renderer {
    struct physics_debug final : ecs_system<
        read_set<physics::collision_component, physics::motion_component>,
        write_set<>
    > {
        using schedule = system_schedule<
            system_stage<
                system_stage_kind::update,
                gse::read_set<physics::collision_component, physics::motion_component>,
                gse::write_set<>
            >
        >;

        explicit physics_debug(
            context& context,
            std::vector<std::reference_wrapper<registry>> registries
        ) : ecs_system(std::move(registries)), m_context(context) {
        }

        auto initialize(
        ) -> void override;

        auto update(
        ) -> void override;

        auto render(
        ) -> void override;

        auto set_enabled(
            const bool enabled
        ) -> void {
            m_enabled = enabled;
        }

        auto enabled(
        ) const -> bool {
            return m_enabled;
        }
    private:
        struct debug_vertex {
            unitless::vec3 position;
            unitless::vec3 color;
        };

        context& m_context;

        vk::raii::Pipeline m_pipeline = nullptr;
        vk::raii::PipelineLayout m_pipeline_layout = nullptr;
        vk::raii::DescriptorSet m_descriptor_set = nullptr;

        resource::handle<shader> m_shader;

        std::unordered_map<std::string, vulkan::persistent_allocator::buffer_resource> m_ubo_allocations;
        vulkan::persistent_allocator::buffer_resource m_vertex_buffer;

        double_buffer<std::vector<debug_vertex>> m_vertices;
        std::size_t m_max_vertices = 0;
        bool m_enabled = true;

        auto ensure_vertex_capacity(
            std::size_t required_vertex_count
        ) -> void;

        static auto add_line(
            const vec3<length>& a,
            const vec3<length>& b,
            const unitless::vec3& color,
            std::vector<debug_vertex>& out_vertices
        ) -> void;

        static auto build_obb_lines_for_collider(
            const physics::collision_component& coll,
            std::vector<debug_vertex>& out_vertices
        ) -> void;

        static auto build_contact_debug_for_collider(
            const physics::collision_component& coll,
            const physics::motion_component& mc,
            std::vector<debug_vertex>& out_vertices
        ) -> void;
    };
}

auto gse::renderer::physics_debug::ensure_vertex_capacity(const std::size_t required_vertex_count) -> void {
    if (required_vertex_count <= m_max_vertices && *m_vertex_buffer.buffer) {
        return;
    }

    auto& config = m_context.config();
    if (*m_vertex_buffer.buffer) {
        vulkan::persistent_allocator::free(m_vertex_buffer.allocation);
        m_vertex_buffer = {};
    }

    m_max_vertices = std::max<std::size_t>(required_vertex_count, 4096);

    const vk::BufferCreateInfo buffer_info{
        .size = m_max_vertices * sizeof(debug_vertex),
        .usage = vk::BufferUsageFlagBits::eVertexBuffer,
        .sharingMode = vk::SharingMode::eExclusive
    };

    const std::vector<std::byte> zeros(buffer_info.size);
    m_vertex_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_config(),
        buffer_info,
        zeros.data()
    );
}

auto gse::renderer::physics_debug::initialize() -> void {
    auto& config = m_context.config();

    m_shader = m_context.get<shader>("physics_debug");
    m_context.instantly_load(m_shader);
    auto descriptor_set_layouts = m_shader->layouts();

    m_descriptor_set = m_shader->descriptor_set(
        config.device_config().device,
        config.descriptor_config().pool,
        shader::set::binding_type::persistent
    );

    const auto camera_ubo = m_shader->uniform_block("CameraUBO");

    vk::BufferCreateInfo camera_ubo_buffer_info{
        .size = camera_ubo.size,
        .usage = vk::BufferUsageFlagBits::eUniformBuffer,
        .sharingMode = vk::SharingMode::eExclusive
    };

    auto camera_ubo_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_config(),
        camera_ubo_buffer_info
    );

    m_ubo_allocations["CameraUBO"] = std::move(camera_ubo_buffer);

    std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
        {
            "CameraUBO",
            {
                .buffer = m_ubo_allocations["CameraUBO"].buffer,
                .offset = 0,
                .range = camera_ubo.size
            }
        }
    };

    std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

    config.device_config().device.updateDescriptorSets(
        m_shader->descriptor_writes(m_descriptor_set, buffer_infos, image_infos),
        nullptr
    );

    const vk::PipelineLayoutCreateInfo pipeline_layout_info{
        .setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

    auto shader_stages = m_shader->shader_stages();
    auto vertex_input_info = m_shader->vertex_input_state();

    constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eLineList,
        .primitiveRestartEnable = vk::False
    };

    constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eLine,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    constexpr vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .depthTestEnable = vk::False,
        .depthWriteEnable = vk::False,
        .depthCompareOp = vk::CompareOp::eLess,
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

    constexpr vk::PipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable = vk::True,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    const vk::PipelineColorBlendStateCreateInfo color_blending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    const auto color_format = config.swap_chain_config().surface_format.format;

    const vk::PipelineRenderingCreateInfoKHR rendering_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format
    };

    const auto extent = config.swap_chain_config().extent;

    const vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    const vk::Rect2D scissor{
        .offset = { 0, 0 },
        .extent = extent
    };

    const vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    const vk::GraphicsPipelineCreateInfo pipeline_info{
        .pNext = &rendering_info,
        .stageCount = static_cast<std::uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = nullptr,
        .layout = m_pipeline_layout,
        .basePipelineHandle = nullptr,
        .basePipelineIndex = 0
    };

    m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

    frame_sync::on_end([this] {
        m_vertices.flip();
    });
}

auto gse::renderer::physics_debug::add_line(
    const vec3<length>& a,
    const vec3<length>& b,
    const unitless::vec3& color,
    std::vector<debug_vertex>& out_vertices
) -> void {
    const auto pa = a.as<meters>().data;
    const auto pb = b.as<meters>().data;

    out_vertices.push_back(debug_vertex{ pa, color });
    out_vertices.push_back(debug_vertex{ pb, color });
}

auto gse::renderer::physics_debug::build_obb_lines_for_collider(const physics::collision_component& coll, std::vector<debug_vertex>& out_vertices) -> void {
    const auto bb = coll.bounding_box;
    std::array<vec3<length>, 8> corners;

    const auto half = bb.half_extents();
    const auto axes = bb.obb().axes;
    const auto center = bb.center();

    int idx = 0;
    for (int i = 0; i < 8; ++i) {
        const auto x = (i & 1 ? 1.0f : -1.0f) * half.x();
        const auto y = (i & 2 ? 1.0f : -1.0f) * half.y();
        const auto z = (i & 4 ? 1.0f : -1.0f) * half.z();
        corners[idx++] = center + axes[0] * x + axes[1] * y + axes[2] * z;
    }

    static constexpr std::array<std::pair<int, int>, 12> edges{ {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    } };

    constexpr unitless::vec3 color{ 0.0f, 1.0f, 0.0f };

    for (const auto& [fst, snd] : edges) {
        add_line(corners[fst], corners[snd], color, out_vertices);
    }
}

auto gse::renderer::physics_debug::build_contact_debug_for_collider(const physics::collision_component& coll, const physics::motion_component& mc, std::vector<debug_vertex>& out_vertices) -> void {
    const auto& [colliding, collision_normal, penetration, collision_point] = coll.collision_information;
    if (!colliding || mc.position_locked) {
        return;
    }

    const auto p = collision_point;
    const auto n = collision_normal;

    constexpr length cross_size = meters(0.1f);

    constexpr unitless::vec3 cross_color{ 1.0f, 1.0f, 1.0f };
    constexpr unitless::vec3 normal_color{ 1.0f, 0.0f, 0.0f };
    constexpr unitless::vec3 penetration_color{ 1.0f, 1.0f, 0.0f };
    constexpr unitless::vec3 center_link_color{ 0.0f, 1.0f, 1.0f };

    const vec3<length> px1 = p + vec3<length>{ cross_size, 0.0f, 0.0f };
    const vec3<length> px2 = p - vec3<length>{ cross_size, 0.0f, 0.0f };

    const vec3<length> py1 = p + vec3<length>{ 0.0f, cross_size, 0.0f };
    const vec3<length> py2 = p - vec3<length>{ 0.0f, cross_size, 0.0f };

    const vec3<length> pz1 = p + vec3<length>{ 0.0f, 0.0f, cross_size };
    const vec3<length> pz2 = p - vec3<length>{ 0.0f, 0.0f, cross_size };

    add_line(px1, px2, cross_color, out_vertices);
    add_line(py1, py2, cross_color, out_vertices);
    add_line(pz1, pz2, cross_color, out_vertices);

    const length normal_len = std::min(penetration, meters(0.5f));
    const vec3<length> normal_end = p + n * normal_len;
    add_line(p, normal_end, normal_color, out_vertices);

    const vec3<length> projected = p - n * penetration;
    add_line(p, projected, penetration_color, out_vertices);

    const auto center = coll.bounding_box.center();
    add_line(center, p, center_link_color, out_vertices);
}

auto gse::renderer::physics_debug::update() -> void {
    if (!m_enabled) {
        return;
    }

    auto& vertices = m_vertices.write();
    vertices.clear();

    this->for_each_read_chunk<physics::collision_component>(
        [&](const auto& chunk) {
            for (const physics::collision_component& coll : chunk) {
                if (!coll.resolve_collisions) {
                    continue;
                }

                build_obb_lines_for_collider(coll, vertices);

                const auto* mc = chunk.template other_read_from<physics::motion_component>(coll);
                if (!mc) {
                    continue;
                }

                build_contact_debug_for_collider(coll, *mc, vertices);
            }
        }
    );

    if (!vertices.empty()) {
        ensure_vertex_capacity(vertices.size());

        const auto byte_count = vertices.size() * sizeof(debug_vertex);
        if (void* dst = m_vertex_buffer.allocation.mapped()) {
            std::memcpy(dst, vertices.data(), byte_count);
        }
    }
}

auto gse::renderer::physics_debug::render() -> void {
    if (!m_enabled) {
        return;
    }

    auto& config = m_context.config();
    const auto command = config.frame_context().command_buffer;

    const auto cam_block = m_shader->uniform_block("CameraUBO");

    m_shader->set_uniform("CameraUBO.view", m_context.camera().view(), m_ubo_allocations.at("CameraUBO").allocation);
    m_shader->set_uniform("CameraUBO.proj", m_context.camera().projection(m_context.window().viewport()), m_ubo_allocations.at("CameraUBO").allocation);

    if (m_vertices.read().empty()) {
        return;
    }

    vk::RenderingAttachmentInfo color_attachment{
        .imageView = *config.swap_chain_config().image_views[config.frame_context().image_index],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore
    };

    const vk::RenderingInfo rendering_info{
        .renderArea = { { 0, 0 }, config.swap_chain_config().extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = nullptr
    };

    vulkan::render(config, rendering_info, [&] {
        const auto& verts = m_vertices.read();
        if (verts.empty()) {
            return;
        }

        command.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        const vk::DescriptorSet sets[] = {
            m_descriptor_set
        };

        command.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            m_pipeline_layout,
            0,
            vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
            {}
        );

        const vk::Buffer vb = m_vertex_buffer.buffer;
        constexpr vk::DeviceSize offsets[] = { 0 };

        command.bindVertexBuffers(0, 1, &vb, offsets);

        command.draw(static_cast<std::uint32_t>(verts.size()), 1, 0, 0);
    });
}
