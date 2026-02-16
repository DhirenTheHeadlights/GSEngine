export module gse.physics:vbd_gpu_solver;

import std;

import gse.utility;
import gse.log;
import gse.platform;

import :vbd_constraints;
import :vbd_constraint_graph;
import :vbd_solver;

export namespace gse::vbd {
	constexpr std::uint32_t max_bodies = 500;
	constexpr std::uint32_t max_contacts = 2000;
	constexpr std::uint32_t max_motors = 16;
	constexpr std::uint32_t max_colors = 32;
	constexpr std::uint32_t workgroup_size = 64;

	struct gpu_body {
		float position[4];
		float predicted_position[4];
		float inertia_target[4];
		float old_position[4];
		float velocity[4];
		float predicted_velocity[4];
		float orientation[4];
		float predicted_orientation[4];
		float angular_inertia_target[4];
		float old_orientation[4];
		float angular_velocity[4];
		float predicted_angular_velocity[4];
		float motor_target[4];
		float mass;
		std::uint32_t flags;
		std::uint32_t sleep_counter;
		float _pad0;
		float inv_inertia_col0[4];
		float inv_inertia_col1[4];
		float inv_inertia_col2[4];
	};
	static_assert(sizeof(gpu_body) == 272);

	struct gpu_contact {
		std::uint32_t body_a;
		std::uint32_t body_b;
		float initial_separation;
		float lambda;
		float normal[4];
		float r_a[4];
		float r_b[4];
		float friction_coeff;
		float mass_a;
		float mass_b;
		float _pad0;
	};
	static_assert(sizeof(gpu_contact) == 80);

	struct gpu_motor {
		std::uint32_t body_index;
		float compliance;
		float max_force;
		std::uint32_t horizontal_only;
		float target_velocity[4];
	};
	static_assert(sizeof(gpu_motor) == 32);

	struct gpu_solve_state {
		float gradient[4];
		float angular_gradient[4];
		float hessian_col0[4];
		float hessian_col1[4];
		float hessian_col2[4];
		float angular_hessian_col0[4];
		float angular_hessian_col1[4];
		float angular_hessian_col2[4];
		float hessian_xtheta_col0[4];
		float hessian_xtheta_col1[4];
		float hessian_xtheta_col2[4];
	};
	static_assert(sizeof(gpu_solve_state) == 176);

	constexpr std::uint32_t flag_locked = 1u;
	constexpr std::uint32_t flag_update_orientation = 2u;
	constexpr std::uint32_t flag_affected_by_gravity = 4u;

	class gpu_solver;

	struct gpu_dispatch_info {
		gpu_solver* solver = nullptr;
	};

	struct color_range {
		std::uint32_t offset;
		std::uint32_t count;
	};

	class gpu_solver {
	public:
		auto create_buffers(vulkan::allocator& alloc) -> void;
		auto buffers_created() const -> bool { return m_buffers_created; }

		auto upload(
			std::span<const body_state> bodies,
			const constraint_graph& graph,
			const solver_config& solver_cfg,
			float dt
		) -> void;

		auto commit_upload() -> void;

		auto stage_readback(std::uint32_t frame_index) -> void;

		auto readback(
			std::span<body_state> bodies,
			std::vector<contact_constraint>& contacts
		) -> void;

		auto has_readback_data() const -> bool { return m_staged_valid; }
		auto pending_dispatch() const -> bool { return m_pending_dispatch; }

		auto mark_dispatched(std::uint32_t frame_index) -> void {
			m_readback_info[frame_index % 2] = { m_body_count, m_contact_count };
			m_pending_dispatch = false;
			m_frame_count++;
		}

