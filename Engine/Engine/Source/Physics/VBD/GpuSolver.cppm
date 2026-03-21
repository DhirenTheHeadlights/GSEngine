export module gse.physics:vbd_gpu_solver;

import std;

import gse.assert;
import gse.utility;

import gse.platform;
import gse.log;

import :vbd_constraints;
import :vbd_solver;	

export namespace gse::vbd {
	constexpr std::uint32_t max_bodies = 500;
	constexpr std::uint32_t max_contacts = 2000;
	constexpr std::uint32_t max_motors = 16;
	constexpr std::uint32_t max_joints = 128;
	constexpr std::uint32_t max_colors = 32;
	constexpr std::uint32_t max_collision_pairs = 8192;
	constexpr std::uint32_t workgroup_size = 64;
	constexpr std::uint32_t collision_state_header_uints = 8;

	constexpr std::uint32_t solve_state_float4s_per_body = 11;
	constexpr std::uint32_t collision_state_uints = collision_state_header_uints;

	struct buffer_layout {
		std::uint32_t stride = 0;
		std::unordered_map<std::string, std::uint32_t> offsets;
	};

	constexpr std::uint32_t flag_locked = 1u;
	constexpr std::uint32_t flag_update_orientation = 2u;
	constexpr std::uint32_t flag_affected_by_gravity = 4u;

	struct collision_body_data {
		vec3<length> half_extents;
		vec3<length> aabb_min;
		vec3<length> aabb_max;
	};

	struct warm_start_entry {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;
		bool sticking = false;
		unitless::vec3 normal;
		unitless::vec3 tangent_u;
		unitless::vec3 tangent_v;
		vec3<length> local_anchor_a;
		vec3<length> local_anchor_b;
		vec3<force> lambda;
		unitless::vec3 penalty;
	};

	struct contact_readback_entry {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;
		bool sticking = false;
		unitless::vec3 normal;
		unitless::vec3 tangent_u;
		unitless::vec3 tangent_v;
		vec3<length> local_anchor_a;
		vec3<length> local_anchor_b;
		vec3<length> c0;
		vec3<force> lambda;
		unitless::vec3 penalty;
		float friction_coeff = 0.f;
	};

	struct cached_body_offsets {
		std::uint32_t position = 0;
		std::uint32_t predicted_position = 0;
		std::uint32_t inertia_target = 0;
		std::uint32_t old_position = 0;
		std::uint32_t velocity = 0;
		std::uint32_t orientation = 0;
		std::uint32_t predicted_orientation = 0;
		std::uint32_t angular_inertia_target = 0;
		std::uint32_t old_orientation = 0;
		std::uint32_t angular_velocity = 0;
		std::uint32_t motor_target = 0;
		std::uint32_t mass = 0;
		std::uint32_t flags = 0;
		std::uint32_t sleep_counter = 0;
		std::uint32_t accel_weight = 0;
		std::uint32_t inv_inertia_col0 = 0;
		std::uint32_t inv_inertia_col1 = 0;
		std::uint32_t inv_inertia_col2 = 0;
		std::uint32_t half_extents = 0;
		std::uint32_t aabb_min = 0;
		std::uint32_t aabb_max = 0;
	};

	struct cached_contact_offsets {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint32_t feature_key_hi = 0;
		std::uint32_t feature_key_lo = 0;
		std::uint32_t sticking = 0;
		std::uint32_t normal = 0;
		std::uint32_t tangent_u = 0;
		std::uint32_t tangent_v = 0;
		std::uint32_t r_a = 0;
		std::uint32_t r_b = 0;
		std::uint32_t c0_friction = 0;
		std::uint32_t lambda = 0;
		std::uint32_t penalty = 0;
	};

	struct cached_motor_offsets {
		std::uint32_t body_index = 0;
		std::uint32_t compliance = 0;
		std::uint32_t max_force = 0;
		std::uint32_t horizontal_only = 0;
		std::uint32_t target_velocity = 0;
	};

	struct cached_warm_start_offsets {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint32_t feature_key_hi = 0;
		std::uint32_t feature_key_lo = 0;
		std::uint32_t sticking = 0;
		std::uint32_t normal = 0;
		std::uint32_t tangent_u = 0;
		std::uint32_t tangent_v = 0;
		std::uint32_t local_anchor_a = 0;
		std::uint32_t local_anchor_b = 0;
		std::uint32_t lambda = 0;
		std::uint32_t penalty = 0;
	};

	struct cached_joint_offsets {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint32_t type = 0;
		std::uint32_t limits_enabled = 0;
		std::uint32_t local_anchor_a = 0;
		std::uint32_t local_anchor_b = 0;
		std::uint32_t local_axis_a = 0;
		std::uint32_t local_axis_b = 0;
		std::uint32_t target_distance = 0;
		std::uint32_t limit_lower = 0;
		std::uint32_t limit_upper = 0;
		std::uint32_t rest_orientation = 0;
		std::uint32_t pos_lambda = 0;
		std::uint32_t pos_penalty = 0;
		std::uint32_t ang_lambda = 0;
		std::uint32_t ang_penalty = 0;
		std::uint32_t limit_lambda = 0;
		std::uint32_t limit_penalty = 0;
		std::uint32_t pos_c0 = 0;
		std::uint32_t ang_c0 = 0;
		std::uint32_t limit_c0 = 0;
	};

	class gpu_solver {
	public:
		~gpu_solver() {
			if (m_compute.initialized && m_compute.device && *m_compute.fence) {
				static_cast<void>(m_compute.device->waitForFences(
					*m_compute.fence, vk::True,
					std::numeric_limits<std::uint64_t>::max()
				));
			}
		}

		auto create_buffers(
			vulkan::allocator& alloc
		) -> void;

		auto initialize_compute(
			gpu::context& ctx
		) -> void;

		auto dispatch_compute(
			vulkan::config& config
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
			std::span<const float> accel_weights,
			std::span<const velocity_motor_constraint> motors,
			std::span<const joint_constraint> joints,
			std::span<const warm_start_entry> prev_contacts,
			std::span<const std::uint32_t> authoritative_body_indices,
			const solver_config& solver_cfg,
			time_step dt,
			int steps
		) -> void;

		auto total_substeps(
		) const -> std::uint32_t;

		auto commit_upload(
		) -> void;

		auto stage_readback(
		) -> void;

		auto readback(
			std::span<body_state> bodies,
			std::vector<contact_readback_entry>& contacts_out,
			std::span<joint_constraint> joints_out
		) -> void;

		auto has_readback_data(
		) const -> bool;

		auto pending_dispatch(
		) const -> bool;

		auto ready_to_dispatch(
		) const -> bool;

		auto mark_dispatched(
		) -> void;

		auto body_buffer(this auto&& self) -> auto& { return self.m_body_buffer; }
		auto contact_buffer(this auto&& self) -> auto& { return self.m_contact_buffer; }
		auto motor_buffer(this auto&& self) -> auto& { return self.m_motor_buffer; }
		auto color_buffer(this auto&& self) -> auto& { return self.m_color_buffer; }
		auto body_contact_map_buffer(this auto&& self) -> auto& { return self.m_body_contact_map_buffer; }
		auto solve_state_buffer(this auto&& self) -> auto& { return self.m_solve_state_buffer; }
		auto readback_buffer(this auto&& self) -> auto& { return self.m_readback_buffer; }
		auto collision_pair_buffer(this auto&& self) -> auto& { return self.m_collision_pair_buffer; }
		auto collision_state_buffer(this auto&& self) -> auto& { return self.m_collision_state_buffer; }
		auto warm_start_buffer(this auto&& self) -> auto& { return self.m_warm_start_buffer; }
		auto joint_buffer(this auto&& self) -> auto& { return self.m_joint_buffer; }

		auto body_count(
		) const -> std::uint32_t;

		auto contact_count(
		) const -> std::uint32_t;

		auto motor_count(
		) const -> std::uint32_t;

		auto joint_count(
		) const -> std::uint32_t;

		auto solver_cfg(
		) const -> const solver_config&;

		auto dt(
		) const -> time_step;

		auto frame_count(
		) const -> std::uint32_t;

		auto solve_time(
		) const -> time_step;

	private:
		struct compute_state {
			resource::handle<shader> predict;
			resource::handle<shader> solve_color;
			resource::handle<shader> update_lambda;
			resource::handle<shader> derive_velocities;
			resource::handle<shader> finalize;
			resource::handle<shader> collision_reset;
			resource::handle<shader> collision_broad_phase;
			resource::handle<shader> collision_narrow_phase;
			resource::handle<shader> collision_build_adjacency;
			resource::handle<shader> update_joint_lambda;

