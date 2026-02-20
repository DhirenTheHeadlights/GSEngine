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
	constexpr std::uint32_t max_collision_pairs = 8192;
	constexpr std::uint32_t workgroup_size = 64;

	constexpr std::uint32_t solve_state_float4s_per_body = 11;
	constexpr std::uint32_t collision_state_uints = 8;

	struct buffer_layout {
		std::uint32_t stride = 0;
		std::unordered_map<std::string, std::uint32_t> offsets;
	};

	constexpr std::uint32_t flag_locked = 1u;
	constexpr std::uint32_t flag_update_orientation = 2u;
	constexpr std::uint32_t flag_affected_by_gravity = 4u;

	struct color_range {
		std::uint32_t offset;
		std::uint32_t count;
	};

	struct collision_body_data {
		unitless::vec3 half_extents;
		unitless::vec3 aabb_min;
		unitless::vec3 aabb_max;
	};

	struct warm_start_entry {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint32_t feature_packed;
		float lambda;
	};

	struct contact_readback_entry {
		std::uint32_t body_a;
		std::uint32_t body_b;
		float lambda;
		unitless::vec3 normal;
		float separation;
		std::uint32_t feature_packed;
	};

	struct gpu_debug_config {
		bool enable_warm_start = true;
		bool enable_velocity_corrections = true;
		bool enable_solve_iterations = true;
		bool log_diagnostics = true;
		std::uint32_t log_interval = 60;
	};

	class gpu_solver {
	public:
		gpu_debug_config debug;

		auto create_buffers(
			vulkan::allocator& alloc
		) -> void;

		auto initialize_compute(
			gpu::context& ctx
		) -> void;

		auto dispatch_compute(
			vk::CommandBuffer cmd,
			vulkan::config& config,
			std::uint32_t frame_index
		) -> void;

		auto compute_initialized(
		) const -> bool;

		auto buffers_created(
		) const -> bool;

		auto body_stride(
		) const -> std::uint32_t;

		auto contact_stride(
		) const -> std::uint32_t;

		auto contact_lambda_offset(
		) const -> std::uint32_t;

		auto upload(
			std::span<const body_state> bodies,
			std::span<const collision_body_data> collision_data,
			std::span<const color_range> prev_coloring,
			std::span<const std::vector<std::uint32_t>> prev_body_colors,
			std::span<const velocity_motor_constraint> motors,
			std::span<const warm_start_entry> prev_contacts,
			const solver_config& solver_cfg,
			time_step dt,
			int steps
		) -> void;

		auto total_substeps(
		) const -> std::uint32_t;

		auto commit_upload(
			std::uint32_t frame_index
		) -> void;

		auto stage_readback(
			std::uint32_t frame_index
		) -> void;

		auto readback(
			std::span<body_state> bodies,
			std::vector<contact_readback_entry>& contacts_out
		) -> void;

		auto has_readback_data(
		) const -> bool;

		auto pending_dispatch(
		) const -> bool;

		auto ready_to_dispatch(
			std::uint32_t frame_index
		) const -> bool;

		auto mark_dispatched(
			std::uint32_t frame_index
		) -> void;

		auto body_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto contact_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto motor_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto color_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto body_contact_map_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto solve_state_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto readback_buffer(
			this auto&& self,
			std::uint32_t fi
		) -> auto&;

		auto body_buffer_resources(
			this auto&& self
		) -> auto&;

		auto contact_buffer_resources(
			this auto&& self
		) -> auto&;

		auto motor_buffer_resources(
			this auto&& self
		) -> auto&;

		auto color_buffer_resources(
			this auto&& self
		) -> auto&;

		auto body_contact_map_buffer_resources(
			this auto&& self
		) -> auto&;

		auto solve_state_buffer_resources(
			this auto&& self
		) -> auto&;

		auto collision_pair_buffer_resources(
			this auto&& self
		) -> auto&;

		auto collision_state_buffer_resources(
			this auto&& self
		) -> auto&;

		auto warm_start_buffer_resources(
			this auto&& self
		) -> auto&;

		auto body_count(
		) const -> std::uint32_t;

		auto contact_count(
		) const -> std::uint32_t;

		auto motor_count(
		) const -> std::uint32_t;

		auto color_ranges(
		) const -> std::span<const color_range>;

		auto motor_only_offset(
		) const -> std::uint32_t;

		auto motor_only_count(
		) const -> std::uint32_t;

		auto solver_cfg(
		) const -> const solver_config&;

		auto dt(
		) const -> time_step;

		auto frame_count(
		) const -> std::uint32_t;

	private:
		struct compute_state {
			resource::handle<shader> predict;
			resource::handle<shader> solve_color;
			resource::handle<shader> update_lambda;
			resource::handle<shader> derive_velocities;
			resource::handle<shader> sequential_passes;
			resource::handle<shader> finalize;
			resource::handle<shader> collision_reset;
			resource::handle<shader> collision_broad_phase;
			resource::handle<shader> collision_narrow_phase;
			resource::handle<shader> collision_build_adjacency;

			vk::raii::Pipeline predict_pipeline = nullptr;
			vk::raii::Pipeline solve_color_pipeline = nullptr;
			vk::raii::Pipeline update_lambda_pipeline = nullptr;
			vk::raii::Pipeline derive_velocities_pipeline = nullptr;
			vk::raii::Pipeline sequential_passes_pipeline = nullptr;
			vk::raii::Pipeline finalize_pipeline = nullptr;
			vk::raii::Pipeline collision_reset_pipeline = nullptr;
			vk::raii::Pipeline collision_broad_phase_pipeline = nullptr;
			vk::raii::Pipeline collision_narrow_phase_pipeline = nullptr;
			vk::raii::Pipeline collision_build_adjacency_pipeline = nullptr;

			vk::raii::PipelineLayout pipeline_layout = nullptr;
			per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;
			vk::raii::QueryPool query_pool = nullptr;
			float timestamp_period = 0.f;
			float solve_ms = 0.f;
			bool descriptors_initialized = false;
			bool initialized = false;
		} m_compute;

		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		bool m_awaiting_readback = false;
		bool m_fi_locked = false;

		std::uint32_t m_awaiting_readback_fi = 0;
		std::uint32_t m_locked_fi = 0;
		std::uint32_t m_frame_count = 0;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_contact_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_num_colors = 0;
		std::uint32_t m_gpu_num_colors = 8;

		std::uint32_t m_steps = 1;
		solver_config m_solver_cfg;
		time_step m_dt{};

		buffer_layout m_body_layout;
		buffer_layout m_contact_layout;
		buffer_layout m_motor_layout;

		std::vector<color_range> m_color_ranges;
		std::uint32_t m_motor_only_offset = 0;
		std::uint32_t m_motor_only_count = 0;

		per_frame_resource<vulkan::buffer_resource> m_body_buffer;
		per_frame_resource<vulkan::buffer_resource> m_contact_buffer;
		per_frame_resource<vulkan::buffer_resource> m_motor_buffer;
		per_frame_resource<vulkan::buffer_resource> m_color_buffer;
		per_frame_resource<vulkan::buffer_resource> m_body_contact_map_buffer;
		per_frame_resource<vulkan::buffer_resource> m_solve_state_buffer;
		per_frame_resource<vulkan::buffer_resource> m_readback_buffer;
		per_frame_resource<vulkan::buffer_resource> m_collision_pair_buffer;
		per_frame_resource<vulkan::buffer_resource> m_collision_state_buffer;
		per_frame_resource<vulkan::buffer_resource> m_warm_start_buffer;

		struct readback_frame_info {
			std::uint32_t body_count = 0;
			std::uint32_t contact_count = 0;
		};
		std::array<readback_frame_info, 2> m_readback_info{};

		std::vector<std::byte> m_upload_body_data;
		std::vector<std::byte> m_upload_motor_data;
		std::vector<std::uint32_t> m_upload_color_data;
		std::vector<std::uint32_t> m_upload_body_contact_map;
		std::vector<std::uint32_t> m_upload_collision_state;
		std::vector<warm_start_entry> m_upload_warm_starts;

		std::vector<std::byte> m_staged_body_data;
		std::vector<std::byte> m_staged_contact_data;
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;

		bool m_staged_valid = false;
		bool m_readback_patched_this_frame = false;
	};
}

