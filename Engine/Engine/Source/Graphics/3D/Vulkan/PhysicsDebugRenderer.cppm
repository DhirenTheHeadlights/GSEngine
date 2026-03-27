export module gse.graphics:physics_debug_renderer;

import std;

import gse.physics;
import gse.math;
import gse.utility;
import gse.platform;

import :camera_system;

namespace gse::renderer::physics_debug {
	struct debug_vertex {
		vec3<position> position;
		vec3f color;
	};

	auto add_line(
		const vec3<position>& a,
		const vec3<position>& b,
		const vec3f& color,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_obb_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_sphere_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_capsule_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_shape_lines_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component* mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto build_contact_debug_for_collider(
		const physics::collision_component& coll,
		const physics::motion_component& mc,
		std::vector<debug_vertex>& out_vertices
	) -> void;

	auto ensure_vertex_capacity(
		struct state& s,
		std::size_t frame_index,
		std::size_t required_vertex_count
	) -> void;
}

export namespace gse::renderer::physics_debug {
	struct debug_stats {
		std::uint32_t body_count = 0;
		std::uint32_t sleeping_count = 0;
		std::uint32_t contact_count = 0;
		std::uint32_t motor_count = 0;
		std::uint32_t colliding_pairs = 0;
		time_t<float, seconds> solve_time{};
		velocity max_linear_speed{};
		angular_velocity max_angular_speed{};
		length max_penetration{};
		bool gpu_solver_active = false;
	};

	struct render_data {
		std::vector<debug_vertex> vertices;
		debug_stats stats;
	};

	struct state {
		gpu::context* ctx = nullptr;

		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;

		resource::handle<shader> shader_handle;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> ubo_allocations;
		per_frame_resource<vulkan::buffer_resource> vertex_buffers;

		per_frame_resource<std::size_t> max_vertices;
		bool enabled = true;
		debug_stats latest_stats;

		explicit state(gpu::context& c) : ctx(std::addressof(c)) {}
		state() = default;
	};

	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(const update_phase& phase, state& s) -> void;
		static auto render(const render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::physics_debug::ensure_vertex_capacity(state& s, const std::size_t frame_index, const std::size_t required_vertex_count) -> void {
	auto& max_vertices = s.max_vertices[frame_index];
	auto& vertex_buffer = s.vertex_buffers[frame_index];

	if (required_vertex_count <= max_vertices && vertex_buffer.buffer) {
		return;
	}

	auto& config = s.ctx->config();
	if (vertex_buffer.buffer) {
		vertex_buffer = {};
	}

	max_vertices = std::max<std::size_t>(max_vertices, 4096);
	while (max_vertices < required_vertex_count) {
		max_vertices *= 2;
	}

	const vk::BufferCreateInfo buffer_info{
		.size = max_vertices * sizeof(debug_vertex),
		.usage = vk::BufferUsageFlagBits::eVertexBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};

	const std::vector<std::byte> zeros(buffer_info.size);
	vertex_buffer = config.allocator().create_buffer(
		buffer_info,
		zeros.data()
	);
}

auto gse::renderer::physics_debug::system::initialize(const initialize_phase& phase, state& s) -> void {
	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Physics Debug Renderer Enabled",
		.description = "Enable or disable outlines on collision boxes & visible impulse vectors",
		.ref = &s.enabled,
		.type = typeid(bool)
	});

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

auto gse::renderer::physics_debug::add_line(const vec3<position>& a, const vec3<position>& b, const vec3f& color, std::vector<debug_vertex>& out_vertices) -> void {
	out_vertices.push_back(debug_vertex{ a, color });
	out_vertices.push_back(debug_vertex{ b, color });
}

auto gse::renderer::physics_debug::build_obb_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	auto bb = coll.bounding_box;
	if (mc) {
		bb.update(mc->render_position, mc->render_orientation);
	}
	std::array<vec3<position>, 8> corners;

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

	constexpr vec3f color{ 0.0f, 1.0f, 0.0f };

	for (const auto& [fst, snd] : edges) {
		add_line(corners[fst], corners[snd], color, out_vertices);
	}
}

auto gse::renderer::physics_debug::build_sphere_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	auto bb = coll.bounding_box;
	if (mc) {
		bb.update(mc->render_position, mc->render_orientation);
	}