		auto body_buffer(this auto&& self) -> auto& { return self.m_body_buffer; }
		auto contact_buffer(this auto&& self) -> auto& { return self.m_contact_buffer; }
		auto motor_buffer(this auto&& self) -> auto& { return self.m_motor_buffer; }
		auto color_buffer(this auto&& self) -> auto& { return self.m_color_buffer; }
		auto body_contact_map_buffer(this auto&& self) -> auto& { return self.m_body_contact_map_buffer; }
		auto solve_state_buffer(this auto&& self) -> auto& { return self.m_solve_state_buffer; }
		auto readback_buffers(this auto&& self) -> auto& { return self.m_readback_buffer; }

		auto body_count() const -> std::uint32_t { return m_body_count; }
		auto contact_count() const -> std::uint32_t { return m_contact_count; }
		auto motor_count() const -> std::uint32_t { return m_motor_count; }
		auto color_ranges() const -> std::span<const color_range> { return m_color_ranges; }
		auto motor_only_offset() const -> std::uint32_t { return m_motor_only_offset; }
		auto motor_only_count() const -> std::uint32_t { return m_motor_only_count; }
		auto solver_cfg() const -> const solver_config& { return m_solver_cfg; }
		auto dt() const -> float { return m_dt; }
		auto frame_count() const -> std::uint32_t { return m_frame_count; }

	private:
		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		std::uint32_t m_frame_count = 0;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_contact_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_num_colors = 0;

		solver_config m_solver_cfg;
		float m_dt = 0.f;

		std::vector<color_range> m_color_ranges;
		std::uint32_t m_motor_only_offset = 0;
		std::uint32_t m_motor_only_count = 0;

		vulkan::buffer_resource m_body_buffer;
		vulkan::buffer_resource m_contact_buffer;
		vulkan::buffer_resource m_motor_buffer;
		vulkan::buffer_resource m_color_buffer;
		vulkan::buffer_resource m_body_contact_map_buffer;
		vulkan::buffer_resource m_solve_state_buffer;
		vulkan::buffer_resource m_readback_buffer[2];

		struct readback_frame_info {
			std::uint32_t body_count = 0;
			std::uint32_t contact_count = 0;
		};
		readback_frame_info m_readback_info[2] = {};

		std::vector<gpu_body> m_upload_bodies;
		std::vector<gpu_contact> m_upload_contacts;
		std::vector<gpu_motor> m_upload_motors;
		std::vector<std::uint32_t> m_upload_color_data;
		std::vector<std::uint32_t> m_upload_body_contact_map;

		std::vector<gpu_body> m_staged_bodies;
		std::vector<float> m_staged_lambdas;
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;
		bool m_staged_valid = false;
	};
}