auto gse::vbd::gpu_solver::create_buffers(vulkan::allocator& alloc) -> void {
	constexpr auto usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc;
	constexpr auto readback_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

	for (auto& buf : m_body_buffer) {
		buf = alloc.create_buffer({
			.size = max_bodies * m_body_layout.stride,
			.usage = usage | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	for (auto& buf : m_contact_buffer) {
		buf = alloc.create_buffer({
			.size = max_contacts * m_contact_layout.stride,
			.usage = usage
		});
	}

	for (auto& buf : m_motor_buffer) {
		buf = alloc.create_buffer({
			.size = max_motors * m_motor_layout.stride,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		});
	}

	const vk::DeviceSize color_buffer_size = max_colors * sizeof(std::uint32_t) * 2 + max_bodies * sizeof(std::uint32_t);
	for (auto& buf : m_color_buffer) {
		buf = alloc.create_buffer({
			.size = color_buffer_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		});
	}

	const vk::DeviceSize map_buffer_size =
		max_bodies * sizeof(std::uint32_t) * 2 +
		max_contacts * 2 * sizeof(std::uint32_t) +
		max_bodies * sizeof(std::uint32_t);
	for (auto& buf : m_body_contact_map_buffer) {
		buf = alloc.create_buffer({
			.size = map_buffer_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		});
	}

	for (auto& buf : m_solve_state_buffer) {
		buf = alloc.create_buffer({
			.size = max_bodies * solve_state_float4s_per_body * sizeof(float) * 4,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		});
	}

	const vk::DeviceSize collision_pair_size =
		sizeof(std::uint32_t) + max_collision_pairs * 2 * sizeof(std::uint32_t);
	for (auto& buf : m_collision_pair_buffer) {
		buf = alloc.create_buffer({
			.size = collision_pair_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		});
	}

	const vk::DeviceSize collision_state_size = collision_state_uints * sizeof(std::uint32_t);
	for (auto& buf : m_collision_state_buffer) {
		buf = alloc.create_buffer({
			.size = collision_state_size,
			.usage = usage
		});
	}

	const vk::DeviceSize warm_start_size = max_contacts * sizeof(warm_start_entry);
	for (auto& buf : m_warm_start_buffer) {
		buf = alloc.create_buffer({
			.size = std::max(warm_start_size, static_cast<vk::DeviceSize>(16)),
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		});
	}

	const vk::DeviceSize readback_size =
		max_bodies * m_body_layout.stride +
		max_contacts * m_contact_layout.stride +
		collision_state_uints * sizeof(std::uint32_t);
	for (auto& rb : m_readback_buffer) {
		rb = alloc.create_buffer({
			.size = readback_size,
			.usage = readback_usage
		});
		std::memset(rb.allocation.mapped(), 0, rb.allocation.size());
	}

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(
	const std::span<const body_state> bodies,
	const std::span<const collision_body_data> collision_data,
	const std::span<const color_range> prev_coloring,
	const std::span<const std::vector<std::uint32_t>> prev_body_colors,
	const std::span<const velocity_motor_constraint> motors,
	const std::span<const warm_start_entry> prev_contacts,
	const solver_config& solver_cfg,
	const time_step dt,
	const int steps
) -> void {
	m_body_count = static_cast<std::uint32_t>(std::min(bodies.size(), static_cast<std::size_t>(max_bodies)));
	m_contact_count = max_contacts;
	m_motor_count = static_cast<std::uint32_t>(std::min(motors.size(), static_cast<std::size_t>(max_motors)));
	m_num_colors = static_cast<std::uint32_t>(prev_body_colors.size());
	m_steps = static_cast<std::uint32_t>(std::max(steps, 1));
	m_solver_cfg = solver_cfg;
	m_dt = dt;

	if (m_body_count == 0) {
		m_pending_dispatch = false;
		return;
	}

	m_readback_patched_this_frame = false;

	const auto& bo = m_body_layout.offsets;
	m_upload_body_data.resize(m_body_count * m_body_layout.stride, std::byte{0});
	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		const auto& [
			position,
			predicted_position,
			inertia_target,
			old_position,
			body_velocity,
			predicted_velocity,
			orientation,
			predicted_orientation,
			angular_inertia_target,
			old_orientation,
			body_angular_velocity,
			predicted_angular_velocity,
			motor_target,
			mass_value,
			inv_inertia,
			locked,
			update_orientation,
			affected_by_gravity,
			sleep_counter
		] = bodies[i];

		auto* elem = m_upload_body_data.data() + i * m_body_layout.stride;

		const auto pos = position.as<meters>();
		const bool has_valid_pos = locked || (pos.x() != 0.f || pos.y() != 0.f || pos.z() != 0.f);
		const bool use_cpu_state = has_valid_pos || m_frame_count < 2;

		if (use_cpu_state) {
			gse::memcpy(elem + bo.at("position"), &pos);

			const auto pp = predicted_position.as<meters>();
			gse::memcpy(elem + bo.at("predicted_position"), &pp);

			const auto it_pos = inertia_target.as<meters>();
			gse::memcpy(elem + bo.at("inertia_target"), &it_pos);

			const auto op = old_position.as<meters>();
			gse::memcpy(elem + bo.at("old_position"), &op);

			const auto vel = body_velocity.as<meters_per_second>();
			gse::memcpy(elem + bo.at("velocity"), &vel);

			const auto pv = predicted_velocity.as<meters_per_second>();
			gse::memcpy(elem + bo.at("predicted_velocity"), &pv);

			gse::memcpy(elem + bo.at("orientation"), &orientation);

			gse::memcpy(elem + bo.at("predicted_orientation"), &predicted_orientation);

			gse::memcpy(elem + bo.at("angular_inertia_target"), &angular_inertia_target);

			gse::memcpy(elem + bo.at("old_orientation"), &old_orientation);

			const auto av = body_angular_velocity.as<radians_per_second>();
			gse::memcpy(elem + bo.at("angular_velocity"), &av);

			const auto pav = predicted_angular_velocity.as<radians_per_second>();
			gse::memcpy(elem + bo.at("predicted_angular_velocity"), &pav);
		}

		const auto mt = motor_target.as<meters>();
		gse::memcpy(elem + bo.at("motor_target"), &mt);

		const float mass = mass_value.as<kilograms>();
		gse::memcpy(elem + bo.at("mass"), &mass);

		std::uint32_t flags = 0;
		if (locked) flags |= flag_locked;
		if (update_orientation) flags |= flag_update_orientation;
		if (affected_by_gravity) flags |= flag_affected_by_gravity;
		gse::memcpy(elem + bo.at("flags"), &flags);

		gse::memcpy(elem + bo.at("sleep_counter"), &sleep_counter);

		const auto inv_i = mat3{ inv_inertia.data };
		gse::memcpy(elem + bo.at("inv_inertia_col0"), &inv_i[0]);
		gse::memcpy(elem + bo.at("inv_inertia_col1"), &inv_i[1]);
		gse::memcpy(elem + bo.at("inv_inertia_col2"), &inv_i[2]);

		if (i < collision_data.size()) {
			const auto& [half_extents, aabb_min, aabb_max] = collision_data[i];
			gse::memcpy(elem + bo.at("half_extents"), &half_extents);
			gse::memcpy(elem + bo.at("aabb_min"), &aabb_min);
			gse::memcpy(elem + bo.at("aabb_max"), &aabb_max);
		}
	}

	const auto& mo = m_motor_layout.offsets;
	m_upload_motor_data.assign(m_motor_count * m_motor_layout.stride, std::byte{0});
	for (std::uint32_t i = 0; i < m_motor_count; ++i) {
		const auto& m = motors[i];
		auto* elem = m_upload_motor_data.data() + i * m_motor_layout.stride;

		gse::memcpy(elem + mo.at("body_index"), &m.body_index);
		gse::memcpy(elem + mo.at("compliance"), &m.compliance);

		const float max_f = m.max_force.as<newtons>();
		gse::memcpy(elem + mo.at("max_force"), &max_f);

		const std::uint32_t horiz = m.horizontal_only ? 1u : 0u;
		gse::memcpy(elem + mo.at("horizontal_only"), &horiz);

		const auto tv = m.target_velocity.as<meters_per_second>();
		gse::memcpy(elem + mo.at("target_velocity"), &tv);
	}

	constexpr std::uint32_t color_data_size = max_colors * 2 + max_bodies;
	m_upload_color_data.resize(color_data_size);
	std::memset(m_upload_color_data.data(), 0, color_data_size * sizeof(std::uint32_t));

	auto* color_offsets = m_upload_color_data.data();
	auto* color_counts = m_upload_color_data.data() + max_colors;
	auto* body_indices = m_upload_color_data.data() + max_colors * 2;

	m_color_ranges.clear();
	m_color_ranges.reserve(m_num_colors);

	std::uint32_t idx = 0;
	std::unordered_set<std::uint32_t> contacted_bodies;
	for (std::uint32_t c = 0; c < m_num_colors; ++c) {
		const auto& bc = prev_body_colors[c];
		color_offsets[c] = idx;
		color_counts[c] = static_cast<std::uint32_t>(bc.size());
		m_color_ranges.push_back({ idx, static_cast<std::uint32_t>(bc.size()) });
		for (const auto bi : bc) {
			body_indices[idx++] = bi;
			contacted_bodies.insert(bi);
		}
	}

	m_motor_only_offset = idx;
	m_motor_only_count = 0;
	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		if (!contacted_bodies.contains(motors[mi].body_index)) {
			body_indices[idx++] = motors[mi].body_index;
			m_motor_only_count++;
		}
	}

	constexpr std::uint32_t map_data_size = max_bodies * 2 + max_contacts * 2 + max_bodies;
	m_upload_body_contact_map.resize(map_data_size);
	std::memset(m_upload_body_contact_map.data(), 0, map_data_size * sizeof(std::uint32_t));

	auto* body_motor_index = m_upload_body_contact_map.data() + max_bodies * 2 + max_contacts * 2;
	std::memset(body_motor_index, 0xFF, max_bodies * sizeof(std::uint32_t));

	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		body_motor_index[motors[mi].body_index] = mi;
	}

	m_upload_collision_state.resize(collision_state_uints, 0);
	m_upload_collision_state[0] = 0;
	std::uint32_t margin_bits;
	float margin_f = solver_cfg.speculative_margin.as<meters>();
	std::memcpy(&margin_bits, &margin_f, sizeof(float));
	m_upload_collision_state[1] = margin_bits;
	std::uint32_t friction_bits;
	std::memcpy(&friction_bits, &solver_cfg.friction_coefficient, sizeof(float));
	m_upload_collision_state[2] = friction_bits;
	std::uint32_t compliance_bits;
	std::memcpy(&compliance_bits, &solver_cfg.contact_compliance, sizeof(float));
	m_upload_collision_state[3] = compliance_bits;
	std::uint32_t damping_bits;
	std::memcpy(&damping_bits, &solver_cfg.contact_damping, sizeof(float));
	m_upload_collision_state[4] = damping_bits;
	m_upload_collision_state[5] = static_cast<std::uint32_t>(prev_contacts.size());
	m_upload_collision_state[6] = m_num_colors;

	if (debug.enable_warm_start) {
		m_upload_warm_starts.assign(prev_contacts.begin(), prev_contacts.end());
	} else {
		m_upload_warm_starts.clear();
		m_upload_collision_state[5] = 0;
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::total_substeps() const -> std::uint32_t {
	return m_solver_cfg.substeps * m_steps;
}

auto gse::vbd::gpu_solver::commit_upload(const std::uint32_t frame_index) -> void {
	if (!m_pending_dispatch || m_body_count == 0) return;

	gse::memcpy(m_body_buffer[frame_index].allocation.mapped(), m_upload_body_data);
	gse::memcpy(m_motor_buffer[frame_index].allocation.mapped(), m_upload_motor_data);
	gse::memcpy(m_color_buffer[frame_index].allocation.mapped(), m_upload_color_data);
	gse::memcpy(m_body_contact_map_buffer[frame_index].allocation.mapped(), m_upload_body_contact_map);
	gse::memcpy(m_collision_state_buffer[frame_index].allocation.mapped(), m_upload_collision_state);

	if (!m_upload_warm_starts.empty()) {
		std::memcpy(
			m_warm_start_buffer[frame_index].allocation.mapped(),
			m_upload_warm_starts.data(),
			m_upload_warm_starts.size() * sizeof(warm_start_entry)
		);
	}
}

auto gse::vbd::gpu_solver::stage_readback(const std::uint32_t frame_index) -> void {
	if (m_frame_count < 2) {
		m_awaiting_readback = false;
		return;
	}
	if (!m_awaiting_readback) return;
	if (frame_index != m_awaiting_readback_fi) return;

	auto& info = m_readback_info[frame_index];

	if (info.body_count == 0) return;

	const auto* rb = m_readback_buffer[frame_index].allocation.mapped();

	m_staged_body_count = info.body_count;
	m_staged_body_data.assign(rb, rb + info.body_count * m_body_layout.stride);

	const vk::DeviceSize contact_region_offset = max_bodies * m_body_layout.stride;
	const vk::DeviceSize count_offset = contact_region_offset + max_contacts * m_contact_layout.stride;

	std::uint32_t gpu_contact_count = 0;
	std::memcpy(&gpu_contact_count, rb + count_offset, sizeof(std::uint32_t));
	m_staged_contact_count = std::min(gpu_contact_count, max_contacts);

	std::uint32_t readback_num_colors = 0;
	std::memcpy(&readback_num_colors, rb + count_offset + 6 * sizeof(std::uint32_t), sizeof(std::uint32_t));
	if (readback_num_colors > 0 && readback_num_colors <= max_colors) {
		m_gpu_num_colors = readback_num_colors;
	}

	const vk::DeviceSize contact_data_size = m_staged_contact_count * m_contact_layout.stride;
	m_staged_contact_data.assign(rb + contact_region_offset, rb + contact_region_offset + contact_data_size);

	info = {};

	m_staged_valid = true;
	m_awaiting_readback = false;

	if (!m_fi_locked) {
		m_fi_locked = true;
		m_locked_fi = frame_index;
	}

	const auto& bo = m_body_layout.offsets;

	if (!m_upload_body_data.empty()) {
		auto is_finite_vec3 = [](const float* v) {
			return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
		};

		const std::uint32_t patch_count = std::min(m_staged_body_count, m_body_count);
		for (std::uint32_t i = 0; i < patch_count; ++i) {
			const auto* src = m_staged_body_data.data() + i * m_body_layout.stride;
			auto* dst = m_upload_body_data.data() + i * m_body_layout.stride;

			std::uint32_t cpu_flags;
			std::memcpy(&cpu_flags, dst + bo.at("flags"), sizeof(std::uint32_t));
			if (cpu_flags & flag_locked) continue;

			float orient[4];
			std::memcpy(orient, src + bo.at("orientation"), sizeof(float) * 4);
			const float q_len_sq = orient[0] * orient[0] + orient[1] * orient[1] + orient[2] * orient[2] + orient[3] * orient[3];
			if (q_len_sq < 0.5f) continue;

			float pos[3];
			std::memcpy(pos, src + bo.at("position"), sizeof(float) * 3);
			if (!is_finite_vec3(pos)) continue;

			float vel[3];
			std::memcpy(vel, src + bo.at("velocity"), sizeof(float) * 3);
			if (!is_finite_vec3(vel)) continue;

			std::memcpy(dst + bo.at("position"), pos, sizeof(float) * 3);
			std::memcpy(dst + bo.at("velocity"), vel, sizeof(float) * 3);
			std::memcpy(dst + bo.at("orientation"), orient, sizeof(float) * 4);
			std::memcpy(dst + bo.at("angular_velocity"), src + bo.at("angular_velocity"), sizeof(float) * 3);
			std::memcpy(dst + bo.at("sleep_counter"), src + bo.at("sleep_counter"), sizeof(std::uint32_t));
			std::memcpy(dst + bo.at("aabb_min"), src + bo.at("aabb_min"), sizeof(float) * 3);
			std::memcpy(dst + bo.at("aabb_max"), src + bo.at("aabb_max"), sizeof(float) * 3);
		}
		m_readback_patched_this_frame = true;
	}

	if (debug.log_diagnostics) {
		float max_lin_vel = 0.f;
		float max_ang_vel = 0.f;
		float max_pos_y = -1e30f;
		float min_pos_y = 1e30f;
		std::uint32_t max_lin_body = 0;
		std::uint32_t max_ang_body = 0;
		std::uint32_t nan_count = 0;
		std::uint32_t bad_quat_count = 0;

		for (std::uint32_t i = 0; i < m_staged_body_count; ++i) {
			const auto* elem = m_staged_body_data.data() + i * m_body_layout.stride;

			std::uint32_t flags;
			std::memcpy(&flags, elem + bo.at("flags"), sizeof(std::uint32_t));
			if (flags & flag_locked) continue;

			float pos[3], vel[3], ang[3], orient[4];
			std::memcpy(pos, elem + bo.at("position"), sizeof(float) * 3);
			std::memcpy(vel, elem + bo.at("velocity"), sizeof(float) * 3);
			std::memcpy(ang, elem + bo.at("angular_velocity"), sizeof(float) * 3);
			std::memcpy(orient, elem + bo.at("orientation"), sizeof(float) * 4);

			bool pos_nan = !std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2]);
			bool vel_nan = !std::isfinite(vel[0]) || !std::isfinite(vel[1]) || !std::isfinite(vel[2]);
			if (pos_nan || vel_nan) { nan_count++; continue; }

			float q_len_sq = orient[0]*orient[0] + orient[1]*orient[1] + orient[2]*orient[2] + orient[3]*orient[3];
			if (q_len_sq < 0.5f) bad_quat_count++;

			float lin_spd = std::sqrt(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]);
			float ang_spd = std::sqrt(ang[0]*ang[0] + ang[1]*ang[1] + ang[2]*ang[2]);

			if (lin_spd > max_lin_vel) { max_lin_vel = lin_spd; max_lin_body = i; }
			if (ang_spd > max_ang_vel) { max_ang_vel = ang_spd; max_ang_body = i; }
			if (pos[1] > max_pos_y) max_pos_y = pos[1];
			if (pos[1] < min_pos_y) min_pos_y = pos[1];
		}

		bool trouble = nan_count > 0 || bad_quat_count > 0 || max_lin_vel > 20.f || max_ang_vel > 30.f;
		bool periodic = m_frame_count % debug.log_interval == 0;

		if (trouble || periodic) {
			log::println("[GPU Phys] frame={} bodies={} contacts={} nan={} bad_q={}", m_frame_count, m_staged_body_count, m_staged_contact_count, nan_count, bad_quat_count);
			log::println("  max_lin_vel={:.2f} (body {}) max_ang_vel={:.2f} (body {}) y_range=[{:.2f}, {:.2f}]", max_lin_vel, max_lin_body, max_ang_vel, max_ang_body, min_pos_y, max_pos_y);
		}

		if (trouble) {
			log::flush();
		}

		if (max_lin_vel > 20.f) {
			const auto* elem = m_staged_body_data.data() + max_lin_body * m_body_layout.stride;
			float pos[3], vel[3];
			std::memcpy(pos, elem + bo.at("position"), sizeof(float) * 3);
			std::memcpy(vel, elem + bo.at("velocity"), sizeof(float) * 3);
			log::println("  FAST body {}: pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f})", max_lin_body, pos[0], pos[1], pos[2], vel[0], vel[1], vel[2]);
			log::flush();
		}
	}
}