			vk::raii::Pipeline predict_pipeline = nullptr;
			vk::raii::Pipeline solve_color_pipeline = nullptr;
			vk::raii::Pipeline update_lambda_pipeline = nullptr;
			vk::raii::Pipeline derive_velocities_pipeline = nullptr;
			vk::raii::Pipeline finalize_pipeline = nullptr;
			vk::raii::Pipeline collision_reset_pipeline = nullptr;
			vk::raii::Pipeline collision_broad_phase_pipeline = nullptr;
			vk::raii::Pipeline collision_narrow_phase_pipeline = nullptr;
			vk::raii::Pipeline collision_build_adjacency_pipeline = nullptr;
			vk::raii::Pipeline update_joint_lambda_pipeline = nullptr;

			vk::raii::PipelineLayout pipeline_layout = nullptr;
			vk::raii::DescriptorSet descriptor_set = nullptr;
			vk::raii::QueryPool query_pool = nullptr;
			vk::raii::CommandPool command_pool = nullptr;
			vk::raii::CommandBuffer command_buffer = nullptr;
			vk::raii::Fence fence = nullptr;
			vk::raii::Queue* queue = nullptr;
			const vk::raii::Device* device = nullptr;
			float timestamp_period = 0.f;
			float solve_ms = 0.f;
			bool descriptors_initialized = false;
			bool initialized = false;
		} m_compute;

		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		bool m_readback_pending = false;
		bool m_first_submit = true;
		std::uint32_t m_frame_count = 0;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_contact_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_joint_count = 0;

		std::uint32_t m_steps = 1;
		solver_config m_solver_cfg;
		time_step m_dt{};

		buffer_layout m_body_layout;
		buffer_layout m_contact_layout;
		buffer_layout m_motor_layout;
		buffer_layout m_warm_start_layout;
		buffer_layout m_joint_layout;

		cached_body_offsets m_bo;
		cached_contact_offsets m_co;
		cached_motor_offsets m_mo;
		cached_warm_start_offsets m_wo;
		cached_joint_offsets m_jo;

		std::uint32_t m_warm_start_count = 0;

		vulkan::buffer_resource m_body_buffer;
		vulkan::buffer_resource m_contact_buffer;
		vulkan::buffer_resource m_motor_buffer;
		vulkan::buffer_resource m_color_buffer;
		vulkan::buffer_resource m_body_contact_map_buffer;
		vulkan::buffer_resource m_solve_state_buffer;
		vulkan::buffer_resource m_readback_buffer;
		vulkan::buffer_resource m_collision_pair_buffer;
		vulkan::buffer_resource m_collision_state_buffer;
		vulkan::buffer_resource m_warm_start_buffer;
		vulkan::buffer_resource m_joint_buffer;

		struct readback_frame_info {
			std::uint32_t body_count = 0;
			std::uint32_t contact_count = 0;
			std::uint32_t joint_count = 0;
		} m_readback_info;

		std::vector<std::byte> m_upload_body_data;
		std::vector<std::byte> m_upload_motor_data;
		std::vector<std::byte> m_upload_warm_start_data;
		std::vector<std::byte> m_upload_joint_data;
		std::vector<std::uint32_t> m_upload_color_data;
		std::vector<std::uint32_t> m_upload_body_contact_map;
		std::vector<std::uint32_t> m_upload_collision_state;
		std::vector<std::uint8_t> m_upload_authoritative_bodies;

		std::vector<std::byte> m_staged_contact_data;
		std::vector<std::byte> m_staged_joint_data;
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;
		std::uint32_t m_staged_joint_count = 0;
		std::uint32_t m_staged_color_count = max_colors;
		bool m_staged_valid = false;

		std::vector<std::byte> m_latest_gpu_body_data;
		std::uint32_t m_latest_gpu_body_count = 0;
	};
}

