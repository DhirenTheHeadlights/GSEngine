export module gse.graphics:shadow_renderer;

import std;

import :render_component;
import :mesh;
import :model;
import :shader;
import :spot_light;
import :directional_light;
import :point_light;
import :cube_map;
import :geometry_renderer;

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

	struct point_shadow_light_entry {
		vec3<length> world_position;
		std::array<mat4, 6> face_view_proj;
		length near_plane;
		length far_plane;
		std::vector<id> ignore_list_ids;
		int point_shadow_index = -1;
	};

	constexpr std::size_t max_shadow_lights = 10;
	constexpr std::size_t max_point_shadow_lights = 4;

	auto ensure_non_collinear_up(
		const unitless::vec3& direction,
		const unitless::vec3& up
	) -> unitless::vec3;
}

export namespace gse::renderer::shadow {
	struct render_data {
		std::vector<shadow_light_entry> lights;
		std::vector<point_shadow_light_entry> point_lights;
	};

	struct state {
		context* ctx = nullptr;

		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;

		resource::handle<shader> shader_handle;

		unitless::vec2u shadow_extent = { 1024, 1024 };
		unitless::vec2u point_shadow_extent = { 512, 512 };

		std::array<vulkan::image_resource, max_shadow_lights> shadow_maps;
		std::array<cube_map, max_point_shadow_lights> point_shadow_cubemaps;

		explicit state(context& c) : ctx(std::addressof(c)) {}
		state() = default;