	const auto center = bb.center();
	const auto axes = bb.obb().axes;
	const auto radius = coll.shape_radius;

	constexpr vec3f color{ 0.0f, 1.0f, 0.0f };

	for (int ring = 0; ring < 3; ++ring) {
		constexpr int segments = 32;
		const auto& u = axes[ring];
		const auto& v = axes[(ring + 1) % 3];

		for (int i = 0; i < segments; ++i) {
			const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / segments;
			const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / segments;

			const auto p0 = center + u * (radius * std::cos(a0)) + v * (radius * std::sin(a0));
			const auto p1 = center + u * (radius * std::cos(a1)) + v * (radius * std::sin(a1));

			add_line(p0, p1, color, out_vertices);
		}
	}
}

auto gse::renderer::physics_debug::build_capsule_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	auto bb = coll.bounding_box;
	if (mc) {
		bb.update(mc->render_position, mc->render_orientation);
	}

	const auto center = bb.center();
	const auto axes = bb.obb().axes;
	const auto radius = coll.shape_radius;
	const auto half_height = coll.shape_half_height;

	const auto& cap_axis = axes[1];
	const auto& u = axes[0];
	const auto& v = axes[2];

	const auto top = center + cap_axis * half_height;
	const auto bottom = center - cap_axis * half_height;

	constexpr vec3f color{ 0.0f, 1.0f, 0.0f };
	constexpr int segments = 24;
	constexpr int half_segments = segments / 2;

	for (const auto& ring_center : { top, bottom }) {
		for (int i = 0; i < segments; ++i) {
			const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / segments;
			const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / segments;
			const auto p0 = ring_center + u * (radius * std::cos(a0)) + v * (radius * std::sin(a0));
			const auto p1 = ring_center + u * (radius * std::cos(a1)) + v * (radius * std::sin(a1));
			add_line(p0, p1, color, out_vertices);
		}
	}

	for (int i = 0; i < 4; ++i) {
		const float angle = static_cast<float>(i) * std::numbers::pi_v<float> * 0.5f;
		const auto offset = u * (radius * std::cos(angle)) + v * (radius * std::sin(angle));
		add_line(top + offset, bottom + offset, color, out_vertices);
	}

	for (int plane = 0; plane < 2; ++plane) {
		const auto& ring_u = (plane == 0) ? u : v;

		for (int i = 0; i < half_segments; ++i) {
			const float a0 = std::numbers::pi_v<float> * static_cast<float>(i) / half_segments;
			const float a1 = std::numbers::pi_v<float> * static_cast<float>(i + 1) / half_segments;

			const auto p0 = top + ring_u * (radius * std::cos(a0)) + cap_axis * (radius * std::sin(a0));
			const auto p1 = top + ring_u * (radius * std::cos(a1)) + cap_axis * (radius * std::sin(a1));
			add_line(p0, p1, color, out_vertices);

			const auto q0 = bottom + ring_u * (radius * std::cos(a0)) - cap_axis * (radius * std::sin(a0));
			const auto q1 = bottom + ring_u * (radius * std::cos(a1)) - cap_axis * (radius * std::sin(a1));
			add_line(q0, q1, color, out_vertices);
		}
	}
}

auto gse::renderer::physics_debug::build_shape_lines_for_collider(const physics::collision_component& coll, const physics::motion_component* mc, std::vector<debug_vertex>& out_vertices) -> void {
	switch (coll.shape) {
	case physics::shape_type::sphere:
		build_sphere_lines_for_collider(coll, mc, out_vertices);
		break;
	case physics::shape_type::capsule:
		build_capsule_lines_for_collider(coll, mc, out_vertices);
		break;
	default:
		build_obb_lines_for_collider(coll, mc, out_vertices);
		break;
	}
}

