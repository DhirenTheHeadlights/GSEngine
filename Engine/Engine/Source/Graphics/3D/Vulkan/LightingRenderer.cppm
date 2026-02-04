export module gse.graphics:lighting_renderer;

import std;

import :point_light;
import :spot_light;
import :directional_light;
import :shader;
import :shadow_renderer;

import gse.utility;

namespace gse::renderer::lighting {
	auto update_gbuffer_descriptors(struct state& s) -> void;
}

export namespace gse::renderer::lighting {
	struct state {
		context* ctx = nullptr;

		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;
		vk::raii::Sampler buffer_sampler = nullptr;
		vk::raii::Sampler shadow_sampler = nullptr;

		resource::handle<shader> shader_handle;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> ubo_allocations;
		per_frame_resource<vulkan::buffer_resource> light_buffers;

		explicit state(context& c) : ctx(std::addressof(c)) {}
		state() = default;
	};

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::lighting::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& config = s.ctx->config();
	s.shader_handle = s.ctx->get<shader>("Shaders/Deferred3D/lighting_pass");
	s.ctx->instantly_load(s.shader_handle);

	auto lighting_layouts = s.shader_handle->layouts();

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.descriptor_sets[i] = s.shader_handle->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);
	}

	const auto cam_block = s.shader_handle->uniform_block("CameraParams");
	const auto light_block = s.shader_handle->uniform_block("lights_ssbo");
	const auto shadow_block = s.shader_handle->uniform_block("ShadowParams");

	vk::BufferCreateInfo cam_buffer_info{
		.size = cam_block.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	vk::BufferCreateInfo light_buffer_info{
		.size = light_block.size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	vk::BufferCreateInfo shadow_buffer_info{
		.size = shadow_block.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraParams"][i] = config.allocator().create_buffer(cam_buffer_info);
		s.ubo_allocations["ShadowParams"][i] = config.allocator().create_buffer(shadow_buffer_info);
		s.light_buffers[i] = config.allocator().create_buffer(light_buffer_info);
	}

	constexpr vk::SamplerCreateInfo sampler_create_info{
		.magFilter = vk::Filter::eNearest,
		.minFilter = vk::Filter::eNearest,
		.mipmapMode = vk::SamplerMipmapMode::eNearest,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
		.addressModeW = vk::SamplerAddressMode::eClampToEdge,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::False,
		.maxAnisotropy = 1.0f,
		.compareEnable = vk::False,
		.minLod = 0.0f,
		.maxLod = 1.0f,
		.borderColor = vk::BorderColor::eFloatOpaqueWhite
	};
	s.buffer_sampler = config.device_config().device.createSampler(sampler_create_info);

	constexpr vk::SamplerCreateInfo shadow_sampler_create_info{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eNearest,
		.addressModeU = vk::SamplerAddressMode::eClampToBorder,
		.addressModeV = vk::SamplerAddressMode::eClampToBorder,
		.addressModeW = vk::SamplerAddressMode::eClampToBorder,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::False,
		.maxAnisotropy = 1.0f,
		.compareEnable = vk::False,
		.minLod = 0.0f,
		.maxLod = 1.0f,
		.borderColor = vk::BorderColor::eFloatOpaqueWhite
	};

	s.shadow_sampler = config.device_config().device.createSampler(shadow_sampler_create_info);

	update_gbuffer_descriptors(s);
	config.on_swap_chain_recreate([&s](vulkan::config&) {
		update_gbuffer_descriptors(s);
	});

	const auto* shadow_state = phase.try_state_of<shadow::state>();

	std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>> array_image_infos;
	std::vector<vk::DescriptorImageInfo> shadow_infos;
	shadow_infos.reserve(max_shadow_lights);

	for (std::size_t i = 0; i < max_shadow_lights; ++i) {
		shadow_infos.push_back({
			.sampler = s.shadow_sampler,
			.imageView = shadow_state ? shadow_state->shadow_map_view(i) : vk::ImageView{},
			.imageLayout = vk::ImageLayout::eGeneral
		});
	}

	array_image_infos.emplace("shadow_maps", std::move(shadow_infos));

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		const std::unordered_map<std::string, vk::DescriptorBufferInfo> lighting_buffer_infos = {
			{
				"CameraParams",
				{
					.buffer = s.ubo_allocations["CameraParams"][i].buffer,
					.offset = 0,
					.range = cam_block.size
				}
			},
			{
				"lights_ssbo",
				{
					.buffer = s.light_buffers[i].buffer,
					.offset = 0,
					.range = light_block.size
				}
			},
			{
				"ShadowParams",
				{
					.buffer = s.ubo_allocations["ShadowParams"][i].buffer,
					.offset = 0,
					.range = shadow_block.size
				}
			}
		};

		auto buffer_and_shadow_writes = s.shader_handle->descriptor_writes(
			**s.descriptor_sets[i],
			lighting_buffer_infos,
			{},
			array_image_infos
		);

		config.device_config().device.updateDescriptorSets(buffer_and_shadow_writes, nullptr);
	}

	const auto range = s.shader_handle->push_constant_range("push_constants");

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(lighting_layouts.size()),
		.pSetLayouts = lighting_layouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &range
	};
	s.pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	auto lighting_stages = s.shader_handle->shader_stages();

	constexpr vk::PipelineVertexInputStateCreateInfo empty_vertex_input{
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr
	};

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
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
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eFront,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::True,
		.depthBiasConstantFactor = 1.25f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 1.75f,
		.lineWidth = 1.0f
	};

	constexpr vk::PipelineMultisampleStateCreateInfo multi_sample_state{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	constexpr vk::PipelineColorBlendAttachmentState color_blend_attachment{
		.blendEnable = vk::False,
		.srcColorBlendFactor = vk::BlendFactor::eOne,
		.dstColorBlendFactor = vk::BlendFactor::eZero,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};

	const vk::PipelineColorBlendStateCreateInfo color_blend_state{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
		.blendConstants = std::array{ 0.0f, 0.0f, 0.0f, 0.0f }
	};

	const auto color_format = config.swap_chain_config().surface_format.format;
	const vk::PipelineRenderingCreateInfoKHR lighting_rendering_info{
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color_format
	};

	const vk::GraphicsPipelineCreateInfo lighting_pipeline_info{
		.pNext = &lighting_rendering_info,
		.stageCount = static_cast<std::uint32_t>(lighting_stages.size()),
		.pStages = lighting_stages.data(),
		.pVertexInputState = &empty_vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multi_sample_state,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &color_blend_state,
		.pDynamicState = &dynamic_state,
		.layout = s.pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};
	s.pipeline = config.device_config().device.createGraphicsPipeline(nullptr, lighting_pipeline_info);
}