auto gse::vbd::gpu_solver::create_buffers(vulkan::allocator& alloc) -> void {
	constexpr auto usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc;
	constexpr auto readback_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

	m_body_buffer = alloc.create_buffer({
		.size = max_bodies * m_body_layout.stride,
		.usage = usage | vk::BufferUsageFlagBits::eTransferDst
	}, nullptr, "VBD Body Buffer");

	m_contact_buffer = alloc.create_buffer({
		.size = max_contacts * m_contact_layout.stride,
		.usage = usage
	}, nullptr, "VBD Contact Buffer");

	m_motor_buffer = alloc.create_buffer({
		.size = max_motors * m_motor_layout.stride,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	}, nullptr, "VBD Motor Buffer");

	const vk::DeviceSize color_buffer_size = max_colors * sizeof(std::uint32_t) * 2 + max_bodies * sizeof(std::uint32_t);
	m_color_buffer = alloc.create_buffer({
		.size = color_buffer_size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	}, nullptr, "VBD Color Buffer");

	const vk::DeviceSize map_buffer_size =
		max_bodies * sizeof(std::uint32_t) * 2 +
		max_contacts * 2 * sizeof(std::uint32_t) +
		max_bodies * sizeof(std::uint32_t) +
		max_bodies * sizeof(std::uint32_t) * 2 +
		max_joints * 2 * sizeof(std::uint32_t);

	m_body_contact_map_buffer = alloc.create_buffer({
		.size = map_buffer_size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	}, nullptr, "VBD Body Contact Map Buffer");

	m_solve_state_buffer = alloc.create_buffer({
		.size = max_bodies * solve_state_float4s_per_body * sizeof(float) * 4,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	}, nullptr, "VBD Solve State Buffer");

	const vk::DeviceSize collision_pair_size = sizeof(std::uint32_t) + max_collision_pairs * 2 * sizeof(std::uint32_t);
	m_collision_pair_buffer = alloc.create_buffer({
		.size = collision_pair_size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	}, nullptr, "VBD Collision Pair Buffer");

	const vk::DeviceSize collision_state_size = collision_state_uints * sizeof(std::uint32_t);
	m_collision_state_buffer = alloc.create_buffer({
		.size = collision_state_size,
		.usage = usage
	}, nullptr, "VBD Collision State Buffer");

	const vk::DeviceSize warm_start_size = max_contacts * m_warm_start_layout.stride;
	m_warm_start_buffer = alloc.create_buffer({
		.size = std::max(warm_start_size, static_cast<vk::DeviceSize>(16)),
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	}, nullptr, "VBD Warm Start Buffer");

	const vk::DeviceSize joint_buffer_size = max_joints * m_joint_layout.stride;
	m_joint_buffer = alloc.create_buffer({
		.size = std::max(joint_buffer_size, static_cast<vk::DeviceSize>(16)),
		.usage = usage
	}, nullptr, "VBD Joint Buffer");

	const vk::DeviceSize readback_size =
		max_bodies * m_body_layout.stride +
		max_contacts * m_contact_layout.stride +
		collision_state_uints * sizeof(std::uint32_t) +
		max_joints * m_joint_layout.stride;

	m_readback_buffer = alloc.create_buffer({
		.size = readback_size,
		.usage = readback_usage
	}, nullptr, "VBD Readback Buffer");
	std::memset(m_readback_buffer.allocation.mapped(), 0, m_readback_buffer.allocation.size());

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(
	const std::span<const body_state> bodies,
	const std::span<const collision_body_data> collision_data,
	const std::span<const float> accel_weights,
	const std::span<const velocity_motor_constraint> motors,
	const std::span<const joint_constraint> joints,
	const std::span<const warm_start_entry> prev_contacts,
	const std::span<const std::uint32_t> authoritative_body_indices,
	const solver_config& solver_cfg,
	const time_step dt,
	const int steps
) -> void {
	m_body_count = static_cast<std::uint32_t>(std::min(bodies.size(), static_cast<std::size_t>(max_bodies)));
	m_contact_count = max_contacts;
	m_motor_count = static_cast<std::uint32_t>(std::min(motors.size(), static_cast<std::size_t>(max_motors)));
	m_steps = static_cast<std::uint32_t>(std::max(steps, 1));
	m_solver_cfg = solver_cfg;
	m_dt = dt;

	if (m_body_count == 0) {
		m_pending_dispatch = false;
		return;
	}

	auto rot_axis = [](const quat& q, const unitless::vec3& v) -> unitless::vec3 {
		const unitless::vec3 u{ q[1], q[2], q[3] };
		const float s = q[0];
		const unitless::vec3 t = 2.f * cross(u, v);
		return v + s * t + cross(u, t);
	};

	const auto& [
		bo_position, bo_predicted_position, bo_inertia_target, bo_old_position,
		bo_velocity, bo_orientation, bo_predicted_orientation,
		bo_angular_inertia_target, bo_old_orientation, bo_angular_velocity,
		bo_motor_target, bo_mass, bo_flags, bo_sleep_counter, bo_accel_weight,
		bo_inv_inertia_col0, bo_inv_inertia_col1, bo_inv_inertia_col2,
		bo_half_extents, bo_aabb_min, bo_aabb_max
	] = m_bo;
	m_upload_body_data.resize(m_body_count * m_body_layout.stride, std::byte{0});
	m_upload_authoritative_bodies.assign(m_body_count, 0);
	for (const auto body_index : authoritative_body_indices) {
		if (body_index < m_upload_authoritative_bodies.size()) {
			m_upload_authoritative_bodies[body_index] = 1;
		}
	}
	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		const auto& [
			position,
			predicted_position,
			inertia_target,
			initial_position,
			body_velocity,
			orientation,
			predicted_orientation,
			angular_inertia_target,
			initial_orientation,
			body_angular_velocity,
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

		if (has_valid_pos || m_frame_count < 2) {
			gse::memcpy(elem + bo_position, &pos);

			const auto pp = predicted_position.as<meters>();
			gse::memcpy(elem + bo_predicted_position, &pp);

			const auto it_pos = inertia_target.as<meters>();
			gse::memcpy(elem + bo_inertia_target, &it_pos);

			const auto op = initial_position.as<meters>();
			gse::memcpy(elem + bo_old_position, &op);

			const auto vel = body_velocity.as<meters_per_second>();
			gse::memcpy(elem + bo_velocity, &vel);

			gse::memcpy(elem + bo_orientation, &orientation);

			gse::memcpy(elem + bo_predicted_orientation, &predicted_orientation);

			gse::memcpy(elem + bo_angular_inertia_target, &angular_inertia_target);

			gse::memcpy(elem + bo_old_orientation, &initial_orientation);

			const auto av = body_angular_velocity.as<radians_per_second>();
			gse::memcpy(elem + bo_angular_velocity, &av);
		}

		const auto mt = motor_target.as<meters>();
		gse::memcpy(elem + bo_motor_target, &mt);

		const float mass = mass_value.as<kilograms>();
		gse::memcpy(elem + bo_mass, &mass);

		std::uint32_t flags = 0;
		if (locked) flags |= flag_locked;
		if (update_orientation) flags |= flag_update_orientation;
		if (affected_by_gravity) flags |= flag_affected_by_gravity;
		gse::memcpy(elem + bo_flags, &flags);

		gse::memcpy(elem + bo_sleep_counter, &sleep_counter);

		const float accel_weight =
			i < accel_weights.size()
				? accel_weights[i]
				: 0.f;
		gse::memcpy(elem + bo_accel_weight, &accel_weight);

		const auto inv_i = inv_inertia.as<per_kilogram_meter_squared>();
		gse::memcpy(elem + bo_inv_inertia_col0, &inv_i[0]);
		gse::memcpy(elem + bo_inv_inertia_col1, &inv_i[1]);
		gse::memcpy(elem + bo_inv_inertia_col2, &inv_i[2]);

		if (i < collision_data.size()) {
			const auto& [half_extents, aabb_min, aabb_max] = collision_data[i];
			const auto he = half_extents.as<meters>();
			const auto amin = aabb_min.as<meters>();
			const auto amax = aabb_max.as<meters>();
			gse::memcpy(elem + bo_half_extents, &he);
			gse::memcpy(elem + bo_aabb_min, &amin);
			gse::memcpy(elem + bo_aabb_max, &amax);
		}
	}

	const auto& [
		mo_body_index, mo_compliance, mo_max_force,
		mo_horizontal_only, mo_target_velocity
	] = m_mo;
	m_upload_motor_data.assign(m_motor_count * m_motor_layout.stride, std::byte{0});
	for (std::uint32_t i = 0; i < m_motor_count; ++i) {
		const auto& [
			body_index, target_velocity,
			compliance, max_force, horizontal_only
		] = motors[i];
		auto* elem = m_upload_motor_data.data() + i * m_motor_layout.stride;

		gse::memcpy(elem + mo_body_index, &body_index);
		gse::memcpy(elem + mo_compliance, &compliance);

		const float max_f = max_force.as<newtons>();
		gse::memcpy(elem + mo_max_force, &max_f);

		const std::uint32_t horiz = horizontal_only ? 1u : 0u;
		gse::memcpy(elem + mo_horizontal_only, &horiz);

		const auto tv = target_velocity.as<meters_per_second>();
		gse::memcpy(elem + mo_target_velocity, &tv);
	}

	constexpr std::uint32_t color_data_size = max_colors * 2 + max_bodies;
	m_upload_color_data.resize(color_data_size);
	std::memset(m_upload_color_data.data(), 0, color_data_size * sizeof(std::uint32_t));

	constexpr std::uint32_t joint_map_base = max_bodies * 2 + max_contacts * 2 + max_bodies;
	constexpr std::uint32_t map_data_size = joint_map_base + max_bodies * 2 + max_joints * 2;
	m_upload_body_contact_map.resize(map_data_size);
	std::memset(m_upload_body_contact_map.data(), 0, map_data_size * sizeof(std::uint32_t));

	auto* body_motor_index = m_upload_body_contact_map.data() + max_bodies * 2 + max_contacts * 2;
	std::memset(body_motor_index, 0xFF, max_bodies * sizeof(std::uint32_t));

	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		body_motor_index[motors[mi].body_index] = mi;
	}

	m_upload_collision_state.resize(collision_state_uints, 0);
	m_upload_collision_state[0] = 0;
	m_upload_collision_state[1] = 0;

	m_warm_start_count = static_cast<std::uint32_t>(std::min(prev_contacts.size(), static_cast<std::size_t>(max_contacts)));

	const auto& [
		wo_body_a, wo_body_b, wo_feature_key_hi, wo_feature_key_lo,
		wo_sticking, wo_normal, wo_tangent_u, wo_tangent_v,
		wo_local_anchor_a, wo_local_anchor_b, wo_lambda, wo_penalty
	] = m_wo;
	m_upload_warm_start_data.assign(m_warm_start_count * m_warm_start_layout.stride, std::byte{0});
	for (std::uint32_t i = 0; i < m_warm_start_count; ++i) {
		const auto& [
			ws_body_a, ws_body_b, ws_feature_key, ws_sticking,
			ws_normal, ws_tangent_u, ws_tangent_v,
			ws_local_anchor_a, ws_local_anchor_b,
			ws_lambda, ws_penalty
		] = prev_contacts[i];
		auto* elem = m_upload_warm_start_data.data() + i * m_warm_start_layout.stride;

		gse::memcpy(elem + wo_body_a, &ws_body_a);
		gse::memcpy(elem + wo_body_b, &ws_body_b);
		const auto feature_hi = static_cast<std::uint32_t>(ws_feature_key >> 32);
		const auto feature_lo = static_cast<std::uint32_t>(ws_feature_key & 0xFFFFFFFFu);
		gse::memcpy(elem + wo_feature_key_hi, &feature_hi);
		gse::memcpy(elem + wo_feature_key_lo, &feature_lo);

		const std::uint32_t sticking = ws_sticking ? 1u : 0u;
		gse::memcpy(elem + wo_sticking, &sticking);

		gse::memcpy(elem + wo_normal, &ws_normal);
		gse::memcpy(elem + wo_tangent_u, &ws_tangent_u);
		gse::memcpy(elem + wo_tangent_v, &ws_tangent_v);

		const auto anchor_a = ws_local_anchor_a.as<meters>();
		gse::memcpy(elem + wo_local_anchor_a, &anchor_a);

		const auto anchor_b = ws_local_anchor_b.as<meters>();
		gse::memcpy(elem + wo_local_anchor_b, &anchor_b);

		const auto lambda = ws_lambda.as<newtons>();
		gse::memcpy(elem + wo_lambda, &lambda);

		gse::memcpy(elem + wo_penalty, &ws_penalty);
	}

	m_joint_count = static_cast<std::uint32_t>(std::min(joints.size(), static_cast<std::size_t>(max_joints)));

	const auto& [
		jo_body_a, jo_body_b, jo_type, jo_limits_enabled,
		jo_local_anchor_a, jo_local_anchor_b, jo_local_axis_a, jo_local_axis_b,
		jo_target_distance, jo_limit_lower, jo_limit_upper,
		jo_rest_orientation,
		jo_pos_lambda, jo_pos_penalty, jo_ang_lambda, jo_ang_penalty,
		jo_limit_lambda, jo_limit_penalty,
		jo_pos_c0, jo_ang_c0, jo_limit_c0
	] = m_jo;

	m_upload_joint_data.assign(m_joint_count * m_joint_layout.stride, std::byte{0});

	const time_step sub_dt = dt / static_cast<float>(std::max(m_steps, 1u));
	const time_squared h_squared = sub_dt * sub_dt;

	for (std::uint32_t i = 0; i < m_joint_count; ++i) {
		const auto& j = joints[i];
		auto* elem = m_upload_joint_data.data() + i * m_joint_layout.stride;

		gse::memcpy(elem + jo_body_a, &j.body_a);
		gse::memcpy(elem + jo_body_b, &j.body_b);

		const auto type_val = static_cast<std::uint32_t>(j.type);
		gse::memcpy(elem + jo_type, &type_val);

		const std::uint32_t limits = j.limits_enabled ? 1u : 0u;
		gse::memcpy(elem + jo_limits_enabled, &limits);

		const auto la = j.local_anchor_a.as<meters>();
		gse::memcpy(elem + jo_local_anchor_a, &la);

		const auto lb = j.local_anchor_b.as<meters>();
		gse::memcpy(elem + jo_local_anchor_b, &lb);

		gse::memcpy(elem + jo_local_axis_a, &j.local_axis_a);
		gse::memcpy(elem + jo_local_axis_b, &j.local_axis_b);

		const float target_dist = j.target_distance.as<meters>();
		gse::memcpy(elem + jo_target_distance, &target_dist);

		const float ll = j.limit_lower.as<radians>();
		gse::memcpy(elem + jo_limit_lower, &ll);
		const float lu = j.limit_upper.as<radians>();
		gse::memcpy(elem + jo_limit_upper, &lu);

		gse::memcpy(elem + jo_rest_orientation, &j.rest_orientation);

		const auto& body_a_state = bodies[j.body_a];
		const auto& body_b_state = bodies[j.body_b];

		auto pos_lambda = j.pos_lambda;
		auto pos_penalty = j.pos_penalty;
		auto ang_lambda = j.ang_lambda;
		auto ang_penalty = j.ang_penalty;
		auto limit_lambda_val = j.limit_lambda;
		auto limit_penalty_val = j.limit_penalty;

		for (int k = 0; k < 3; ++k) pos_lambda[k] *= solver_cfg.gamma;
		for (int k = 0; k < 3; ++k) ang_lambda[k] *= solver_cfg.gamma;
		if (j.limits_enabled) limit_lambda_val *= solver_cfg.gamma;

		const auto r_aw = rotate_vector(body_a_state.orientation, j.local_anchor_a);
		const auto r_bw = rotate_vector(body_b_state.orientation, j.local_anchor_b);

		auto contact_eff_mass = [&](const unitless::vec3& dir) -> mass {
			inverse_mass inv_mass_sum{};
			if (!body_a_state.locked && body_a_state.mass_value > mass{})
				inv_mass_sum += 1.f / body_a_state.mass_value;
			if (!body_b_state.locked && body_b_state.mass_value > mass{})
				inv_mass_sum += 1.f / body_b_state.mass_value;

			if (body_a_state.update_orientation && !body_a_state.locked) {
				const auto ang_j = cross(r_aw, dir);
				inv_mass_sum += dot(cross(body_a_state.inv_inertia * ang_j, r_aw), dir);
			}
			if (body_b_state.update_orientation && !body_b_state.locked) {
				const auto ang_j = cross(r_bw, dir);
				inv_mass_sum += dot(cross(body_b_state.inv_inertia * ang_j, r_bw), dir);
			}
			if (inv_mass_sum <= per_kilograms(1e-10f)) return mass{};
			return 1.f / inv_mass_sum;
		};

		auto angular_inv_i = [&](const unitless::vec3& ang_dir) -> inverse_inertia {
			inverse_inertia inv_i_sum{};
			if (!body_a_state.locked) {
				inv_i_sum += dot(ang_dir, body_a_state.inv_inertia * ang_dir);
			}
			if (!body_b_state.locked) {
				inv_i_sum += dot(ang_dir, body_b_state.inv_inertia * ang_dir);
			}
			return inv_i_sum;
		};

		const unitless::vec3 dirs[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };

		int num_pos_rows = 3;
		if (j.type == joint_type::distance) num_pos_rows = 1;
		else if (j.type == joint_type::slider) num_pos_rows = 2;

		for (int k = 0; k < num_pos_rows; ++k) {
			unitless::vec3 dir;
			if (j.type == joint_type::distance) {
				const auto d = (body_a_state.position + r_aw) - (body_b_state.position + r_bw);
				dir = magnitude(d) > meters(1e-7f) ? normalize(d) : unitless::vec3{ 0.f, 1.f, 0.f };
			}
			else if (j.type == joint_type::slider) {
				const auto axis_w = rot_axis(body_a_state.orientation, j.local_axis_a);
				unitless::vec3 perp0 = cross(axis_w, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp0) < 1e-6f) perp0 = cross(axis_w, unitless::vec3{ 1.f, 0.f, 0.f });
				perp0 = normalize(perp0);
				dir = k == 0 ? perp0 : normalize(cross(axis_w, perp0));
			}
			else {
				dir = dirs[k];
			}
			const stiffness eff = contact_eff_mass(dir) / h_squared;
			pos_penalty[k] = std::clamp(
				std::max(pos_penalty[k] * solver_cfg.gamma, eff),
				solver_cfg.penalty_min, solver_cfg.penalty_max
			);
		}

		int num_ang_rows = 3;
		if (j.type == joint_type::distance) num_ang_rows = 0;
		else if (j.type == joint_type::hinge) num_ang_rows = 2;

		for (int k = 0; k < num_ang_rows; ++k) {
			unitless::vec3 ang_dir;
			if (j.type == joint_type::hinge) {
				const auto axis_a = rot_axis(body_a_state.orientation, j.local_axis_a);
				unitless::vec3 perp_u = cross(axis_a, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp_u) < 1e-6f) perp_u = cross(axis_a, unitless::vec3{ 1.f, 0.f, 0.f });
				perp_u = normalize(perp_u);
				ang_dir = k == 0 ? perp_u : normalize(cross(axis_a, perp_u));
			}
			else {
				ang_dir = dirs[k];
			}
			const auto inv_i_sum = angular_inv_i(ang_dir);
			const angular_stiffness eff_ang = inv_i_sum > per_kilogram_meter_squared(1e-10f)
				? 1.f / inv_i_sum / h_squared / rad
				: solver_cfg.ang_penalty_min;
			ang_penalty[k] = std::clamp(
				std::max(ang_penalty[k] * solver_cfg.gamma, eff_ang),
				solver_cfg.ang_penalty_min, solver_cfg.ang_penalty_max
			);
		}

		if (j.limits_enabled) {
			const auto limit_axis = rot_axis(body_a_state.orientation, j.local_axis_a);
			const auto inv_i_sum = angular_inv_i(limit_axis);
			const angular_stiffness eff_limit = inv_i_sum > per_kilogram_meter_squared(1e-10f)
				? 1.f / inv_i_sum / h_squared / rad
				: solver_cfg.ang_penalty_min;
			limit_penalty_val = std::clamp(
				std::max(limit_penalty_val * solver_cfg.gamma, eff_limit),
				solver_cfg.ang_penalty_min, solver_cfg.ang_penalty_max
			);
		}

		const auto d_vec = (body_a_state.position + r_aw) - (body_b_state.position + r_bw);
		vec3<length> pos_c0_val;
		vec3<angle> ang_c0_val;
		angle limit_c0_val = {};

		if (j.type == joint_type::distance) {
			pos_c0_val[0] = magnitude(d_vec) - j.target_distance;
		}
		else if (j.type == joint_type::fixed || j.type == joint_type::hinge) {
			for (int k = 0; k < 3; ++k) pos_c0_val[k] = dot(dirs[k], d_vec);

			const quat q_error = body_b_state.orientation * conjugate(body_a_state.orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (j.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) ang_c0_val[k] = theta[k];
			}
			else {
				const auto axis_a = rot_axis(body_a_state.orientation, j.local_axis_a);
				const auto axis_b = rot_axis(body_b_state.orientation, j.local_axis_b);
				const auto swing_error = cross(axis_a, axis_b);
				unitless::vec3 perp_u = cross(axis_a, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp_u) < 1e-6f) perp_u = cross(axis_a, unitless::vec3{ 1.f, 0.f, 0.f });
				perp_u = normalize(perp_u);
				const auto perp_v = normalize(cross(axis_a, perp_u));
				ang_c0_val[0] = radians(dot(perp_u, swing_error));
				ang_c0_val[1] = radians(dot(perp_v, swing_error));

				if (j.limits_enabled) {
					const angle twist_angle = dot(axis_a, theta);
					if (twist_angle < j.limit_lower) limit_c0_val = twist_angle - j.limit_lower;
					else if (twist_angle > j.limit_upper) limit_c0_val = twist_angle - j.limit_upper;
				}
			}
		}
		else if (j.type == joint_type::slider) {
			const auto axis_w = normalize(rot_axis(body_a_state.orientation, j.local_axis_a));
			unitless::vec3 perp0 = cross(axis_w, unitless::vec3{ 0.f, 1.f, 0.f });
			if (magnitude(perp0) < 1e-6f) perp0 = cross(axis_w, unitless::vec3{ 1.f, 0.f, 0.f });
			perp0 = normalize(perp0);
			const auto perp1 = normalize(cross(axis_w, perp0));
			pos_c0_val[0] = dot(perp0, d_vec);
			pos_c0_val[1] = dot(perp1, d_vec);

			const auto slider_theta = to_axis_angle(body_b_state.orientation * conjugate(body_a_state.orientation) * conjugate(j.rest_orientation));
			for (int k = 0; k < 3; ++k) ang_c0_val[k] = slider_theta[k];
		}

		gse::memcpy(elem + jo_pos_lambda, &pos_lambda);
		gse::memcpy(elem + jo_pos_penalty, &pos_penalty);

		gse::memcpy(elem + jo_ang_lambda, &ang_lambda);
		gse::memcpy(elem + jo_ang_penalty, &ang_penalty);

		gse::memcpy(elem + jo_limit_lambda, &limit_lambda_val);
		gse::memcpy(elem + jo_limit_penalty, &limit_penalty_val);

		gse::memcpy(elem + jo_pos_c0, &pos_c0_val);
		gse::memcpy(elem + jo_ang_c0, &ang_c0_val);
		gse::memcpy(elem + jo_limit_c0, &limit_c0_val);
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::total_substeps() const -> std::uint32_t {
	return 1 * m_steps;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_pending_dispatch || m_body_count == 0) return;

	if (m_latest_gpu_body_count > 0 && !m_latest_gpu_body_data.empty() && !m_upload_body_data.empty()) {
		const auto& [
			bo_position, bo_predicted_position, bo_inertia_target, bo_old_position,
			bo_velocity, bo_orientation, bo_predicted_orientation,
			bo_angular_inertia_target, bo_old_orientation, bo_angular_velocity,
			bo_motor_target, bo_mass, bo_flags, bo_sleep_counter, bo_accel_weight,
			bo_inv_inertia_col0, bo_inv_inertia_col1, bo_inv_inertia_col2,
			bo_half_extents, bo_aabb_min, bo_aabb_max
		] = m_bo;
		auto is_finite_vec3 = [](const float* v) {
			return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
		};

		const std::uint32_t patch_count = std::min(m_latest_gpu_body_count, m_body_count);
		for (std::uint32_t i = 0; i < patch_count; ++i) {
			const auto* src = m_latest_gpu_body_data.data() + i * m_body_layout.stride;
			auto* dst = m_upload_body_data.data() + i * m_body_layout.stride;

			if (i < m_upload_authoritative_bodies.size() && m_upload_authoritative_bodies[i] != 0) {
				continue;
			}

			std::uint32_t cpu_flags = 0;
			gse::memcpy(cpu_flags, dst + bo_flags);
			if (cpu_flags & flag_locked) continue;

			float orient[4];
			gse::memcpy(orient, src + bo_orientation);
			const float q_len_sq = orient[0] * orient[0] + orient[1] * orient[1] + orient[2] * orient[2] + orient[3] * orient[3];
			if (q_len_sq < 0.5f) continue;

			float pos[3];
			gse::memcpy(pos, src + bo_position);
			if (!is_finite_vec3(pos)) continue;

			float vel[3];
			gse::memcpy(vel, src + bo_velocity);
			if (!is_finite_vec3(vel)) continue;

			gse::memcpy(dst + bo_position, pos);
			gse::memcpy(dst + bo_velocity, vel);
			gse::memcpy(dst + bo_orientation, orient);
			gse::memcpy(dst + bo_angular_velocity, src + bo_angular_velocity, sizeof(float) * 3);
			gse::memcpy(dst + bo_sleep_counter, src + bo_sleep_counter, sizeof(std::uint32_t));

			float he[3];
			gse::memcpy(he, dst + bo_half_extents);
			const quat q(orient[0], orient[1], orient[2], orient[3]);
			const auto rot = mat3_cast(q);
			float aabb_he[3];
			for (int a = 0; a < 3; ++a) {
				aabb_he[a] = std::abs(rot[0][a]) * he[0] + std::abs(rot[1][a]) * he[1] + std::abs(rot[2][a]) * he[2];
			}
			float amin[3] = { pos[0] - aabb_he[0], pos[1] - aabb_he[1], pos[2] - aabb_he[2] };
			float amax[3] = { pos[0] + aabb_he[0], pos[1] + aabb_he[1], pos[2] + aabb_he[2] };
			gse::memcpy(dst + bo_aabb_min, amin);
			gse::memcpy(dst + bo_aabb_max, amax);
		}
	}

	gse::memcpy(m_body_buffer.allocation.mapped(), m_upload_body_data);
	gse::memcpy(m_motor_buffer.allocation.mapped(), m_upload_motor_data);
	gse::memcpy(m_color_buffer.allocation.mapped(), m_upload_color_data);
	gse::memcpy(m_body_contact_map_buffer.allocation.mapped(), m_upload_body_contact_map);
	gse::memcpy(m_collision_state_buffer.allocation.mapped(), m_upload_collision_state);

	if (!m_upload_warm_start_data.empty()) {
		gse::memcpy(
			m_warm_start_buffer.allocation.mapped(),
			m_upload_warm_start_data
		);
	}

	if (!m_upload_joint_data.empty()) {
		gse::memcpy(
			m_joint_buffer.allocation.mapped(),
			m_upload_joint_data
		);
	}
}