auto gse::vbd::gpu_solver::readback(const std::span<body_state> bodies, std::vector<contact_readback_entry>& contacts_out) -> void {
	if (!m_staged_valid) return;

	auto is_finite_vec3 = [](const float* v) {
		return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
	};

	const auto& bo = m_body_layout.offsets;
	const std::uint32_t count = std::min(m_staged_body_count, static_cast<std::uint32_t>(bodies.size()));
	for (std::uint32_t i = 0; i < count; ++i) {
		const auto* elem = m_staged_body_data.data() + i * m_body_layout.stride;
		auto& b = bodies[i];

		float orient[4];
		std::memcpy(orient, elem + bo.at("orientation"), sizeof(float) * 4);
		const float q_len_sq = orient[0] * orient[0] + orient[1] * orient[1] + orient[2] * orient[2] + orient[3] * orient[3];
		if (q_len_sq < 0.5f) continue;

		float pos[3];
		std::memcpy(pos, elem + bo.at("position"), sizeof(float) * 3);
		if (!is_finite_vec3(pos)) continue;

		float vel[3];
		std::memcpy(vel, elem + bo.at("velocity"), sizeof(float) * 3);

		b.position = unitless::vec3(pos[0], pos[1], pos[2]) * meters(1.f);

		if (is_finite_vec3(vel)) {
			b.body_velocity = unitless::vec3(vel[0], vel[1], vel[2]) * meters_per_second(1.f);
		} else {
			b.body_velocity = {};
		}

		std::memcpy(&b.orientation, orient, sizeof(quat));

		float av[3];
		std::memcpy(av, elem + bo.at("angular_velocity"), sizeof(float) * 3);
		if (is_finite_vec3(av)) {
			b.body_angular_velocity = unitless::vec3(av[0], av[1], av[2]) * radians_per_second(1.f);
		} else {
			b.body_angular_velocity = {};
		}

		std::memcpy(&b.sleep_counter, elem + bo.at("sleep_counter"), sizeof(std::uint32_t));
	}

	const auto& co = m_contact_layout.offsets;
	contacts_out.clear();
	contacts_out.reserve(m_staged_contact_count);
	for (std::uint32_t i = 0; i < m_staged_contact_count; ++i) {
		const auto* elem = m_staged_contact_data.data() + i * m_contact_layout.stride;

		contact_readback_entry entry;
		std::memcpy(&entry.body_a, elem + co.at("body_a"), sizeof(std::uint32_t));
		std::memcpy(&entry.body_b, elem + co.at("body_b"), sizeof(std::uint32_t));
		std::memcpy(&entry.lambda, elem + co.at("lambda"), sizeof(float));
		std::memcpy(&entry.normal, elem + co.at("normal"), sizeof(unitless::vec3));
		std::memcpy(&entry.separation, elem + co.at("initial_separation"), sizeof(float));
		std::memcpy(&entry.feature_packed, elem + co.at("feature_packed"), sizeof(std::uint32_t));

		contacts_out.push_back(entry);
	}

	m_staged_valid = false;

}