auto gse::vbd::gpu_solver::create_buffers(vulkan::allocator& alloc) -> void {
	constexpr auto usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc;
	constexpr auto readback_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

	m_body_buffer = alloc.create_buffer({
		.size = max_bodies * sizeof(gpu_body),
		.usage = usage | vk::BufferUsageFlagBits::eTransferDst
	});

	m_contact_buffer = alloc.create_buffer({
		.size = max_contacts * sizeof(gpu_contact),
		.usage = usage
	});

	m_motor_buffer = alloc.create_buffer({
		.size = max_motors * sizeof(gpu_motor),
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	const vk::DeviceSize color_buffer_size = max_colors * sizeof(std::uint32_t) * 2 + max_bodies * sizeof(std::uint32_t);
	m_color_buffer = alloc.create_buffer({
		.size = color_buffer_size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	const vk::DeviceSize map_buffer_size =
		max_bodies * sizeof(std::uint32_t) * 2 +
		max_contacts * 2 * sizeof(std::uint32_t) +
		max_bodies * sizeof(std::uint32_t);
	m_body_contact_map_buffer = alloc.create_buffer({
		.size = map_buffer_size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	m_solve_state_buffer = alloc.create_buffer({
		.size = max_bodies * sizeof(gpu_solve_state),
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	for (auto& rb : m_readback_buffer) {
		rb = alloc.create_buffer({
			.size = max_bodies * sizeof(gpu_body) + max_contacts * sizeof(float),
			.usage = readback_usage
		});
	}

	m_buffers_created = true;
	log::println(log::level::info, "VBD GPU solver buffers created");
}

auto gse::vbd::gpu_solver::upload(
	const std::span<const body_state> bodies,
	const constraint_graph& graph,
	const solver_config& solver_cfg,
	const float dt
) -> void {
	m_body_count = static_cast<std::uint32_t>(std::min(bodies.size(), static_cast<std::size_t>(max_bodies)));
	m_contact_count = static_cast<std::uint32_t>(std::min(graph.contact_constraints().size(), static_cast<std::size_t>(max_contacts)));
	m_motor_count = static_cast<std::uint32_t>(std::min(graph.motor_constraints().size(), static_cast<std::size_t>(max_motors)));
	m_num_colors = static_cast<std::uint32_t>(graph.body_colors().size());
	m_solver_cfg = solver_cfg;
	m_dt = dt;

	if (m_body_count == 0) {
		m_pending_dispatch = false;
		return;
	}

	m_upload_bodies.resize(m_body_count);
	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		const auto& b = bodies[i];
		auto& gb = m_upload_bodies[i];

		const auto pos = b.position.as<meters>();
		gb.position[0] = pos.x(); gb.position[1] = pos.y(); gb.position[2] = pos.z(); gb.position[3] = 0.f;

		const auto pp = b.predicted_position.as<meters>();
		gb.predicted_position[0] = pp.x(); gb.predicted_position[1] = pp.y(); gb.predicted_position[2] = pp.z(); gb.predicted_position[3] = 0.f;

		const auto it = b.inertia_target.as<meters>();
		gb.inertia_target[0] = it.x(); gb.inertia_target[1] = it.y(); gb.inertia_target[2] = it.z(); gb.inertia_target[3] = 0.f;

		const auto op = b.old_position.as<meters>();
		gb.old_position[0] = op.x(); gb.old_position[1] = op.y(); gb.old_position[2] = op.z(); gb.old_position[3] = 0.f;

		const auto vel = b.body_velocity.as<meters_per_second>();
		gb.velocity[0] = vel.x(); gb.velocity[1] = vel.y(); gb.velocity[2] = vel.z(); gb.velocity[3] = 0.f;

		const auto pv = b.predicted_velocity.as<meters_per_second>();
		gb.predicted_velocity[0] = pv.x(); gb.predicted_velocity[1] = pv.y(); gb.predicted_velocity[2] = pv.z(); gb.predicted_velocity[3] = 0.f;

		gb.orientation[0] = b.orientation.s(); gb.orientation[1] = b.orientation.x();
		gb.orientation[2] = b.orientation.y(); gb.orientation[3] = b.orientation.z();

		gb.predicted_orientation[0] = b.predicted_orientation.s(); gb.predicted_orientation[1] = b.predicted_orientation.x();
		gb.predicted_orientation[2] = b.predicted_orientation.y(); gb.predicted_orientation[3] = b.predicted_orientation.z();

		gb.angular_inertia_target[0] = b.angular_inertia_target.s(); gb.angular_inertia_target[1] = b.angular_inertia_target.x();
		gb.angular_inertia_target[2] = b.angular_inertia_target.y(); gb.angular_inertia_target[3] = b.angular_inertia_target.z();

		gb.old_orientation[0] = b.old_orientation.s(); gb.old_orientation[1] = b.old_orientation.x();
		gb.old_orientation[2] = b.old_orientation.y(); gb.old_orientation[3] = b.old_orientation.z();

		const auto av = b.body_angular_velocity.as<radians_per_second>();
		gb.angular_velocity[0] = av.x(); gb.angular_velocity[1] = av.y(); gb.angular_velocity[2] = av.z(); gb.angular_velocity[3] = 0.f;

		const auto pav = b.predicted_angular_velocity.as<radians_per_second>();
		gb.predicted_angular_velocity[0] = pav.x(); gb.predicted_angular_velocity[1] = pav.y(); gb.predicted_angular_velocity[2] = pav.z(); gb.predicted_angular_velocity[3] = 0.f;

		const auto mt = b.motor_target.as<meters>();
		gb.motor_target[0] = mt.x(); gb.motor_target[1] = mt.y(); gb.motor_target[2] = mt.z(); gb.motor_target[3] = 0.f;

		gb.mass = b.mass_value.as<kilograms>();

		gb.flags = 0;
		if (b.locked) gb.flags |= flag_locked;
		if (b.update_orientation) gb.flags |= flag_update_orientation;
		if (b.affected_by_gravity) gb.flags |= flag_affected_by_gravity;

		gb.sleep_counter = b.sleep_counter;
		gb._pad0 = 0.f;

		const mat3 inv_i = mat3{ b.inv_inertia.data };
		gb.inv_inertia_col0[0] = inv_i[0][0]; gb.inv_inertia_col0[1] = inv_i[0][1]; gb.inv_inertia_col0[2] = inv_i[0][2]; gb.inv_inertia_col0[3] = 0.f;
		gb.inv_inertia_col1[0] = inv_i[1][0]; gb.inv_inertia_col1[1] = inv_i[1][1]; gb.inv_inertia_col1[2] = inv_i[1][2]; gb.inv_inertia_col1[3] = 0.f;
		gb.inv_inertia_col2[0] = inv_i[2][0]; gb.inv_inertia_col2[1] = inv_i[2][1]; gb.inv_inertia_col2[2] = inv_i[2][2]; gb.inv_inertia_col2[3] = 0.f;
	}

	m_upload_contacts.resize(m_contact_count);
	for (std::uint32_t i = 0; i < m_contact_count; ++i) {
		const auto& c = graph.contact_constraints()[i];
		auto& gc = m_upload_contacts[i];
		gc.body_a = c.body_a;
		gc.body_b = c.body_b;
		gc.initial_separation = c.initial_separation.as<meters>();
		gc.lambda = c.lambda;
		gc.normal[0] = c.normal.x(); gc.normal[1] = c.normal.y(); gc.normal[2] = c.normal.z(); gc.normal[3] = 0.f;
		const auto ra = c.r_a.as<meters>();
		gc.r_a[0] = ra.x(); gc.r_a[1] = ra.y(); gc.r_a[2] = ra.z(); gc.r_a[3] = 0.f;
		const auto rb = c.r_b.as<meters>();
		gc.r_b[0] = rb.x(); gc.r_b[1] = rb.y(); gc.r_b[2] = rb.z(); gc.r_b[3] = 0.f;
		gc.friction_coeff = c.friction_coeff;

		const auto& ba = bodies[c.body_a];
		const auto& bb = bodies[c.body_b];
		gc.mass_a = ba.locked ? 0.f : ba.mass_value.as<kilograms>();
		gc.mass_b = bb.locked ? 0.f : bb.mass_value.as<kilograms>();
		gc._pad0 = 0.f;
	}

	m_upload_motors.resize(m_motor_count);
	for (std::uint32_t i = 0; i < m_motor_count; ++i) {
		const auto& m = graph.motor_constraints()[i];
		auto& gm = m_upload_motors[i];
		gm.body_index = m.body_index;
		gm.compliance = m.compliance;
		gm.max_force = m.max_force.as<newtons>();
		gm.horizontal_only = m.horizontal_only ? 1u : 0u;
		const auto tv = m.target_velocity.as<meters_per_second>();
		gm.target_velocity[0] = tv.x(); gm.target_velocity[1] = tv.y(); gm.target_velocity[2] = tv.z(); gm.target_velocity[3] = 0.f;
	}

	const std::uint32_t color_data_size = max_colors * 2 + max_bodies;
	m_upload_color_data.resize(color_data_size);
	std::memset(m_upload_color_data.data(), 0, color_data_size * sizeof(std::uint32_t));

	auto* color_offsets = m_upload_color_data.data();
	auto* color_counts = m_upload_color_data.data() + max_colors;
	auto* body_indices = m_upload_color_data.data() + max_colors * 2;

	m_color_ranges.clear();
	m_color_ranges.reserve(m_num_colors);

	std::uint32_t idx = 0;
	for (std::uint32_t c = 0; c < m_num_colors; ++c) {
		const auto& bc = graph.body_colors()[c];
		color_offsets[c] = idx;
		color_counts[c] = static_cast<std::uint32_t>(bc.size());
		m_color_ranges.push_back({ idx, static_cast<std::uint32_t>(bc.size()) });
		for (const auto bi : bc) {
			body_indices[idx++] = bi;
		}
	}

	std::unordered_set<std::uint32_t> contacted_bodies;
	for (const auto& bc : graph.body_colors()) {
		for (const auto bi : bc) {
			contacted_bodies.insert(bi);
		}
	}

	m_motor_only_offset = idx;
	m_motor_only_count = 0;
	for (const auto& motor : graph.motor_constraints()) {
		if (!contacted_bodies.contains(motor.body_index)) {
			body_indices[idx++] = motor.body_index;
			m_motor_only_count++;
		}
	}

	const std::uint32_t map_data_size = max_bodies * 2 + max_contacts * 2 + max_bodies;
	m_upload_body_contact_map.resize(map_data_size);

	auto* body_contact_offset = m_upload_body_contact_map.data();
	auto* body_contact_count = m_upload_body_contact_map.data() + max_bodies;
	auto* contact_index_list = m_upload_body_contact_map.data() + max_bodies * 2;
	auto* body_motor_index = m_upload_body_contact_map.data() + max_bodies * 2 + max_contacts * 2;

	std::memset(body_contact_offset, 0, max_bodies * sizeof(std::uint32_t));
	std::memset(body_contact_count, 0, max_bodies * sizeof(std::uint32_t));
	std::memset(body_motor_index, 0xFF, max_bodies * sizeof(std::uint32_t));

	for (std::uint32_t ci = 0; ci < m_contact_count; ++ci) {
		const auto& c = graph.contact_constraints()[ci];
		body_contact_count[c.body_a]++;
		body_contact_count[c.body_b]++;
	}

	std::uint32_t list_offset = 0;
	for (std::uint32_t bi = 0; bi < m_body_count; ++bi) {
		body_contact_offset[bi] = list_offset;
		list_offset += body_contact_count[bi];
	}

	std::vector<std::uint32_t> current_offset(m_body_count, 0);
	for (std::uint32_t ci = 0; ci < m_contact_count; ++ci) {
		const auto& c = graph.contact_constraints()[ci];
		contact_index_list[body_contact_offset[c.body_a] + current_offset[c.body_a]++] = ci;
		contact_index_list[body_contact_offset[c.body_b] + current_offset[c.body_b]++] = ci;
	}

	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		body_motor_index[graph.motor_constraints()[mi].body_index] = mi;
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_pending_dispatch || m_body_count == 0) return;

	std::memcpy(m_body_buffer.allocation.mapped(), m_upload_bodies.data(), m_body_count * sizeof(gpu_body));
	std::memcpy(m_contact_buffer.allocation.mapped(), m_upload_contacts.data(), m_contact_count * sizeof(gpu_contact));
	std::memcpy(m_motor_buffer.allocation.mapped(), m_upload_motors.data(), m_motor_count * sizeof(gpu_motor));
	std::memcpy(m_color_buffer.allocation.mapped(), m_upload_color_data.data(), m_upload_color_data.size() * sizeof(std::uint32_t));
	std::memcpy(m_body_contact_map_buffer.allocation.mapped(), m_upload_body_contact_map.data(), m_upload_body_contact_map.size() * sizeof(std::uint32_t));

	std::uint32_t first_dynamic = std::numeric_limits<std::uint32_t>::max();
	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		if (!(m_upload_bodies[i].flags & flag_locked)) {
			first_dynamic = i;
			break;
		}
	}

	if (first_dynamic < m_body_count) {
		const auto& b = m_upload_bodies[first_dynamic];
		log::println(log::level::debug, "COMMIT fc={} bodies={} contacts={} dyn_bi={} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f}) flags={} sleep={}",
			m_frame_count, m_body_count, m_contact_count, first_dynamic,
			b.position[0], b.position[1], b.position[2],
			b.velocity[0], b.velocity[1], b.velocity[2],
			b.flags, b.sleep_counter);
	}
}

auto gse::vbd::gpu_solver::stage_readback(const std::uint32_t frame_index) -> void {
	if (m_frame_count < 2) return;

	const std::uint32_t read_index = frame_index % 2;
	auto& info = m_readback_info[read_index];

	if (info.body_count == 0) return;

	const auto* rb_bodies = reinterpret_cast<const gpu_body*>(m_readback_buffer[read_index].allocation.mapped());

	m_staged_body_count = info.body_count;
	m_staged_contact_count = info.contact_count;
	m_staged_bodies.assign(rb_bodies, rb_bodies + info.body_count);

	const auto* lambda_data = reinterpret_cast<const float*>(
		m_readback_buffer[read_index].allocation.mapped() + max_bodies * sizeof(gpu_body)
	);
	m_staged_lambdas.assign(lambda_data, lambda_data + info.contact_count);

	info.body_count = 0;
	info.contact_count = 0;

	m_staged_valid = true;

	std::uint32_t first_dynamic = std::numeric_limits<std::uint32_t>::max();
	for (std::uint32_t i = 0; i < m_staged_body_count; ++i) {
		if (!(m_staged_bodies[i].flags & flag_locked)) {
			first_dynamic = i;
			break;
		}
	}

	if (first_dynamic < m_staged_body_count) {
		const auto& b = m_staged_bodies[first_dynamic];
		log::println(log::level::debug, "STAGE  fc={} fi={} ri={} dyn_bi={} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f}) sleep={}",
			m_frame_count, frame_index, read_index, first_dynamic,
			b.position[0], b.position[1], b.position[2],
			b.velocity[0], b.velocity[1], b.velocity[2],
			b.sleep_counter);
	}
}

auto gse::vbd::gpu_solver::readback(
	const std::span<body_state> bodies,
	std::vector<contact_constraint>& contacts
) -> void {
	if (!m_staged_valid) return;

	const std::uint32_t count = std::min(m_staged_body_count, static_cast<std::uint32_t>(bodies.size()));
	for (std::uint32_t i = 0; i < count; ++i) {
		const auto& gb = m_staged_bodies[i];
		auto& b = bodies[i];

		b.position = unitless::vec3(gb.position[0], gb.position[1], gb.position[2]) * meters(1.f);
		b.body_velocity = unitless::vec3(gb.velocity[0], gb.velocity[1], gb.velocity[2]) * meters_per_second(1.f);
		b.orientation = quat(gb.orientation[0], gb.orientation[1], gb.orientation[2], gb.orientation[3]);
		b.body_angular_velocity = unitless::vec3(gb.angular_velocity[0], gb.angular_velocity[1], gb.angular_velocity[2]) * radians_per_second(1.f);
		b.sleep_counter = gb.sleep_counter;
	}

	const std::uint32_t contact_count = std::min(m_staged_contact_count, static_cast<std::uint32_t>(contacts.size()));
	for (std::uint32_t i = 0; i < contact_count; ++i) {
		contacts[i].lambda = m_staged_lambdas[i];
	}

	m_staged_valid = false;

	for (std::uint32_t i = 0; i < count; ++i) {
		if (!bodies[i].locked) {
			const auto p = bodies[i].position.as<meters>();
			const auto v = bodies[i].body_velocity.as<meters_per_second>();
			log::println(log::level::debug, "APPLY  dyn_bi={} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f}) sleep={}",
				i, p.x(), p.y(), p.z(), v.x(), v.y(), v.z(), bodies[i].sleep_counter);
			break;
		}
	}
}