auto gse::renderer::lighting::system::render(render_phase& phase, const state& s) -> void {
	auto& config = s.ctx->config();

	if (!config.frame_in_progress()) {
		return;
	}

	auto dir_chunk = phase.registry.view<directional_light_component>();
	auto spot_chunk = phase.registry.view<spot_light_component>();
	auto point_chunk = phase.registry.view<point_light_component>();

	const auto command = config.frame_context().command_buffer;

	const auto frame_index = config.current_frame();

	auto proj = s.ctx->camera().projection(s.ctx->window().viewport());
	auto inv_proj = proj.inverse();
	auto view = s.ctx->camera().view();
	auto inv_view = view.inverse();

	const auto& cam_alloc = s.ubo_allocations.at("CameraParams")[frame_index].allocation;
	const auto& light_alloc = s.light_buffers[frame_index].allocation;
	const auto& shadow_alloc = s.ubo_allocations.at("ShadowParams")[frame_index].allocation;

	s.shader_handle->set_uniform("CameraParams.inv_proj", inv_proj, cam_alloc);
	s.shader_handle->set_uniform("CameraParams.inv_view", inv_view, cam_alloc);

	const auto light_block = s.shader_handle->uniform_block("lights_ssbo");
	const auto stride = light_block.size;

	std::vector zero_elem(stride, std::byte{ 0 });

	auto zero_at = [&](const std::size_t index) {
		s.shader_handle->set_ssbo_struct(
			"lights_ssbo",
			index,
			std::span<const std::byte>(zero_elem.data(), zero_elem.size()),
			light_alloc
		);
	};

	auto set = [&](const std::size_t index, const std::string_view member, auto const& v) {
		s.shader_handle->set_ssbo_element(
			"lights_ssbo",
			index,
			member,
			std::as_bytes(std::span(std::addressof(v), 1)),
			light_alloc
		);
	};

	auto to_view_pos = [&](const vec3<length>& world_pos) {
		const auto p = vec4<length>(world_pos, meters(1.0f));
		auto vp = view * p;
		return vec3<length>(vp.x(), vp.y(), vp.z());
	};

	auto to_view_dir = [&](const unitless::vec3& world_dir) {
		const auto d4 = unitless::vec4(world_dir, 0.0f);
		const auto vd = view * d4;
		return unitless::vec3(vd.x(), vd.y(), vd.z());
	};

	std::size_t light_count = 0;

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_shadow_lights) {
			break;
		}

		zero_at(light_count);

		int type = 0;
		set(light_count, "light_type", type);
		set(light_count, "direction", to_view_dir(comp.direction));
		set(light_count, "color", comp.color);
		set(light_count, "intensity", comp.intensity);
		set(light_count, "ambient_strength", comp.ambient_strength);

		++light_count;
	}

	for (const auto& comp : spot_chunk) {
		if (light_count >= max_shadow_lights) {
			break;
		}

		zero_at(light_count);

		int type = 2;
		auto pos_meters = comp.position.as<meters>();
		float cut_off_cos = std::cos(comp.cut_off.as<radians>());
		float outer_cut_off_cos = std::cos(comp.outer_cut_off.as<radians>());

		set(light_count, "light_type", type);
		set(light_count, "position", to_view_pos(pos_meters));
		set(light_count, "direction", to_view_dir(comp.direction));
		set(light_count, "color", comp.color);
		set(light_count, "intensity", comp.intensity);
		set(light_count, "constant", comp.constant);
		set(light_count, "linear", comp.linear);
		set(light_count, "quadratic", comp.quadratic);
		set(light_count, "cut_off", cut_off_cos);
		set(light_count, "outer_cut_off", outer_cut_off_cos);
		set(light_count, "ambient_strength", comp.ambient_strength);

		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_shadow_lights) {
			break;
		}

		zero_at(light_count);

		int type = 1;
		auto pos_meters = comp.position.as<meters>();

		set(light_count, "light_type", type);
		set(light_count, "position", to_view_pos(pos_meters));
		set(light_count, "color", comp.color);
		set(light_count, "intensity", comp.intensity);
		set(light_count, "constant", comp.constant);
		set(light_count, "linear", comp.linear);
		set(light_count, "quadratic", comp.quadratic);
		set(light_count, "ambient_strength", comp.ambient_strength);

		++light_count;
	}

	const auto* shadow_state = phase.try_state_of<shadow::state>();
	unitless::vec2 texel_size{};
	std::span<const shadow_light_entry> shadow_entries{};
	if (shadow_state) {
		texel_size = shadow_state->shadow_texel_size();
		shadow_entries = shadow_state->shadow_lights();
	}

	std::array<int, max_shadow_lights> shadow_indices{};
	std::array<mat4, max_shadow_lights> shadow_view_proj{};
	const std::size_t shadow_light_count =
		std::min<std::size_t>(shadow_entries.size(), max_shadow_lights);

	for (std::size_t idx = 0; idx < shadow_light_count; ++idx) {
		shadow_indices[idx] = static_cast<int>(idx);
		shadow_view_proj[idx] = shadow_entries[idx].proj * shadow_entries[idx].view;
	}

	int shadow_count_i = static_cast<int>(shadow_light_count);

	std::unordered_map<std::string, std::span<const std::byte>> shadow_data;
	shadow_data.emplace("shadow_light_count", std::as_bytes(std::span(std::addressof(shadow_count_i), 1)));
	shadow_data.emplace("shadow_light_indices", std::as_bytes(std::span(shadow_indices.data(), shadow_indices.size())));
	shadow_data.emplace("shadow_view_proj", std::as_bytes(std::span(shadow_view_proj.data(), shadow_view_proj.size())));
	shadow_data.emplace("shadow_texel_size", std::as_bytes(std::span(std::addressof(texel_size), 1)));

	s.shader_handle->set_uniform_block("ShadowParams", shadow_data, shadow_alloc);

	vk::RenderingAttachmentInfo color_attachment{
		.imageView = *config.swap_chain_config().image_views[config.frame_context().image_index],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{
			.color = vk::ClearColorValue{
				.float32 = std::array{ 0.1f, 0.1f, 0.1f, 1.0f }
			}
		}
	};

	const vk::RenderingInfo lighting_rendering_info{
		.renderArea = { { 0, 0 }, config.swap_chain_config().extent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr
	};

	vulkan::render(
		config,
		lighting_rendering_info,
		[&] {
			command.bindPipeline(
				vk::PipelineBindPoint::eGraphics,
				s.pipeline
			);

			const vk::Viewport viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = static_cast<float>(config.swap_chain_config().extent.width),
				.height = static_cast<float>(config.swap_chain_config().extent.height),
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			command.setViewport(0, viewport);

			const vk::Rect2D scissor{
				.offset = { 0, 0 },
				.extent = config.swap_chain_config().extent
			};
			command.setScissor(0, scissor);

			command.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				s.pipeline_layout,
				0,
				1,
				&**s.descriptor_sets[config.current_frame()],
				0,
				nullptr
			);

			s.shader_handle->push_bytes(
				command,
				s.pipeline_layout,
				"push_constants",
				std::addressof(light_count),
				0
			);

			command.draw(3, 1, 0, 0);
		}
	);
}

auto gse::renderer::lighting::update_gbuffer_descriptors(state& s) -> void {
	auto& config = s.ctx->config();

	const std::unordered_map<std::string, vk::DescriptorImageInfo> gbuffer_image_infos = {
		{
			"g_albedo",
			{
				.sampler = s.buffer_sampler,
				.imageView = config.swap_chain_config().albedo_image.view,
				.imageLayout = vk::ImageLayout::eGeneral
			}
		},
		{
			"g_normal",
			{
				.sampler = s.buffer_sampler,
				.imageView = config.swap_chain_config().normal_image.view,
				.imageLayout = vk::ImageLayout::eGeneral
			}
		},
		{
			"g_depth",
			{
				.sampler = s.buffer_sampler,
				.imageView = config.swap_chain_config().depth_image.view,
				.imageLayout = vk::ImageLayout::eGeneral
			}
		}
	};

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		const auto writes = s.shader_handle->descriptor_writes(
			*s.descriptor_sets[i],
			{},
			gbuffer_image_infos,
			{}
		);

		config.device_config().device.updateDescriptorSets(writes, nullptr);
	}
}