auto gse::vbd::gpu_solver::has_readback_data() const -> bool {
	return m_staged_valid;
}

auto gse::vbd::gpu_solver::pending_dispatch() const -> bool {
	return m_pending_dispatch;
}

auto gse::vbd::gpu_solver::ready_to_dispatch(const std::uint32_t frame_index) const -> bool {
	if (m_awaiting_readback) return false;
	if (m_fi_locked && frame_index != m_locked_fi) return false;
	return true;
}

auto gse::vbd::gpu_solver::mark_dispatched(const std::uint32_t frame_index) -> void {
	m_readback_info[frame_index] = { m_body_count, m_contact_count };
	m_pending_dispatch = false;
	m_awaiting_readback = true;
	m_awaiting_readback_fi = frame_index;
	m_frame_count++;
}

auto gse::vbd::gpu_solver::body_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_body_buffer[fi];
}

auto gse::vbd::gpu_solver::contact_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_contact_buffer[fi];
}

auto gse::vbd::gpu_solver::motor_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_motor_buffer[fi];
}

auto gse::vbd::gpu_solver::color_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_color_buffer[fi];
}

auto gse::vbd::gpu_solver::body_contact_map_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_body_contact_map_buffer[fi];
}

auto gse::vbd::gpu_solver::solve_state_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_solve_state_buffer[fi];
}