auto gse::vbd::gpu_solver::stage_readback() -> void {
	if (!m_readback_pending) return;

	if (!m_first_submit) {
		static_cast<void>(m_compute.device->waitForFences(*m_compute.fence, vk::True, std::numeric_limits<std::uint64_t>::max()));
	}

	auto& info = m_readback_info;

	if (info.body_count == 0) {
		info = {};
		m_readback_pending = false;
		return;
	}

	const auto* rb = m_readback_buffer.allocation.mapped();

	m_staged_body_count = info.body_count;
	m_latest_gpu_body_data.assign(rb, rb + info.body_count * m_body_layout.stride);

	const vk::DeviceSize contact_region_offset = max_bodies * m_body_layout.stride;
	const vk::DeviceSize state_offset = contact_region_offset + max_contacts * m_contact_layout.stride;

	std::array<std::uint32_t, collision_state_uints> staged_collision_state{};
	gse::memcpy(
		reinterpret_cast<std::byte*>(staged_collision_state.data()),
		rb + state_offset,
		collision_state_uints * sizeof(std::uint32_t)
	);

	const std::uint32_t gpu_contact_count = staged_collision_state[0];
	m_staged_contact_count = std::min(gpu_contact_count, max_contacts);
	m_staged_color_count = std::min(staged_collision_state[1], max_colors);

	const vk::DeviceSize contact_data_size = m_staged_contact_count * m_contact_layout.stride;
	m_staged_contact_data.assign(rb + contact_region_offset, rb + contact_region_offset + contact_data_size);

	const vk::DeviceSize joint_region_offset = state_offset + collision_state_uints * sizeof(std::uint32_t);
	m_staged_joint_count = info.joint_count;
	if (m_staged_joint_count > 0) {
		const vk::DeviceSize joint_data_size = m_staged_joint_count * m_joint_layout.stride;
		m_staged_joint_data.assign(rb + joint_region_offset, rb + joint_region_offset + joint_data_size);
	} else {
		m_staged_joint_data.clear();
	}

	info = {};

	m_staged_valid = true;
	m_readback_pending = false;

	m_latest_gpu_body_count = m_staged_body_count;
}