		auto shadow_map_view(std::size_t index) const -> vk::ImageView;
		auto point_shadow_cube_view(std::size_t index) const -> vk::ImageView;
		auto point_shadow_face_view(std::size_t index, std::size_t face) const -> vk::ImageView;
		auto point_shadow_sampler(std::size_t index) const -> vk::Sampler;
		auto shadow_texel_size() const -> unitless::vec2;
	};

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

auto gse::renderer::shadow::state::shadow_texel_size() const -> unitless::vec2 {
	return unitless::vec2(
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

		auto up = ensure_non_collinear_up(dir, { 0.0f, 1.0f, 0.0f });

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
		entry.world_position = comp.position;
		entry.near_plane = comp.near_plane;
		entry.far_plane = comp.far_plane;

		entry.ignore_list_ids = comp.ignore_list_ids;

		const auto proj = perspective(
			degrees(90.0f),
			1.0f,
			comp.near_plane,
			comp.far_plane
		);

		const auto pos = comp.position;

		entry.face_view_proj[0] = proj * look_at(
			pos, pos + vec3<length>(meters(1.0f), meters(0.0f), meters(0.0f)),
			unitless::vec3(0.0f, -1.0f, 0.0f)
		);
		entry.face_view_proj[1] = proj * look_at(
			pos, pos + vec3<length>(meters(-1.0f), meters(0.0f), meters(0.0f)),
			unitless::vec3(0.0f, -1.0f, 0.0f)
		);
		entry.face_view_proj[2] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(1.0f), meters(0.0f)),
			unitless::vec3(0.0f, 0.0f, 1.0f)
		);
		entry.face_view_proj[3] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(-1.0f), meters(0.0f)),
			unitless::vec3(0.0f, 0.0f, -1.0f)
		);
		entry.face_view_proj[4] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(0.0f), meters(1.0f)),
			unitless::vec3(0.0f, -1.0f, 0.0f)
		);
		entry.face_view_proj[5] = proj * look_at(
			pos, pos + vec3<length>(meters(0.0f), meters(0.0f), meters(-1.0f)),
			unitless::vec3(0.0f, -1.0f, 0.0f)
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

    const auto& geom_items = phase.read_channel<geometry::render_data>();
    if (geom_items.empty()) {
        return;
    }

    auto& config = s.ctx->config();

    if (!config.frame_in_progress()) {
        return;
    }

    const auto command = config.frame_context().command_buffer;

    const auto& lights = shadow_items[0].lights;
    const auto& point_lights = shadow_items[0].point_lights;
    const auto& geom_data = geom_items[0];

    for (const auto& [view, proj, ignore_list_ids, shadow_index] : lights) {
        const auto draw_list = geometry::filter_render_queue(geom_data, ignore_list_ids);

        if (draw_list.empty()) {
            continue;
        }
        if (shadow_index < 0 ||
            static_cast<std::size_t>(shadow_index) >= s.shadow_maps.size()) {
            continue;
        }

        const auto& depth_image = s.shadow_maps[static_cast<std::size_t>(shadow_index)];

        constexpr vk::MemoryBarrier2 pre_barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
            .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        };

        const vk::DependencyInfo pre_dep{
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &pre_barrier
        };

        command.pipelineBarrier2(pre_dep);

        vk::RenderingAttachmentInfo depth_attachment{
            .imageView = depth_image.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                .depthStencil = { 1.0f, 0 }
            }
        };

        const vk::RenderingInfo rendering_info{
            .renderArea = { { 0, 0 }, { s.shadow_extent.x(), s.shadow_extent.y() } },
            .layerCount = 1,
            .colorAttachmentCount = 0,
            .pColorAttachments = nullptr,
            .pDepthAttachment = &depth_attachment
        };

        vulkan::render(config, rendering_info, [&] {
            command.bindPipeline(vk::PipelineBindPoint::eGraphics, s.pipeline);

            const vk::Viewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(s.shadow_extent.x()),
                .height = static_cast<float>(s.shadow_extent.y()),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };
            command.setViewport(0, viewport);

            const vk::Rect2D scissor{
                .offset = { 0, 0 },
                .extent = { s.shadow_extent.x(), s.shadow_extent.y() }
            };
            command.setScissor(0, scissor);

            const mat4 light_view_proj = proj * view;

            for (const auto& e : draw_list) {
                s.shader_handle->push(
                    command,
                    s.pipeline_layout,
                    "push_constants",
                    "light_view_proj", light_view_proj,
                    "model", e.model_matrix
                );

                const auto& mesh = e.model->meshes()[e.index];
                mesh.bind(command);
                mesh.draw(command);
            }
        });

        constexpr vk::MemoryBarrier2 post_barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead
        };

        const vk::DependencyInfo post_dep{
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &post_barrier
        };

        command.pipelineBarrier2(post_dep);
    }

    for (const auto& pl : point_lights) {
        if (pl.point_shadow_index < 0 ||
            static_cast<std::size_t>(pl.point_shadow_index) >= s.point_shadow_cubemaps.size()) {
            continue;
        }

        const auto draw_list = geometry::filter_render_queue(geom_data, pl.ignore_list_ids);

        if (draw_list.empty()) {
            continue;
        }

        const auto cubemap_index = static_cast<std::size_t>(pl.point_shadow_index);

        const vk::ImageMemoryBarrier2 cm_pre_barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
            .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = s.point_shadow_cubemaps[cubemap_index].image_resource().image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eDepth,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6
            }
        };

        const vk::DependencyInfo cm_pre_dep{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &cm_pre_barrier
        };

        command.pipelineBarrier2(cm_pre_dep);

        for (std::size_t face = 0; face < 6; ++face) {
            vk::RenderingAttachmentInfo depth_attachment{
                .imageView = s.point_shadow_face_view(cubemap_index, face),
                .imageLayout = vk::ImageLayout::eGeneral,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue{
                    .depthStencil = { 1.0f, 0 }
                }
            };

            const vk::RenderingInfo rendering_info{
                .renderArea = { { 0, 0 }, { s.point_shadow_extent.x(), s.point_shadow_extent.y() } },
                .layerCount = 1,
                .colorAttachmentCount = 0,
                .pColorAttachments = nullptr,
                .pDepthAttachment = &depth_attachment
            };

            vulkan::render(config, rendering_info, [&] {
                command.bindPipeline(vk::PipelineBindPoint::eGraphics, s.pipeline);

                const vk::Viewport viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(s.point_shadow_extent.x()),
                    .height = static_cast<float>(s.point_shadow_extent.y()),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                };
                command.setViewport(0, viewport);

                const vk::Rect2D scissor{
                    .offset = { 0, 0 },
                    .extent = { s.point_shadow_extent.x(), s.point_shadow_extent.y() }
                };
                command.setScissor(0, scissor);

                for (const auto& e : draw_list) {
                    s.shader_handle->push(
                        command,
                        s.pipeline_layout,
                        "push_constants",
                        "light_view_proj", pl.face_view_proj[face],
                        "model", e.model_matrix
                    );

                    const auto& mesh = e.model->meshes()[e.index];
                    mesh.bind(command);
                    mesh.draw(command);
                }
            });
        }

        const vk::ImageMemoryBarrier2 cm_post_barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = s.point_shadow_cubemaps[cubemap_index].image_resource().image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eDepth,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6
            }
        };

        const vk::DependencyInfo cm_post_dep{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &cm_post_barrier
        };

        command.pipelineBarrier2(cm_post_dep);
    }
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