auto gse::vbd::gpu_solver::readback_buffer(this auto&& self, std::uint32_t fi) -> auto& {
	return self.m_readback_buffer[fi];
}

auto gse::vbd::gpu_solver::body_buffer_resources(this auto&& self) -> auto& {
	return self.m_body_buffer.resources();
}

auto gse::vbd::gpu_solver::contact_buffer_resources(this auto&& self) -> auto& {
	return self.m_contact_buffer.resources();
}

auto gse::vbd::gpu_solver::motor_buffer_resources(this auto&& self) -> auto& {
	return self.m_motor_buffer.resources();
}

auto gse::vbd::gpu_solver::color_buffer_resources(this auto&& self) -> auto& {
	return self.m_color_buffer.resources();
}

auto gse::vbd::gpu_solver::body_contact_map_buffer_resources(this auto&& self) -> auto& {
	return self.m_body_contact_map_buffer.resources();
}

auto gse::vbd::gpu_solver::solve_state_buffer_resources(this auto&& self) -> auto& {
	return self.m_solve_state_buffer.resources();
}

auto gse::vbd::gpu_solver::collision_pair_buffer_resources(this auto&& self) -> auto& {
	return self.m_collision_pair_buffer.resources();
}

auto gse::vbd::gpu_solver::collision_state_buffer_resources(this auto&& self) -> auto& {
	return self.m_collision_state_buffer.resources();
}

