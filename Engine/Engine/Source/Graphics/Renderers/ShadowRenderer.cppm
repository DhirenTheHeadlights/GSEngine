export module gse.graphics:shadow_renderer;

import std;

import :render_component;
import :mesh;
import :model;
import :spot_light;
import :directional_light;
import :point_light;
import :cube_map;
import :shadow_data;
import :geometry_collector;

import gse.platform;
import gse.physics;
import gse.math;
import gse.utility;

export namespace gse::renderer::shadow {
	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::shadow::state::shadow_map_view(const std::size_t index) const -> vk::ImageView {
	return shadow_maps[index].view;
}

auto gse::renderer::shadow::state::point_shadow_cube_view(const std::size_t index) const -> vk::ImageView {
	return point_shadow_cubemaps[index].image_resource().view;
}

auto gse::renderer::shadow::state::point_shadow_face_view(const std::size_t index, const std::size_t face) const -> vk::ImageView {
	return point_shadow_cubemaps[index].face_view(face);
}

auto gse::renderer::shadow::state::point_shadow_sampler(const std::size_t index) const -> vk::Sampler {
	return point_shadow_cubemaps[index].sampler();
}

auto gse::renderer::shadow::state::shadow_texel_size() const -> vec2f {
	return vec2f(
		1.0f / static_cast<float>(shadow_extent.x()),
		1.0f / static_cast<float>(shadow_extent.y())
	);
}

auto gse::renderer::shadow::system::initialize(initialize_phase&, state& s) -> void {
	auto& config = s.ctx->config();

	s.shader_handle = s.ctx->get<shader>("Shaders/Deferred3D/shadow_pass");
	s.ctx->instantly_load(s.shader_handle);

	auto layouts = s.shader_handle->layouts();
	auto range = s.shader_handle->push_constant_range("push_constants");

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data(),
		.pushConstantRangeCount = 1u,
		.pPushConstantRanges = &range
	};

	s.pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	auto shader_stages = s.shader_handle->shader_stages();
	auto vertex_input = s.shader_handle->vertex_input_state();

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