auto gse::vbd::gpu_solver::readback(const std::span<body_state> bodies, std::vector<contact_readback_entry>& contacts_out, const std::span<joint_constraint> joints_out) -> void {
	if (!m_staged_valid) return;

	auto is_finite_vec3 = [](const float* v) {
		return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
	};

	const auto& [
		bo_position, bo_predicted_position, bo_inertia_target, bo_old_position,
		bo_velocity, bo_orientation, bo_predicted_orientation,
		bo_angular_inertia_target, bo_old_orientation, bo_angular_velocity,
		bo_motor_target, bo_mass, bo_flags, bo_sleep_counter, bo_accel_weight,
		bo_inv_inertia_col0, bo_inv_inertia_col1, bo_inv_inertia_col2,
		bo_half_extents, bo_aabb_min, bo_aabb_max
	] = m_bo;
	const std::uint32_t count = std::min(m_staged_body_count, static_cast<std::uint32_t>(bodies.size()));
	for (std::uint32_t i = 0; i < count; ++i) {
		const auto* elem = m_latest_gpu_body_data.data() + i * m_body_layout.stride;
		auto& b = bodies[i];

		float orient[4];
		gse::memcpy(orient, elem + bo_orientation);
		const float q_len_sq = orient[0] * orient[0] + orient[1] * orient[1] + orient[2] * orient[2] + orient[3] * orient[3];
		if (q_len_sq < 0.5f) continue;

		float pos[3];
		gse::memcpy(pos, elem + bo_position);
		if (!is_finite_vec3(pos)) continue;

		float vel[3];
		gse::memcpy(vel, elem + bo_velocity);

		b.position = unitless::vec3(pos[0], pos[1], pos[2]) * meters(1.f);

		if (is_finite_vec3(vel)) {
			b.body_velocity = unitless::vec3(vel[0], vel[1], vel[2]) * meters_per_second(1.f);
		} else {
			b.body_velocity = {};
		}

		gse::memcpy(reinterpret_cast<std::byte*>(&b.orientation), orient);

		float av[3];
		gse::memcpy(av, elem + bo_angular_velocity);
		if (is_finite_vec3(av)) {
			b.body_angular_velocity = unitless::vec3(av[0], av[1], av[2]) * radians_per_second(1.f);
		} else {
			b.body_angular_velocity = {};
		}

		gse::memcpy(b.sleep_counter, elem + bo_sleep_counter);
	}

	const auto& [
		co_body_a, co_body_b, co_feature_key_hi, co_feature_key_lo,
		co_sticking, co_normal, co_tangent_u, co_tangent_v,
		co_r_a, co_r_b, co_c0_friction, co_lambda, co_penalty
	] = m_co;
	contacts_out.clear();
	contacts_out.reserve(m_staged_contact_count);
	for (std::uint32_t i = 0; i < m_staged_contact_count; ++i) {
		const auto* elem = m_staged_contact_data.data() + i * m_contact_layout.stride;

		contact_readback_entry entry;
		gse::memcpy(entry.body_a, elem + co_body_a);
		gse::memcpy(entry.body_b, elem + co_body_b);
		std::uint32_t feature_hi = 0;
		std::uint32_t feature_lo = 0;
		gse::memcpy(feature_hi, elem + co_feature_key_hi);
		gse::memcpy(feature_lo, elem + co_feature_key_lo);
		entry.feature_key =
			(static_cast<std::uint64_t>(feature_hi) << 32) |
			static_cast<std::uint64_t>(feature_lo);

		std::uint32_t sticking = 0;
		gse::memcpy(sticking, elem + co_sticking);
		entry.sticking = sticking != 0;

		gse::memcpy(entry.normal, elem + co_normal);
		gse::memcpy(entry.tangent_u, elem + co_tangent_u);
		gse::memcpy(entry.tangent_v, elem + co_tangent_v);

		unitless::vec3 local_anchor_a{};
		gse::memcpy(local_anchor_a, elem + co_r_a);
		entry.local_anchor_a = local_anchor_a * meters(1.f);

		unitless::vec3 local_anchor_b{};
		gse::memcpy(local_anchor_b, elem + co_r_b);
		entry.local_anchor_b = local_anchor_b * meters(1.f);

		float c0_friction[4] = {};
		gse::memcpy(c0_friction, elem + co_c0_friction);
		entry.c0 = { meters(c0_friction[0]), meters(c0_friction[1]), meters(c0_friction[2]) };
		entry.friction_coeff = c0_friction[3];

		unitless::vec3 lambda_raw{};
		gse::memcpy(lambda_raw, elem + co_lambda);
		entry.lambda = { newtons(lambda_raw[0]), newtons(lambda_raw[1]), newtons(lambda_raw[2]) };

		gse::memcpy(entry.penalty, elem + co_penalty);

		contacts_out.push_back(entry);
	}

	const auto& [
		jo_body_a, jo_body_b, jo_type, jo_limits_enabled,
		jo_local_anchor_a, jo_local_anchor_b, jo_local_axis_a, jo_local_axis_b,
		jo_target_distance, jo_limit_lower, jo_limit_upper,
		jo_rest_orientation,
		jo_pos_lambda, jo_pos_penalty, jo_ang_lambda, jo_ang_penalty,
		jo_limit_lambda, jo_limit_penalty,
		jo_pos_c0, jo_ang_c0, jo_limit_c0
	] = m_jo;
	const std::uint32_t jcount = std::min(m_staged_joint_count, static_cast<std::uint32_t>(joints_out.size()));
	for (std::uint32_t i = 0; i < jcount; ++i) {
		const auto* elem = m_staged_joint_data.data() + i * m_joint_layout.stride;
		auto& jout = joints_out[i];

		unitless::vec3 pl_raw{};
		gse::memcpy(pl_raw, elem + jo_pos_lambda);
		jout.pos_lambda = { newtons(pl_raw[0]), newtons(pl_raw[1]), newtons(pl_raw[2]) };

		gse::memcpy(jout.pos_penalty, elem + jo_pos_penalty);

		unitless::vec3 al_raw{};
		gse::memcpy(al_raw, elem + jo_ang_lambda);
		jout.ang_lambda = { newton_meters(al_raw[0]), newton_meters(al_raw[1]), newton_meters(al_raw[2]) };

		unitless::vec3 ap_raw{};
		gse::memcpy(ap_raw, elem + jo_ang_penalty);
		jout.ang_penalty = { newton_meters_per_radian(ap_raw[0]), newton_meters_per_radian(ap_raw[1]), newton_meters_per_radian(ap_raw[2]) };

		float ll_val = 0.f;
		gse::memcpy(ll_val, elem + jo_limit_lambda);
		jout.limit_lambda = newton_meters(ll_val);

		float lp_val = 0.f;
		gse::memcpy(lp_val, elem + jo_limit_penalty);
		jout.limit_penalty = newton_meters_per_radian(lp_val);
	}

	m_staged_valid = false;

}