auto gse::vbd::gpu_solver::warm_start_buffer_resources(this auto&& self) -> auto& {
	return self.m_warm_start_buffer.resources();
}

auto gse::vbd::gpu_solver::body_count() const -> std::uint32_t {
	return m_body_count;
}

auto gse::vbd::gpu_solver::contact_count() const -> std::uint32_t {
	return m_contact_count;
}

auto gse::vbd::gpu_solver::motor_count() const -> std::uint32_t {
	return m_motor_count;
}

auto gse::vbd::gpu_solver::color_ranges() const -> std::span<const color_range> {
	return m_color_ranges;
}

auto gse::vbd::gpu_solver::motor_only_offset() const -> std::uint32_t {
	return m_motor_only_offset;
}

auto gse::vbd::gpu_solver::motor_only_count() const -> std::uint32_t {
	return m_motor_only_count;
}

auto gse::vbd::gpu_solver::solver_cfg() const -> const solver_config& {
	return m_solver_cfg;
}

auto gse::vbd::gpu_solver::dt() const -> time_step {
	return m_dt;
}

auto gse::vbd::gpu_solver::frame_count() const -> std::uint32_t {
	return m_frame_count;
}

auto gse::vbd::gpu_solver::initialize_compute(gpu::context& ctx) -> void {
	m_compute.predict = ctx.get<shader>("Shaders/Compute/vbd_predict");
	m_compute.solve_color = ctx.get<shader>("Shaders/Compute/vbd_solve_color");
	m_compute.update_lambda = ctx.get<shader>("Shaders/Compute/vbd_update_lambda");
	m_compute.derive_velocities = ctx.get<shader>("Shaders/Compute/vbd_derive_velocities");
	m_compute.sequential_passes = ctx.get<shader>("Shaders/Compute/vbd_sequential_passes");
	m_compute.finalize = ctx.get<shader>("Shaders/Compute/vbd_finalize");
	m_compute.collision_reset = ctx.get<shader>("Shaders/Compute/collision_reset");
	m_compute.collision_broad_phase = ctx.get<shader>("Shaders/Compute/collision_broad_phase");
	m_compute.collision_narrow_phase = ctx.get<shader>("Shaders/Compute/collision_narrow_phase");
	m_compute.collision_build_adjacency = ctx.get<shader>("Shaders/Compute/collision_build_adjacency");

	ctx.instantly_load(m_compute.predict);
	ctx.instantly_load(m_compute.solve_color);
	ctx.instantly_load(m_compute.update_lambda);
	ctx.instantly_load(m_compute.derive_velocities);
	ctx.instantly_load(m_compute.sequential_passes);
	ctx.instantly_load(m_compute.finalize);
	ctx.instantly_load(m_compute.collision_reset);
	ctx.instantly_load(m_compute.collision_broad_phase);
	ctx.instantly_load(m_compute.collision_narrow_phase);
	ctx.instantly_load(m_compute.collision_build_adjacency);

	auto& config = ctx.config();

	auto vbd_layouts = m_compute.predict->layouts();
	std::vector vbd_ranges = { m_compute.predict->push_constant_range("push_constants") };

	m_compute.pipeline_layout = config.device_config().device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(vbd_layouts.size()),
		.pSetLayouts = vbd_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(vbd_ranges.size()),
		.pPushConstantRanges = vbd_ranges.data()
	});

	auto create_pipeline = [&](const resource::handle<shader>& sh) {
		return config.device_config().device.createComputePipeline(nullptr, {
			.stage = sh->compute_stage(),
			.layout = *m_compute.pipeline_layout
		});
	};

	m_compute.predict_pipeline = create_pipeline(m_compute.predict);
	m_compute.solve_color_pipeline = create_pipeline(m_compute.solve_color);
	m_compute.update_lambda_pipeline = create_pipeline(m_compute.update_lambda);
	m_compute.derive_velocities_pipeline = create_pipeline(m_compute.derive_velocities);
	m_compute.sequential_passes_pipeline = create_pipeline(m_compute.sequential_passes);
	m_compute.finalize_pipeline = create_pipeline(m_compute.finalize);
	m_compute.collision_reset_pipeline = create_pipeline(m_compute.collision_reset);
	m_compute.collision_broad_phase_pipeline = create_pipeline(m_compute.collision_broad_phase);
	m_compute.collision_narrow_phase_pipeline = create_pipeline(m_compute.collision_narrow_phase);
	m_compute.collision_build_adjacency_pipeline = create_pipeline(m_compute.collision_build_adjacency);

	auto extract_layout = [](const resource::handle<shader>& sh, const std::string& name) {
		const auto block = sh->uniform_block(name);
		buffer_layout layout;
		layout.stride = block.size;
		for (const auto& [n, member] : block.members) {
			layout.offsets[n] = member.offset;
		}
		return layout;
	};

	m_body_layout = extract_layout(m_compute.predict, "body_data");
	m_contact_layout = extract_layout(m_compute.predict, "contact_data");
	m_motor_layout = extract_layout(m_compute.predict, "motor_data");

	m_compute.query_pool = config.device_config().device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 8
	});
	static_cast<vk::Device>(*config.device_config().device).resetQueryPool(*m_compute.query_pool, 0, 8);

	m_compute.timestamp_period = config.device_config().physical_device.getProperties().limits.timestampPeriod;

	create_buffers(config.allocator());

	m_compute.initialized = true;
}