	const vk::PipelineRenderingCreateInfoKHR rendering_info{
		.colorAttachmentCount = 0u,
		.pColorAttachmentFormats = nullptr,
		.depthAttachmentFormat = vk::Format::eD32Sfloat,
		.stencilAttachmentFormat = vk::Format::eUndefined
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state{
		.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	const vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1u,
		.pViewports = nullptr,
		.scissorCount = 1u,
		.pScissors = nullptr
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
		.pDynamicState = &dynamic_state,
		.layout = s.pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};

	s.pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

	const vk::ImageCreateInfo image_info{
		.flags = {},
		.imageType = vk::ImageType::e2D,
		.format = vk::Format::eD32Sfloat,
		.extent = {
			.width = s.shadow_extent.x(),
			.height = s.shadow_extent.y(),
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

	constexpr vk::ImageViewCreateInfo view_info{
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

	for (auto& img : s.shadow_maps) {
		img = config.allocator().create_image(
			image_info,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			view_info
		);
	}

	for (auto& cm : s.point_shadow_cubemaps) {
		cm.create(config, static_cast<int>(s.point_shadow_extent.x()), true);
	}

	config.add_transient_work(
        [&s](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
            std::vector<vk::ImageMemoryBarrier2> barriers;
            barriers.reserve(s.shadow_maps.size() + s.point_shadow_cubemaps.size());

            for (auto& img : s.shadow_maps) {
                barriers.push_back({
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = {},
                    .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                    .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = img.image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                });

                img.current_layout = vk::ImageLayout::eGeneral;
            }

            for (auto& cm : s.point_shadow_cubemaps) {
                barriers.push_back({
                    .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                    .srcAccessMask = {},
                    .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                    .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral,
                    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                    .image = cm.image_resource().image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 6
                    }
                });
            }

            const vk::DependencyInfo dep{
                .imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
                .pImageMemoryBarriers = barriers.data()
            };

            cmd.pipelineBarrier2(dep);

            return {};
        }
    );
}

auto gse::renderer::shadow::system::update(update_phase& phase, state& s) -> void {
	render_data data;

	std::size_t next_shadow_index = 0;

	const auto dir_view = phase.registry.view<directional_light_component>();
	const auto spot_view = phase.registry.view<spot_light_component>();
	const auto point_view = phase.registry.view<point_light_component>();

	for (const auto& comp : dir_view) {
		if (next_shadow_index >= max_shadow_lights) {
			break;
		}

		shadow_light_entry entry;
		entry.shadow_index = static_cast<int>(next_shadow_index);

		auto dir = comp.direction;
		vec3<length> light_pos = -dir * 10.0f;

		entry.proj = orthographic(
			meters(-100.0f),
			meters(100.0f),
			meters(-100.0f),
			meters(100.0f),
			comp.near_plane,
			comp.far_plane
		);

		auto up = ensure_non_collinear_up(dir, axis_y);

		entry.view = look_at(
			light_pos,
			light_pos + vec3<length>(dir),
			up
		);

		entry.ignore_list_ids = comp.ignore_list_ids;

		data.lights.push_back(std::move(entry));
		++next_shadow_index;
	}

	for (const auto& comp : spot_view) {
		if (next_shadow_index >= max_shadow_lights) {
			break;
		}

		shadow_light_entry entry;
		entry.shadow_index = static_cast<int>(next_shadow_index);

		const vec3<length> pos = vec3<length>(comp.position - vec3<position>{});
		auto dir = comp.direction;
		const auto cutoff = comp.cut_off;

		entry.proj = perspective(
			cutoff,
			1.0f,
			comp.near_plane,
			comp.far_plane
		);

		auto up = ensure_non_collinear_up(dir, axis_y);

		entry.view = look_at(
			pos,
			pos + vec3<length>(dir),
			up
		);

		entry.ignore_list_ids = comp.ignore_list_ids;

		data.lights.push_back(std::move(entry));
		++next_shadow_index;
	}

	std::size_t next_point_shadow_index = 0;

	for (const auto& comp : point_view) {
		if (next_point_shadow_index >= max_point_shadow_lights) {
			break;
		}

		point_shadow_light_entry entry;
		entry.point_shadow_index = static_cast<int>(next_point_shadow_index);
		entry.world_position = vec3<length>(comp.position - vec3<position>{});
		entry.near_plane = comp.near_plane;
		entry.far_plane = comp.far_plane;

		entry.ignore_list_ids = comp.ignore_list_ids;

		const auto proj = perspective(
			degrees(90.0f),
			1.0f,
			comp.near_plane,
			comp.far_plane
		);

		const vec3<length> pos = vec3<length>(comp.position - vec3<position>{});

		entry.face_view_proj[0] = proj * look_at(
			pos, pos + vec3<length>(meters(1.0f), meters(0.0f), meters(0.0f)),
			-axis_y
		);
		entry.face_view_proj[1] = proj * look_at(
			pos, pos + vec3<length>(meters(-1.0f), meters(0.0f), meters(0.0f)),
			-axis_y
		);
		entry.face_view_proj[2] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(1.0f), meters(0.0f)),
			axis_z
		);
		entry.face_view_proj[3] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(-1.0f), meters(0.0f)),
			-axis_z
		);
		entry.face_view_proj[4] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(0.0f), meters(1.0f)),
			-axis_y
		);
		entry.face_view_proj[5] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(0.0f), meters(-1.0f)),
			-axis_y
		);

		data.point_lights.push_back(std::move(entry));
		++next_point_shadow_index;
	}

	phase.channels.push(std::move(data));
}

