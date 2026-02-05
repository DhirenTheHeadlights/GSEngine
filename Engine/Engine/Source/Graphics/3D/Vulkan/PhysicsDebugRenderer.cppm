export module gse.graphics:physics_debug_renderer;

import std;

import gse.physics;
import gse.physics.math;
import gse.utility;
import gse.platform;

import :camera;
import :shader;

namespace gse::renderer::physics_debug {
	struct debug_vertex {
		unitless::vec3 position;
		unitless::vec3 color;
	};

	auto add_line(
		const vec3<length>& a,
		const vec3<length>& b,
		const unitless::vec3& color,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_obb_lines_for_collider(
		const physics::collision_component& coll,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_contact_debug_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component& mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto ensure_vertex_capacity(
		struct state& s,
		std::size_t required_vertex_count
	) -> void;
}

export namespace gse::renderer::physics_debug {
	struct render_data {
		std::vector<debug_vertex> vertices;
	};

	struct state {
		context* ctx = nullptr;

		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;

		resource::handle<shader> shader_handle;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> ubo_allocations;
		vulkan::buffer_resource vertex_buffer;

		std::size_t max_vertices = 0;
		bool enabled = true;

		explicit state(context& c) : ctx(std::addressof(c)) {}
		state() = default;
	};

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::physics_debug::ensure_vertex_capacity(state& s, const std::size_t required_vertex_count) -> void {
	if (required_vertex_count <= s.max_vertices && s.vertex_buffer.buffer) {
		return;
	}

	auto& config = s.ctx->config();
	if (s.vertex_buffer.buffer) {
		s.vertex_buffer = {};
	}

	s.max_vertices = std::max<std::size_t>(required_vertex_count, 4096);

	const vk::BufferCreateInfo buffer_info{
		.size = s.max_vertices * sizeof(debug_vertex),
		.usage = vk::BufferUsageFlagBits::eVertexBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	const std::vector<std::byte> zeros(buffer_info.size);
	s.vertex_buffer = config.allocator().create_buffer(
		buffer_info,
		zeros.data()
	);
}

auto gse::renderer::physics_debug::system::initialize(initialize_phase& phase, state& s) -> void {
	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Physics Debug Renderer Enabled",
		.description = "Enable or disable outlines on collision boxes & visible impulse vectors",
		.ref = &s.enabled,
		.type = typeid(bool)
	});

	if (const auto* save_state = phase.try_state_of<save::state>()) {
		s.enabled = save_state->read("Graphics", "Physics Debug Renderer Enabled", true);
	}

	auto& config = s.ctx->config();

	s.shader_handle = s.ctx->get<shader>("Shaders/Standard3D/physics_debug");
	s.ctx->instantly_load(s.shader_handle);
	auto descriptor_set_layouts = s.shader_handle->layouts();

	const auto camera_ubo = s.shader_handle->uniform_block("CameraUBO");

	vk::BufferCreateInfo camera_ubo_buffer_info{
		.size = camera_ubo.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = config.allocator().create_buffer(camera_ubo_buffer_info);

		s.descriptor_sets[i] = s.shader_handle->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

		config.device_config().device.updateDescriptorSets(
			s.shader_handle->descriptor_writes(*s.descriptor_sets[i], buffer_infos, image_infos),
			nullptr
		);
	}

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
		.pSetLayouts = descriptor_set_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	s.pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	auto shader_stages = s.shader_handle->shader_stages();
	auto vertex_input_info = s.shader_handle->vertex_input_state();

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
		.pDynamicState = &dynamic_state,
		.layout = s.pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};

	s.pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);
}

auto gse::renderer::physics_debug::add_line(const vec3<length>& a, const vec3<length>& b, const unitless::vec3& color, std::vector<debug_vertex>& out_vertices) -> void {
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
	const auto& [colliding, collision_normal, penetration, collision_points] = coll.collision_information;
	if (!colliding || mc.position_locked) {
		return;
	}

	for (auto& collision_point : collision_points) {
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
}

auto gse::renderer::physics_debug::system::update(update_phase& phase, state& s) -> void {
	if (!s.enabled) {
		return;
	}

	std::vector<debug_vertex> vertices;

	for (const auto& coll : phase.registry.view<physics::collision_component>()) {
		if (!coll.resolve_collisions) {
			continue;
		}

		build_obb_lines_for_collider(coll, vertices);

		if (const auto* mc = phase.registry.try_read<physics::motion_component>(coll.owner_id())) {
			build_contact_debug_for_collider(coll, *mc, vertices);
		}
	}

	if (!vertices.empty()) {
		phase.channels.push(render_data{ std::move(vertices) });
	}
}

auto gse::renderer::physics_debug::system::render(render_phase& phase, const state& s) -> void {
	if (!s.enabled) {
		return;
	}

	auto& config = s.ctx->config();

	if (!config.frame_in_progress()) {
		return;
	}

	const auto& render_items = phase.read_channel<render_data>();
	if (render_items.empty()) {
		return;
	}

	const auto& verts = render_items[0].vertices;
	if (verts.empty()) {
		return;
	}

	auto& mutable_state = const_cast<state&>(s);
	ensure_vertex_capacity(mutable_state, verts.size());

	const auto byte_count = verts.size() * sizeof(debug_vertex);
	if (void* dst = mutable_state.vertex_buffer.allocation.mapped()) {
		std::memcpy(dst, verts.data(), byte_count);
	}

	const auto command = config.frame_context().command_buffer;
	const auto frame_index = config.current_frame();

	s.shader_handle->set_uniform("CameraUBO.view", s.ctx->camera().view(), s.ubo_allocations.at("CameraUBO")[frame_index].allocation);
	s.shader_handle->set_uniform("CameraUBO.proj", s.ctx->camera().projection(s.ctx->window().viewport()), s.ubo_allocations.at("CameraUBO")[frame_index].allocation);

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
		command.bindPipeline(vk::PipelineBindPoint::eGraphics, s.pipeline);

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

		const vk::DescriptorSet sets[] = {
			**s.descriptor_sets[frame_index]
		};

		command.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			s.pipeline_layout,
			0,
			vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
			{}
		);

		const vk::Buffer vb = mutable_state.vertex_buffer.buffer;
		constexpr vk::DeviceSize offsets[] = { 0 };

		command.bindVertexBuffers(0, 1, &vb, offsets);
		command.draw(static_cast<std::uint32_t>(verts.size()), 1, 0, 0);
	});
}