auto gse::vbd::gpu_solver::has_readback_data() const -> bool {
	return m_staged_valid;
}

auto gse::vbd::gpu_solver::pending_dispatch() const -> bool {
	return m_pending_dispatch;
}

auto gse::vbd::gpu_solver::ready_to_dispatch() const -> bool {
	return !m_readback_pending;
}

auto gse::vbd::gpu_solver::mark_dispatched() -> void {
	m_readback_info = { m_body_count, m_contact_count, m_joint_count };
	m_pending_dispatch = false;
	m_readback_pending = true;
	m_frame_count++;
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

auto gse::vbd::gpu_solver::joint_count() const -> std::uint32_t {
	return m_joint_count;
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

auto gse::vbd::gpu_solver::solve_time() const -> time_step {
	return milliseconds(m_compute.solve_ms);
}

auto gse::vbd::gpu_solver::initialize_compute(gpu::context& ctx) -> void {
	m_compute.predict = ctx.get<shader>("Shaders/Compute/vbd_predict");
	m_compute.solve_color = ctx.get<shader>("Shaders/Compute/vbd_solve_color");
	m_compute.update_lambda = ctx.get<shader>("Shaders/Compute/vbd_update_lambda");
	m_compute.derive_velocities = ctx.get<shader>("Shaders/Compute/vbd_derive_velocities");
	m_compute.finalize = ctx.get<shader>("Shaders/Compute/vbd_finalize");
	m_compute.collision_reset = ctx.get<shader>("Shaders/Compute/collision_reset");
	m_compute.collision_broad_phase = ctx.get<shader>("Shaders/Compute/collision_broad_phase");
	m_compute.collision_narrow_phase = ctx.get<shader>("Shaders/Compute/collision_narrow_phase");
	m_compute.collision_build_adjacency = ctx.get<shader>("Shaders/Compute/collision_build_adjacency");
	m_compute.update_joint_lambda = ctx.get<shader>("Shaders/Compute/vbd_update_joint_lambda");

	ctx.instantly_load(m_compute.predict);
	ctx.instantly_load(m_compute.solve_color);
	ctx.instantly_load(m_compute.update_lambda);
	ctx.instantly_load(m_compute.derive_velocities);
	ctx.instantly_load(m_compute.finalize);
	ctx.instantly_load(m_compute.collision_reset);
	ctx.instantly_load(m_compute.collision_broad_phase);
	ctx.instantly_load(m_compute.collision_narrow_phase);
	ctx.instantly_load(m_compute.collision_build_adjacency);
	ctx.instantly_load(m_compute.update_joint_lambda);

	auto& config = ctx.config();

	auto vbd_layouts = m_compute.predict->layouts();
	std::vector vbd_ranges = { m_compute.predict->push_constant_range("vbd_push_constants") };

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
	m_compute.finalize_pipeline = create_pipeline(m_compute.finalize);
	m_compute.collision_reset_pipeline = create_pipeline(m_compute.collision_reset);
	m_compute.collision_broad_phase_pipeline = create_pipeline(m_compute.collision_broad_phase);
	m_compute.collision_narrow_phase_pipeline = create_pipeline(m_compute.collision_narrow_phase);
	m_compute.collision_build_adjacency_pipeline = create_pipeline(m_compute.collision_build_adjacency);
	m_compute.update_joint_lambda_pipeline = create_pipeline(m_compute.update_joint_lambda);

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
	m_warm_start_layout = extract_layout(m_compute.predict, "warm_starts");
	m_joint_layout = extract_layout(m_compute.predict, "joint_data");
	{
		const auto& o = m_body_layout.offsets;
		m_bo.position = o.at("position");
		m_bo.predicted_position = o.at("predicted_position");
		m_bo.inertia_target = o.at("inertia_target");
		m_bo.old_position = o.at("old_position");
		m_bo.velocity = o.at("velocity");
		m_bo.orientation = o.at("orientation");
		m_bo.predicted_orientation = o.at("predicted_orientation");
		m_bo.angular_inertia_target = o.at("angular_inertia_target");
		m_bo.old_orientation = o.at("old_orientation");
		m_bo.angular_velocity = o.at("angular_velocity");
		m_bo.motor_target = o.at("motor_target");
		m_bo.mass = o.at("mass");
		m_bo.flags = o.at("flags");
		m_bo.sleep_counter = o.at("sleep_counter");
		m_bo.accel_weight = o.at("accel_weight");
		m_bo.inv_inertia_col0 = o.at("inv_inertia_col0");
		m_bo.inv_inertia_col1 = o.at("inv_inertia_col1");
		m_bo.inv_inertia_col2 = o.at("inv_inertia_col2");
		m_bo.half_extents = o.at("half_extents");
		m_bo.aabb_min = o.at("aabb_min");
		m_bo.aabb_max = o.at("aabb_max");
	}

	{
		const auto& o = m_contact_layout.offsets;
		m_co.body_a = o.at("body_a");
		m_co.body_b = o.at("body_b");
		m_co.feature_key_hi = o.at("feature_key_hi");
		m_co.feature_key_lo = o.at("feature_key_lo");
		m_co.sticking = o.at("sticking");
		m_co.normal = o.at("normal");
		m_co.tangent_u = o.at("tangent_u");
		m_co.tangent_v = o.at("tangent_v");
		m_co.r_a = o.at("r_a");
		m_co.r_b = o.at("r_b");
		m_co.c0_friction = o.at("c0_friction");
		m_co.lambda = o.at("lambda");
		m_co.penalty = o.at("penalty");
	}

	{
		const auto& o = m_motor_layout.offsets;
		m_mo.body_index = o.at("body_index");
		m_mo.compliance = o.at("compliance");
		m_mo.max_force = o.at("max_force");
		m_mo.horizontal_only = o.at("horizontal_only");
		m_mo.target_velocity = o.at("target_velocity");
	}

	{
		const auto& o = m_warm_start_layout.offsets;
		m_wo.body_a = o.at("body_a");
		m_wo.body_b = o.at("body_b");
		m_wo.feature_key_hi = o.at("feature_key_hi");
		m_wo.feature_key_lo = o.at("feature_key_lo");
		m_wo.sticking = o.at("sticking");
		m_wo.normal = o.at("normal");
		m_wo.tangent_u = o.at("tangent_u");
		m_wo.tangent_v = o.at("tangent_v");
		m_wo.local_anchor_a = o.at("local_anchor_a");
		m_wo.local_anchor_b = o.at("local_anchor_b");
		m_wo.lambda = o.at("lambda");
		m_wo.penalty = o.at("penalty");
	}

	{
		const auto& o = m_joint_layout.offsets;
		m_jo.body_a = o.at("body_a");
		m_jo.body_b = o.at("body_b");
		m_jo.type = o.at("type");
		m_jo.limits_enabled = o.at("limits_enabled");
		m_jo.local_anchor_a = o.at("local_anchor_a");
		m_jo.local_anchor_b = o.at("local_anchor_b");
		m_jo.local_axis_a = o.at("local_axis_a");
		m_jo.local_axis_b = o.at("local_axis_b");
		m_jo.target_distance = o.at("target_distance");
		m_jo.limit_lower = o.at("limit_lower");
		m_jo.limit_upper = o.at("limit_upper");
		m_jo.rest_orientation = o.at("rest_orientation");
		m_jo.pos_lambda = o.at("pos_lambda");
		m_jo.pos_penalty = o.at("pos_penalty");
		m_jo.ang_lambda = o.at("ang_lambda");
		m_jo.ang_penalty = o.at("ang_penalty");
		m_jo.limit_lambda = o.at("limit_lambda");
		m_jo.limit_penalty = o.at("limit_penalty");
		m_jo.pos_c0 = o.at("pos_c0");
		m_jo.ang_c0 = o.at("ang_c0");
		m_jo.limit_c0 = o.at("limit_c0");
	}

	m_compute.query_pool = config.device_config().device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 8
	});
	static_cast<vk::Device>(*config.device_config().device).resetQueryPool(*m_compute.query_pool, 0, 8);

	m_compute.timestamp_period = config.device_config().physical_device.getProperties().limits.timestampPeriod;

	create_buffers(config.allocator());

	m_compute.command_pool = config.device_config().device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = config.queue_config().compute_family_index
	});

	m_compute.command_buffer = std::move(config.device_config().device.allocateCommandBuffers({
		.commandPool = *m_compute.command_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	}).front());

	m_compute.fence = config.device_config().device.createFence({
		.flags = vk::FenceCreateFlagBits::eSignaled
	});

	m_compute.queue = &config.queue_config().compute;
	m_compute.device = &config.device_config().device;

	m_compute.initialized = true;
}