auto gse::vbd::gpu_solver::dispatch_compute(const vk::CommandBuffer command, vulkan::config& config, const std::uint32_t frame_index) -> void {
	if (!m_compute.descriptors_initialized) {
		for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
			m_compute.descriptor_sets.resources()[i] = m_compute.predict->descriptor_set(
				config.device_config().device,
				config.descriptor_config().pool,
				shader::set::binding_type::persistent
			);

			const vk::DescriptorBufferInfo body_info{
				.buffer = m_body_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo contact_info{
				.buffer = m_contact_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo motor_info{
				.buffer = m_motor_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo color_info{
				.buffer = m_color_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo map_info{
				.buffer = m_body_contact_map_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo solve_info{
				.buffer = m_solve_state_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo collision_pair_info{
				.buffer = m_collision_pair_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo collision_state_info{
				.buffer = m_collision_state_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};
			const vk::DescriptorBufferInfo warm_start_info{
				.buffer = m_warm_start_buffer.resources()[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			auto& ds = m_compute.descriptor_sets.resources()[i];
			std::array writes{
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &body_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &contact_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &motor_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &color_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 4,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &map_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 5,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &solve_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 6,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &collision_pair_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 7,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &collision_state_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 8,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &warm_start_info
				}
			};

			config.device_config().device.updateDescriptorSets(writes, nullptr);
		}

		m_compute.descriptors_initialized = true;
	}

	if (m_body_count == 0) return;

	if (m_frame_count >= 2) {
		const std::uint32_t prev_query_base = (frame_index % 2) * 4;
		std::array<std::uint64_t, 2> timestamps{};
		const vk::Device device = *config.device_config().device;
		auto result = device.getQueryPoolResults(
			*m_compute.query_pool, prev_query_base, 2,
			sizeof(timestamps), timestamps.data(), sizeof(std::uint64_t),
			vk::QueryResultFlagBits::e64
		);
		if (result == vk::Result::eSuccess) {
			m_compute.solve_ms = static_cast<float>(timestamps[1] - timestamps[0]) * m_compute.timestamp_period * 1e-6f;
		}
	}

	const std::uint32_t query_base = (frame_index % 2) * 4;
	command.resetQueryPool(*m_compute.query_pool, query_base, 4);

	constexpr vk::MemoryBarrier2 host_to_compute{
		.srcStageMask = vk::PipelineStageFlagBits2::eHost,
		.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
	};
	constexpr vk::MemoryBarrier2 prev_transfer_to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite
	};
	constexpr vk::MemoryBarrier2 init_barriers[] = { host_to_compute, prev_transfer_to_transfer };
	command.pipelineBarrier2({ .memoryBarrierCount = 2, .pMemoryBarriers = init_barriers });

	const vk::DescriptorSet sets[] = { *m_compute.descriptor_sets[frame_index % 2] };

	constexpr vk::MemoryBarrier2 compute_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
	};
	const vk::DependencyInfo compute_dep{ .memoryBarrierCount = 1, .pMemoryBarriers = &compute_barrier };

	command.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, *m_compute.query_pool, query_base);

	const auto& cfg = m_solver_cfg;
	const std::uint32_t total = total_substeps();
	const float sub_dt = m_dt.as<seconds>() / static_cast<float>(total);
	const float h_squared = sub_dt * sub_dt;

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) {
		return (a + b - 1) / b;
	};

	auto bind_and_push = [&](const resource::handle<shader>& sh, const vk::raii::Pipeline& pipeline, std::uint32_t color_offset, std::uint32_t color_count, std::uint32_t substep, std::uint32_t iteration) {
		command.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
		command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_compute.pipeline_layout, 0, { 1, sets }, {});
		sh->push(command, *m_compute.pipeline_layout, "push_constants",
			"body_count", m_body_count,
			"contact_count", max_contacts,
			"motor_count", m_motor_count,
			"color_offset", color_offset,
			"color_count", color_count,
			"h_squared", h_squared,
			"dt", sub_dt,
			"rho", cfg.rho,
			"linear_damping", cfg.linear_damping,
			"velocity_sleep_threshold", cfg.velocity_sleep_threshold,
			"substep", substep,
			"iteration", iteration
		);
	};

	const std::uint32_t n = m_body_count;
	const std::uint32_t total_pairs = n * (n - 1) / 2;

	bind_and_push(m_compute.collision_reset, m_compute.collision_reset_pipeline, 0u, 0u, 0u, 0u);
	command.dispatch(ceil_div(m_body_count, workgroup_size), 1, 1);
	command.pipelineBarrier2(compute_dep);

	bind_and_push(m_compute.collision_broad_phase, m_compute.collision_broad_phase_pipeline, 0u, 0u, 0u, 0u);
	command.dispatch(ceil_div(total_pairs, workgroup_size), 1, 1);
	command.pipelineBarrier2(compute_dep);

	bind_and_push(m_compute.collision_narrow_phase, m_compute.collision_narrow_phase_pipeline, 0u, 0u, 0u, 0u);
	command.dispatch(ceil_div(max_collision_pairs, workgroup_size), 1, 1);
	command.pipelineBarrier2(compute_dep);

	bind_and_push(m_compute.collision_build_adjacency, m_compute.collision_build_adjacency_pipeline, 0u, 0u, 0u, 0u);
	command.dispatch(1, 1, 1);
	command.pipelineBarrier2(compute_dep);

	const std::uint32_t body_workgroups = ceil_div(m_body_count, workgroup_size);
	const std::uint32_t num_colors = std::min(std::max(m_gpu_num_colors * 2, m_gpu_num_colors + 4), max_colors);

	for (std::uint32_t sub = 0; sub < total; ++sub) {
		bind_and_push(m_compute.predict, m_compute.predict_pipeline, 0u, 0u, sub, 0u);
		command.dispatch(body_workgroups, 1, 1);
		command.pipelineBarrier2(compute_dep);

		if (debug.enable_solve_iterations) {
			for (std::uint32_t iter = 0; iter < cfg.iterations; ++iter) {
				for (std::uint32_t c = 0; c < num_colors; ++c) {
					bind_and_push(m_compute.solve_color, m_compute.solve_color_pipeline, c, 0u, sub, iter);
					command.dispatch(body_workgroups, 1, 1);
					command.pipelineBarrier2(compute_dep);
				}
			}
		}

		bind_and_push(m_compute.update_lambda, m_compute.update_lambda_pipeline, 0u, 0u, sub, 0u);
		command.dispatch(ceil_div(max_contacts, workgroup_size), 1, 1);
		command.pipelineBarrier2(compute_dep);

		bind_and_push(m_compute.derive_velocities, m_compute.derive_velocities_pipeline, 0u, 0u, sub, 0u);
		command.dispatch(body_workgroups, 1, 1);
		command.pipelineBarrier2(compute_dep);

		if (debug.enable_velocity_corrections) {
			for (std::uint32_t pass = 0; pass < 8u; ++pass) {
				for (std::uint32_t c = 0; c < num_colors; ++c) {
					bind_and_push(m_compute.sequential_passes, m_compute.sequential_passes_pipeline, c, 0u, sub, pass);
					command.dispatch(body_workgroups, 1, 1);
					command.pipelineBarrier2(compute_dep);
				}
			}
		}

		bind_and_push(m_compute.finalize, m_compute.finalize_pipeline, 0u, 0u, sub, 0u);
		command.dispatch(body_workgroups, 1, 1);
		command.pipelineBarrier2(compute_dep);
	}

	command.writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, *m_compute.query_pool, query_base + 1);

	constexpr vk::MemoryBarrier2 compute_to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead
	};

	command.pipelineBarrier2({
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &compute_to_transfer
	});

	const vk::DeviceSize body_copy_size = m_body_count * m_body_layout.stride;
	command.copyBuffer(
		m_body_buffer[frame_index].buffer, 
		m_readback_buffer[frame_index].buffer,
		vk::BufferCopy{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = body_copy_size
		}
	);

	const vk::DeviceSize contact_dst_base = max_bodies * m_body_layout.stride;
	const vk::DeviceSize contact_copy_size = max_contacts * m_contact_layout.stride;

	command.copyBuffer(
		m_contact_buffer[frame_index].buffer, 
		m_readback_buffer[frame_index].buffer,
		vk::BufferCopy{
			.srcOffset = 0,
			.dstOffset = contact_dst_base,
			.size = contact_copy_size
		}
	);

	const vk::DeviceSize count_dst = contact_dst_base + contact_copy_size;
	command.copyBuffer(
		m_collision_state_buffer[frame_index].buffer,
		m_readback_buffer[frame_index].buffer,
		vk::BufferCopy{
			.srcOffset = 0,
			.dstOffset = count_dst,
			.size = collision_state_uints * sizeof(std::uint32_t)
		}
	);

	constexpr vk::MemoryBarrier2 compute_to_host{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eHost,
		.dstAccessMask = vk::AccessFlagBits2::eHostRead
	};
	command.pipelineBarrier2({ .memoryBarrierCount = 1, .pMemoryBarriers = &compute_to_host });

	mark_dispatched(frame_index);
}

auto gse::vbd::gpu_solver::compute_initialized() const -> bool {
	return m_compute.initialized;
}

auto gse::vbd::gpu_solver::buffers_created() const -> bool {
	return m_buffers_created;
}

auto gse::vbd::gpu_solver::body_stride() const -> std::uint32_t {
	return m_body_layout.stride;
}

auto gse::vbd::gpu_solver::contact_stride() const -> std::uint32_t {
	return m_contact_layout.stride;
}

auto gse::vbd::gpu_solver::contact_lambda_offset() const -> std::uint32_t {
	return m_contact_layout.offsets.at("lambda");
}