auto gse::renderer::physics_debug::build_contact_debug_for_collider(const physics::collision_component& coll, const physics::motion_component& mc, std::vector<debug_vertex>& out_vertices) -> void {
	const auto& [colliding, collision_normal, penetration, collision_points] = coll.collision_information;
	if (!colliding || mc.position_locked) {
		return;
	}

	const float t = std::clamp(penetration / meters(0.05f), 0.f, 1.f);
	const vec3f satisfaction_color{ t, 1.f - t, 0.f };

	for (auto& collision_point : collision_points) {
		const auto p = collision_point;
		const auto n = collision_normal;

		constexpr length cross_size = meters(0.05f);
		constexpr vec3f normal_color{ 0.f, 0.7f, 1.f };

		const auto px1 = p + vec3<length>{ cross_size, meters(0.f), meters(0.f) };
		const auto px2 = p - vec3<length>{ cross_size, meters(0.f), meters(0.f) };

		const auto py1 = p + vec3<length>{ meters(0.f), cross_size, meters(0.f) };
		const auto py2 = p - vec3<length>{ meters(0.f), cross_size, meters(0.f) };

		const auto pz1 = p + vec3<length>{ meters(0.f), meters(0.f), cross_size };
		const auto pz2 = p - vec3<length>{ meters(0.f), meters(0.f), cross_size };

		add_line(px1, px2, satisfaction_color, out_vertices);
		add_line(py1, py2, satisfaction_color, out_vertices);
		add_line(pz1, pz2, satisfaction_color, out_vertices);

		constexpr length min_normal_len = meters(0.15f);
		const length normal_len = std::max<length>(std::min<length>(penetration, meters(0.5f)), min_normal_len);
		const auto normal_end = p + n * normal_len;
		add_line(p, normal_end, normal_color, out_vertices);
	}
}

auto gse::renderer::physics_debug::system::update(const update_phase& phase, state& s) -> void {
	if (!s.enabled) {
		return;
	}

	std::vector<debug_vertex> vertices;
	debug_stats stats;

	for (const auto& mc : phase.registry.view<physics::motion_component>()) {
		stats.body_count++;
		if (mc.sleeping) stats.sleeping_count++;

		const auto lin = magnitude(mc.current_velocity);
		const auto ang = magnitude(mc.angular_velocity);
		if (lin > stats.max_linear_speed) stats.max_linear_speed = lin;
		if (ang > stats.max_angular_speed) stats.max_angular_speed = ang;
	}

	for (const auto& coll : phase.registry.view<physics::collision_component>()) {
		if (!coll.resolve_collisions) {
			continue;
		}

		const auto* mc = phase.registry.try_read<physics::motion_component>(coll.owner_id());
		build_shape_lines_for_collider(coll, mc, vertices);

		if (mc) {
			build_contact_debug_for_collider(coll, *mc, vertices);
		}

		if (coll.collision_information.colliding) {
			stats.colliding_pairs++;
			if (coll.collision_information.penetration > stats.max_penetration)
				stats.max_penetration = coll.collision_information.penetration;
		}
	}

	if (const auto* ps = phase.try_state_of<physics::state>()) {
		if (ps->use_gpu_solver && ps->gpu_solver.compute_initialized()) {
			stats.gpu_solver_active = true;
			stats.contact_count = ps->gpu_solver.contact_count();
			stats.motor_count = ps->gpu_solver.motor_count();
			stats.solve_time = ps->gpu_solver.solve_time();
		}
	}

	s.latest_stats = stats;
	phase.channels.push(render_data{ std::move(vertices), stats });
}

auto gse::renderer::physics_debug::system::render(const render_phase& phase, const state& s) -> void {
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
	const auto frame_index = config.current_frame();
	ensure_vertex_capacity(mutable_state, frame_index, verts.size());

	auto& vertex_buffer = mutable_state.vertex_buffers[frame_index];
	if (auto* dst = vertex_buffer.allocation.mapped()) {
		gse::memcpy(dst, verts);
	}

	const auto command = config.frame_context().command_buffer;

	const auto* cam_state = phase.try_state_of<camera::state>();
	const mat4f view_matrix = cam_state ? cam_state->view_matrix : mat4f(1.0f);
	const mat4f proj_matrix = cam_state ? cam_state->projection_matrix : mat4f(1.0f);

	s.shader_handle->set_uniform("CameraUBO.view", view_matrix, s.ubo_allocations.at("CameraUBO")[frame_index].allocation);
	s.shader_handle->set_uniform("CameraUBO.proj", proj_matrix, s.ubo_allocations.at("CameraUBO")[frame_index].allocation);

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

		const vk::Buffer vb = vertex_buffer.buffer;
		constexpr vk::DeviceSize offsets[] = { 0 };

		command.bindVertexBuffers(0, 1, &vb, offsets);
		command.draw(static_cast<std::uint32_t>(verts.size()), 1, 0, 0);
	});
}