auto gse::vbd::gpu_solver::dispatch_compute(vulkan::config& config) -> void {
	const vk::Device device = *config.device_config().device;

	if (!m_compute.descriptors_initialized) {
		m_compute.descriptor_set = m_compute.predict->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		const vk::DescriptorBufferInfo body_info{
			.buffer = m_body_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo contact_info{
			.buffer = m_contact_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo motor_info{
			.buffer = m_motor_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo color_info{
			.buffer = m_color_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo map_info{
			.buffer = m_body_contact_map_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo solve_info{
			.buffer = m_solve_state_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo collision_pair_info{
			.buffer = m_collision_pair_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo collision_state_info{
			.buffer = m_collision_state_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo warm_start_info{
			.buffer = m_warm_start_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const vk::DescriptorBufferInfo joint_info{
			.buffer = m_joint_buffer.buffer,
			.offset = 0,
			.range = vk::WholeSize
		};

		auto& ds = m_compute.descriptor_set;
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
			},
			vk::WriteDescriptorSet{
				.dstSet = *ds,
				.dstBinding = 9,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &joint_info
			}
		};

		config.device_config().device.updateDescriptorSets(writes, nullptr);

		m_compute.descriptors_initialized = true;
	}

	if (m_body_count == 0) return;

	if (m_frame_count >= 2) {
		std::array<std::uint64_t, 2> timestamps{};
		auto result = device.getQueryPoolResults(
			*m_compute.query_pool, 0, 2,
			sizeof(timestamps), timestamps.data(), sizeof(std::uint64_t),
			vk::QueryResultFlagBits::e64
		);
		if (result == vk::Result::eSuccess) {
			m_compute.solve_ms = static_cast<float>(timestamps[1] - timestamps[0]) * m_compute.timestamp_period * 1e-6f;
		}
	}

	m_compute.command_buffer.reset({});
	m_compute.command_buffer.begin({});

	const vk::CommandBuffer command = *m_compute.command_buffer;

	constexpr std::uint32_t query_base = 0;
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

	const vk::DescriptorSet sets[] = { *m_compute.descriptor_set };

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
	const time_step sub_dt = m_dt / static_cast<float>(total);
	const float h_squared = sub_dt.as<seconds>() * sub_dt.as<seconds>();

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) {
		return (a + b - 1) / b;
	};

	auto bind_and_push = [&](const resource::handle<shader>& sh, const vk::raii::Pipeline& pipeline, std::uint32_t color_offset, std::uint32_t color_count, std::uint32_t substep, std::uint32_t iteration, float current_alpha) {
		command.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
		command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_compute.pipeline_layout, 0, { 1, sets }, {});
		sh->push(command, *m_compute.pipeline_layout, "vbd_push_constants",
			"body_count", m_body_count,
			"contact_count", max_contacts,
			"motor_count", m_motor_count,
			"color_offset", color_offset,
			"color_count", color_count,
			"warm_start_count", m_warm_start_count,
			"post_stabilize", cfg.post_stabilize ? 1u : 0u,
			"joint_count", m_joint_count,
			"h_squared", h_squared,
			"dt", sub_dt.as<seconds>(),
			"beta", cfg.beta,
			"linear_damping", 0.0f,
			"velocity_sleep_threshold", cfg.velocity_sleep_threshold.as<meters_per_second>(),
			"current_alpha", current_alpha,
			"collision_margin", cfg.collision_margin.as<meters>(),
			"friction_coefficient", cfg.friction_coefficient,
			"penalty_min", cfg.penalty_min,
			"penalty_max", cfg.penalty_max,
			"gamma", cfg.gamma,
			"solver_alpha", cfg.alpha,
			"speculative_margin", cfg.speculative_margin.as<meters>(),
			"stick_threshold", cfg.stick_threshold.as<meters>(),
			"substep", substep,
			"iteration", iteration
		);
	};

	const std::uint32_t n = m_body_count;
	const std::uint32_t total_pairs = n * (n - 1) / 2;

	bind_and_push(m_compute.collision_reset, m_compute.collision_reset_pipeline, 0u, 0u, 0u, 0u, 0.f);
	command.dispatch(ceil_div(m_body_count, workgroup_size), 1, 1);
	command.pipelineBarrier2(compute_dep);

	bind_and_push(m_compute.collision_broad_phase, m_compute.collision_broad_phase_pipeline, 0u, 0u, 0u, 0u, 0.f);
	command.dispatch(ceil_div(total_pairs, workgroup_size), 1, 1);
	command.pipelineBarrier2(compute_dep);

	bind_and_push(m_compute.collision_narrow_phase, m_compute.collision_narrow_phase_pipeline, 0u, 0u, 0u, 0u, 0.f);
	command.dispatch(ceil_div(max_collision_pairs, workgroup_size), 1, 1);
	command.pipelineBarrier2(compute_dep);

	bind_and_push(m_compute.collision_build_adjacency, m_compute.collision_build_adjacency_pipeline, 0u, 0u, 0u, 0u, 0.f);
	command.dispatch(1, 1, 1);
	command.pipelineBarrier2(compute_dep);

	const std::uint32_t body_workgroups = ceil_div(m_body_count, workgroup_size);
	constexpr std::uint32_t num_colors = max_colors;
	const std::uint32_t total_iterations =
		cfg.iterations +
		(cfg.post_stabilize ? 1u : 0u);

	for (std::uint32_t sub = 0; sub < total; ++sub) {
		bind_and_push(m_compute.predict, m_compute.predict_pipeline, 0u, 0u, sub, 0u, 0.f);
		command.dispatch(body_workgroups, 1, 1);
		command.pipelineBarrier2(compute_dep);

		for (std::uint32_t iterations = 0; iterations < total_iterations; ++iterations) {
			const float current_alpha =
				cfg.post_stabilize
					? (iterations < cfg.iterations ? 1.f : 0.f)
					: cfg.alpha;

			bind_and_push(m_compute.solve_color, m_compute.solve_color_pipeline, 0u, num_colors, sub, iterations, current_alpha);
			command.dispatch(body_workgroups, 1, 1);
			command.pipelineBarrier2(compute_dep);

			if (iterations < cfg.iterations) {
				bind_and_push(m_compute.update_lambda, m_compute.update_lambda_pipeline, 0u, 0u, sub, iterations, current_alpha);
				command.dispatch(ceil_div(max_contacts, workgroup_size), 1, 1);
				command.pipelineBarrier2(compute_dep);
			}

			if (cfg.iterations > 0 && iterations == cfg.iterations - 1u) {
				if (m_joint_count > 0) {
					bind_and_push(m_compute.update_joint_lambda, m_compute.update_joint_lambda_pipeline, 0u, 0u, sub, iterations, current_alpha);
					command.dispatch(ceil_div(m_joint_count, workgroup_size), 1, 1);
					command.pipelineBarrier2(compute_dep);
				}

				bind_and_push(m_compute.derive_velocities, m_compute.derive_velocities_pipeline, 0u, 0u, sub, iterations, current_alpha);
				command.dispatch(body_workgroups, 1, 1);
				command.pipelineBarrier2(compute_dep);
			}
		}

		if (cfg.iterations == 0u) {
			bind_and_push(m_compute.derive_velocities, m_compute.derive_velocities_pipeline, 0u, 0u, sub, 0u, 0.f);
			command.dispatch(body_workgroups, 1, 1);
			command.pipelineBarrier2(compute_dep);
		}

		bind_and_push(m_compute.finalize, m_compute.finalize_pipeline, 0u, 0u, sub, 0u, 0.f);
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
		m_body_buffer.buffer,
		m_readback_buffer.buffer,
		vk::BufferCopy{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = body_copy_size
		}
	);

	const vk::DeviceSize contact_dst_base = max_bodies * m_body_layout.stride;
	const vk::DeviceSize contact_copy_size = max_contacts * m_contact_layout.stride;

	command.copyBuffer(
		m_contact_buffer.buffer,
		m_readback_buffer.buffer,
		vk::BufferCopy{
			.srcOffset = 0,
			.dstOffset = contact_dst_base,
			.size = contact_copy_size
		}
	);

	const vk::DeviceSize count_dst = contact_dst_base + contact_copy_size;
	command.copyBuffer(
		m_collision_state_buffer.buffer,
		m_readback_buffer.buffer,
		vk::BufferCopy{
			.srcOffset = 0,
			.dstOffset = count_dst,
			.size = collision_state_uints * sizeof(std::uint32_t)
		}
	);

	if (m_joint_count > 0) {
		const vk::DeviceSize joint_dst = count_dst + collision_state_uints * sizeof(std::uint32_t);
		const vk::DeviceSize joint_copy_size = m_joint_count * m_joint_layout.stride;
		command.copyBuffer(
			m_joint_buffer.buffer,
			m_readback_buffer.buffer,
			vk::BufferCopy{
				.srcOffset = 0,
				.dstOffset = joint_dst,
				.size = joint_copy_size
			}
		);
	}

	constexpr vk::MemoryBarrier2 compute_to_host{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eHost,
		.dstAccessMask = vk::AccessFlagBits2::eHostRead
	};
	command.pipelineBarrier2({ .memoryBarrierCount = 1, .pMemoryBarriers = &compute_to_host });

	m_compute.command_buffer.end();

	const vk::CommandBuffer submit_cmd = *m_compute.command_buffer;
	const vk::SubmitInfo submit_info{
		.commandBufferCount = 1,
		.pCommandBuffers = &submit_cmd
	};
	device.resetFences(*m_compute.fence);
	m_compute.queue->submit(submit_info, *m_compute.fence);

	mark_dispatched();
	m_first_submit = false;
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
	return m_co.lambda;
}