auto gse::renderer::shadow::system::render(render_phase& phase, const state& s) -> void {
    const auto& shadow_items = phase.read_channel<render_data>();
    if (shadow_items.empty()) {
        return;
    }

    const auto& geom_items = phase.read_channel<geometry_collector::render_data>();
    if (geom_items.empty()) {
        return;
    }

    if (!s.ctx->graph().frame_in_progress()) {
        return;
    }

    const auto& lights = shadow_items[0].lights;
    const auto& point_lights = shadow_items[0].point_lights;
    const auto& geom_data = geom_items[0];

    struct shadow_draw_batch {
        std::vector<render_queue_entry> draw_list;
        view_projection_matrix light_view_proj;
        vk::ImageView depth_view;
        vk::Extent2D extent;
    };

    std::vector<shadow_draw_batch> batches;

    for (const auto& [view, proj, ignore_list_ids, shadow_index] : lights) {
        if (shadow_index < 0 || static_cast<std::size_t>(shadow_index) >= s.shadow_maps.size()) {
            continue;
        }

        auto draw_list = geometry_collector::filter_render_queue(geom_data, ignore_list_ids);
        if (draw_list.empty()) {
            continue;
        }

        batches.push_back({
            .draw_list = std::move(draw_list),
            .light_view_proj = proj * view,
            .depth_view = s.shadow_maps[static_cast<std::size_t>(shadow_index)].view,
            .extent = { s.shadow_extent.x(), s.shadow_extent.y() }
        });
    }

    for (const auto& pl : point_lights) {
        if (pl.point_shadow_index < 0 || static_cast<std::size_t>(pl.point_shadow_index) >= s.point_shadow_cubemaps.size()) {
            continue;
        }

        auto draw_list = geometry_collector::filter_render_queue(geom_data, pl.ignore_list_ids);
        if (draw_list.empty()) {
            continue;
        }

        const auto cubemap_index = static_cast<std::size_t>(pl.point_shadow_index);

        for (std::size_t face = 0; face < 6; ++face) {
            batches.push_back({
                .draw_list = draw_list,
                .light_view_proj = pl.face_view_proj[face],
                .depth_view = s.point_shadow_face_view(cubemap_index, face),
                .extent = { s.point_shadow_extent.x(), s.point_shadow_extent.y() }
            });
        }
    }

    if (batches.empty()) {
        return;
    }

    s.ctx->graph()
        .add_pass<state>()
        .record([&s, batches = std::move(batches)](vulkan::recording_context& ctx) {
            for (const auto& batch : batches) {
                vk::RenderingAttachmentInfo depth_attachment{
                    .imageView = batch.depth_view,
                    .imageLayout = vk::ImageLayout::eGeneral,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .clearValue = vk::ClearValue{
                        .depthStencil = { 1.0f, 0 }
                    }
                };

                const vk::RenderingInfo rendering_info{
                    .renderArea = { { 0, 0 }, batch.extent },
                    .layerCount = 1,
                    .colorAttachmentCount = 0,
                    .pColorAttachments = nullptr,
                    .pDepthAttachment = &depth_attachment
                };

                ctx.begin_rendering(rendering_info);

                ctx.bind_pipeline(vk::PipelineBindPoint::eGraphics, s.pipeline);

                ctx.set_viewport(0.0f, 0.0f, static_cast<float>(batch.extent.width), static_cast<float>(batch.extent.height));
                ctx.set_scissor(0, 0, batch.extent.width, batch.extent.height);

                for (const auto& e : batch.draw_list) {
                    auto pc = s.shader_handle->cache_push_block("push_constants");
                    pc.set("light_view_proj", batch.light_view_proj);
                    pc.set("model", e.model_matrix);
                    ctx.push(pc, s.pipeline_layout);

                    const auto& mesh = e.model->meshes()[e.index];
                    const vk::Buffer vbufs[]{ mesh.vertex_buffer() };
                    const vk::DeviceSize voffs[]{ 0 };
                    ctx.bind_vertex_buffers(0, vbufs, voffs);
                    ctx.bind_index_buffer(mesh.index_buffer(), 0, vk::IndexType::eUint32);
                    ctx.draw_indexed(static_cast<std::uint32_t>(mesh.indices().size()));
                }

                ctx.end_rendering();
            }
        });
}

auto gse::renderer::ensure_non_collinear_up(const vec3f& direction, const vec3f& up) -> vec3f {
	auto normalized_direction = normalize(direction);
	auto normalized_up = normalize(up);
	if (const float dot_product = dot(normalized_direction, normalized_up); std::abs(dot_product) > 1.0f - std::numeric_limits<float>::epsilon()) {
		if (std::abs(normalized_direction.y()) > 0.9f) {
			normalized_up = axis_z;
		} else {
			normalized_up = axis_y;
		}
	}
	return normalized_up;
}
