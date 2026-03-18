export module gse.physics:vbd_gpu_solver;

import std;

import gse.assert;
import gse.utility;
import gse.log;

import gse.platform;

import :vbd_constraints;
import :vbd_solver;

#ifndef GSE_GPU_COMPARE
#define GSE_GPU_COMPARE 0
#endif

export namespace gse::vbd {
	constexpr std::uint32_t max_bodies = 500;
	constexpr std::uint32_t max_contacts = 2000;
	constexpr std::uint32_t max_motors = 16;
	constexpr std::uint32_t max_colors = 32;
	constexpr std::uint32_t max_collision_pairs = 8192;
	constexpr std::uint32_t workgroup_size = 64;
	constexpr std::uint32_t collision_state_header_uints = 8;
	constexpr std::uint32_t narrow_phase_debug_record_uints = 8;
	constexpr std::uint32_t max_narrow_phase_debug_records = 32;

	constexpr std::uint32_t solve_state_float4s_per_body = 11;
	constexpr std::uint32_t collision_state_uints =
		collision_state_header_uints +
		narrow_phase_debug_record_uints * max_narrow_phase_debug_records;

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
		unitless::vec3 normal = {};
		unitless::vec3 tangent_u = {};
		unitless::vec3 tangent_v = {};
		vec3<length> local_anchor_a = {};
		vec3<length> local_anchor_b = {};
		std::array<float, 3> lambda = {};
		std::array<float, 3> penalty = {};
	};

	struct contact_readback_entry {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;
		bool sticking = false;
		unitless::vec3 normal = {};
		unitless::vec3 tangent_u = {};
		unitless::vec3 tangent_v = {};
		vec3<length> local_anchor_a = {};
		vec3<length> local_anchor_b = {};
		std::array<float, 3> c0 = {};
		std::array<float, 3> lambda = {};
		std::array<float, 3> penalty = {};
		float friction_coeff = 0.f;
	};

#if GSE_GPU_COMPARE
	struct narrow_phase_debug_entry {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		bool clipped_valid = false;
		bool used_fallback = false;
		bool speculative = false;
		bool reference_is_a = false;
		std::uint32_t clipped_vertices = 0;
		std::uint32_t raw_candidates = 0;
		std::uint32_t unique_candidates = 0;
		std::uint32_t result_count = 0;
		std::uint32_t reference_face = 0;
		std::uint32_t incident_face = 0;
	};
#endif

	struct gpu_debug_config {
		bool enable_warm_start = true;
		bool enable_solve_iterations = true;
		bool log_diagnostics = true;
		std::uint32_t log_interval = 60;
	};

	class gpu_solver {
	public:
		gpu_debug_config debug;

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
			std::vector<contact_readback_entry>& contacts_out
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

		auto body_count(
		) const -> std::uint32_t;

		auto contact_count(
		) const -> std::uint32_t;

		auto motor_count(
		) const -> std::uint32_t;

		auto solver_cfg(
		) const -> const solver_config&;

		auto dt(
		) const -> time_step;

		auto frame_count(
		) const -> std::uint32_t;

		auto solve_time(
		) const -> time_step;

#if GSE_GPU_COMPARE
		auto narrow_phase_debug_entries(
		) const -> std::span<const narrow_phase_debug_entry>;
#endif

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

			vk::raii::Pipeline predict_pipeline = nullptr;
			vk::raii::Pipeline solve_color_pipeline = nullptr;
			vk::raii::Pipeline update_lambda_pipeline = nullptr;
			vk::raii::Pipeline derive_velocities_pipeline = nullptr;
			vk::raii::Pipeline finalize_pipeline = nullptr;
			vk::raii::Pipeline collision_reset_pipeline = nullptr;
			vk::raii::Pipeline collision_broad_phase_pipeline = nullptr;
			vk::raii::Pipeline collision_narrow_phase_pipeline = nullptr;
			vk::raii::Pipeline collision_build_adjacency_pipeline = nullptr;

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

		std::uint32_t m_steps = 1;
		solver_config m_solver_cfg;
		time_step m_dt{};

		buffer_layout m_body_layout;
		buffer_layout m_contact_layout;
		buffer_layout m_motor_layout;
		buffer_layout m_warm_start_layout;

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

		struct readback_frame_info {
			std::uint32_t body_count = 0;
			std::uint32_t contact_count = 0;
		};
		readback_frame_info m_readback_info{};

		std::vector<std::byte> m_upload_body_data;
		std::vector<std::byte> m_upload_motor_data;
		std::vector<std::byte> m_upload_warm_start_data;
		std::vector<std::uint32_t> m_upload_color_data;
		std::vector<std::uint32_t> m_upload_body_contact_map;
		std::vector<std::uint32_t> m_upload_collision_state;
		std::vector<std::uint8_t> m_upload_authoritative_bodies;

		std::vector<std::byte> m_staged_body_data;
		std::vector<std::byte> m_staged_contact_data;
#if GSE_GPU_COMPARE
		std::vector<narrow_phase_debug_entry> m_staged_narrow_phase_debug;
#endif
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;
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
	});

	m_contact_buffer = alloc.create_buffer({
		.size = max_contacts * m_contact_layout.stride,
		.usage = usage
	});

	m_motor_buffer = alloc.create_buffer({
		.size = max_motors * m_motor_layout.stride,
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
		.size = max_bodies * solve_state_float4s_per_body * sizeof(float) * 4,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	const vk::DeviceSize collision_pair_size = sizeof(std::uint32_t) + max_collision_pairs * 2 * sizeof(std::uint32_t);
	m_collision_pair_buffer = alloc.create_buffer({
		.size = collision_pair_size,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	const vk::DeviceSize collision_state_size = collision_state_uints * sizeof(std::uint32_t);
	m_collision_state_buffer = alloc.create_buffer({
		.size = collision_state_size,
		.usage = usage
	});

	const vk::DeviceSize warm_start_size = max_contacts * m_warm_start_layout.stride;
	m_warm_start_buffer = alloc.create_buffer({
		.size = std::max(warm_start_size, static_cast<vk::DeviceSize>(16)),
		.usage = vk::BufferUsageFlagBits::eStorageBuffer
	});

	const vk::DeviceSize readback_size =
		max_bodies * m_body_layout.stride +
		max_contacts * m_contact_layout.stride +
		collision_state_uints * sizeof(std::uint32_t);

	m_readback_buffer = alloc.create_buffer({
		.size = readback_size,
		.usage = readback_usage
	});
	std::memset(m_readback_buffer.allocation.mapped(), 0, m_readback_buffer.allocation.size());

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(
	const std::span<const body_state> bodies,
	const std::span<const collision_body_data> collision_data,
	const std::span<const float> accel_weights,
	const std::span<const velocity_motor_constraint> motors,
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

	const auto& bo = m_body_layout.offsets;
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
			gse::memcpy(elem + bo.at("position"), &pos);

			const auto pp = predicted_position.as<meters>();
			gse::memcpy(elem + bo.at("predicted_position"), &pp);

			const auto it_pos = inertia_target.as<meters>();
			gse::memcpy(elem + bo.at("inertia_target"), &it_pos);

			const auto op = initial_position.as<meters>();
			gse::memcpy(elem + bo.at("old_position"), &op);

			const auto vel = body_velocity.as<meters_per_second>();
			gse::memcpy(elem + bo.at("velocity"), &vel);

			gse::memcpy(elem + bo.at("orientation"), &orientation);

			gse::memcpy(elem + bo.at("predicted_orientation"), &predicted_orientation);

			gse::memcpy(elem + bo.at("angular_inertia_target"), &angular_inertia_target);

			gse::memcpy(elem + bo.at("old_orientation"), &initial_orientation);

			const auto av = body_angular_velocity.as<radians_per_second>();
			gse::memcpy(elem + bo.at("angular_velocity"), &av);
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

		const float accel_weight =
			i < accel_weights.size()
				? accel_weights[i]
				: 0.f;
		gse::memcpy(elem + bo.at("accel_weight"), &accel_weight);

		const auto inv_i = mat3{ inv_inertia.data };
		gse::memcpy(elem + bo.at("inv_inertia_col0"), &inv_i[0]);
		gse::memcpy(elem + bo.at("inv_inertia_col1"), &inv_i[1]);
		gse::memcpy(elem + bo.at("inv_inertia_col2"), &inv_i[2]);

		if (i < collision_data.size()) {
			const auto& [half_extents, aabb_min, aabb_max] = collision_data[i];
			const auto he = half_extents.as<meters>();
			const auto amin = aabb_min.as<meters>();
			const auto amax = aabb_max.as<meters>();
			gse::memcpy(elem + bo.at("half_extents"), &he);
			gse::memcpy(elem + bo.at("aabb_min"), &amin);
			gse::memcpy(elem + bo.at("aabb_max"), &amax);
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
	m_upload_collision_state[1] = 0;

	if (debug.enable_warm_start) {
		m_warm_start_count = static_cast<std::uint32_t>(std::min(prev_contacts.size(), static_cast<std::size_t>(max_contacts)));
	} else {
		m_warm_start_count = 0;
	}

	const auto& wo = m_warm_start_layout.offsets;
	m_upload_warm_start_data.assign(m_warm_start_count * m_warm_start_layout.stride, std::byte{0});
	for (std::uint32_t i = 0; i < m_warm_start_count; ++i) {
		const auto& ws = prev_contacts[i];
		auto* elem = m_upload_warm_start_data.data() + i * m_warm_start_layout.stride;

		gse::memcpy(elem + wo.at("body_a"), &ws.body_a);
		gse::memcpy(elem + wo.at("body_b"), &ws.body_b);
		const auto feature_hi = static_cast<std::uint32_t>(ws.feature_key >> 32);
		const auto feature_lo = static_cast<std::uint32_t>(ws.feature_key & 0xFFFFFFFFu);
		gse::memcpy(elem + wo.at("feature_key_hi"), &feature_hi);
		gse::memcpy(elem + wo.at("feature_key_lo"), &feature_lo);

		const std::uint32_t sticking = ws.sticking ? 1u : 0u;
		gse::memcpy(elem + wo.at("sticking"), &sticking);

		const auto normal = ws.normal;
		gse::memcpy(elem + wo.at("normal"), &normal);

		const auto tangent_u = ws.tangent_u;
		gse::memcpy(elem + wo.at("tangent_u"), &tangent_u);

		const auto tangent_v = ws.tangent_v;
		gse::memcpy(elem + wo.at("tangent_v"), &tangent_v);

		const auto local_anchor_a = ws.local_anchor_a.as<meters>();
		gse::memcpy(elem + wo.at("local_anchor_a"), &local_anchor_a);

		const auto local_anchor_b = ws.local_anchor_b.as<meters>();
		gse::memcpy(elem + wo.at("local_anchor_b"), &local_anchor_b);

		gse::memcpy(elem + wo.at("lambda"), ws.lambda.data(), sizeof(float) * 3);
		gse::memcpy(elem + wo.at("penalty"), ws.penalty.data(), sizeof(float) * 3);
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::total_substeps() const -> std::uint32_t {
	return 1 * m_steps;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_pending_dispatch || m_body_count == 0) return;

	if (m_latest_gpu_body_count > 0 && !m_latest_gpu_body_data.empty() && !m_upload_body_data.empty()) {
		const auto& bo = m_body_layout.offsets;
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
			gse::memcpy(cpu_flags, dst + bo.at("flags"));
			if (cpu_flags & flag_locked) continue;

			float orient[4];
			gse::memcpy(orient, src + bo.at("orientation"));
			const float q_len_sq = orient[0] * orient[0] + orient[1] * orient[1] + orient[2] * orient[2] + orient[3] * orient[3];
			if (q_len_sq < 0.5f) continue;

			float pos[3];
			gse::memcpy(pos, src + bo.at("position"));
			if (!is_finite_vec3(pos)) continue;

			float vel[3];
			gse::memcpy(vel, src + bo.at("velocity"));
			if (!is_finite_vec3(vel)) continue;

			gse::memcpy(dst + bo.at("position"), pos);
			gse::memcpy(dst + bo.at("velocity"), vel);
			gse::memcpy(dst + bo.at("orientation"), orient);
			gse::memcpy(dst + bo.at("angular_velocity"), src + bo.at("angular_velocity"), sizeof(float) * 3);
			gse::memcpy(dst + bo.at("sleep_counter"), src + bo.at("sleep_counter"), sizeof(std::uint32_t));

			float he[3];
			gse::memcpy(he, dst + bo.at("half_extents"));
			const quat q(orient[0], orient[1], orient[2], orient[3]);
			const auto rot = mat3_cast(q);
			float aabb_he[3];
			for (int a = 0; a < 3; ++a) {
				aabb_he[a] = std::abs(rot[0][a]) * he[0] + std::abs(rot[1][a]) * he[1] + std::abs(rot[2][a]) * he[2];
			}
			float amin[3] = { pos[0] - aabb_he[0], pos[1] - aabb_he[1], pos[2] - aabb_he[2] };
			float amax[3] = { pos[0] + aabb_he[0], pos[1] + aabb_he[1], pos[2] + aabb_he[2] };
			gse::memcpy(dst + bo.at("aabb_min"), amin);
			gse::memcpy(dst + bo.at("aabb_max"), amax);
		}
	}

	if (!m_upload_body_data.empty() && m_body_count > 0) {
		float pos[3] = {}, vel[3] = {};
		gse::memcpy(pos, m_upload_body_data.data() + m_body_layout.offsets.at("position"));
		gse::memcpy(vel, m_upload_body_data.data() + m_body_layout.offsets.at("velocity"));
		log::println("[COMMIT] body0_pos=({:.3f},{:.3f},{:.3f}) body0_vel=({:.3f},{:.3f},{:.3f}) frame={} warm_starts={}",
			pos[0], pos[1], pos[2], vel[0], vel[1], vel[2], m_frame_count, m_warm_start_count);
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
}

auto gse::vbd::gpu_solver::stage_readback() -> void {
	if (!m_readback_pending) return;

	auto& info = m_readback_info;

	if (info.body_count == 0) {
		info = {};
		m_readback_pending = false;
		return;
	}

	const auto* rb = m_readback_buffer.allocation.mapped();

	m_staged_body_count = info.body_count;
	m_staged_body_data.assign(rb, rb + info.body_count * m_body_layout.stride);

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

#if GSE_GPU_COMPARE
	m_staged_narrow_phase_debug.clear();
	const std::uint32_t debug_count = std::min(staged_collision_state[2], max_narrow_phase_debug_records);
	m_staged_narrow_phase_debug.reserve(debug_count);
	for (std::uint32_t i = 0; i < debug_count; ++i) {
		const std::uint32_t base = collision_state_header_uints + i * narrow_phase_debug_record_uints;
		if (base + narrow_phase_debug_record_uints > staged_collision_state.size()) break;

		const std::uint32_t flags = staged_collision_state[base + 2];
		const std::uint32_t packed_faces = staged_collision_state[base + 7];
		m_staged_narrow_phase_debug.push_back(narrow_phase_debug_entry{
			.body_a = staged_collision_state[base + 0],
			.body_b = staged_collision_state[base + 1],
			.clipped_valid = (flags & 0x1u) != 0,
			.used_fallback = (flags & 0x2u) != 0,
			.speculative = (flags & 0x4u) != 0,
			.reference_is_a = (flags & 0x8u) != 0,
			.clipped_vertices = staged_collision_state[base + 3],
			.raw_candidates = staged_collision_state[base + 4],
			.unique_candidates = staged_collision_state[base + 5],
			.result_count = staged_collision_state[base + 6],
			.reference_face = packed_faces & 0xFFu,
			.incident_face = (packed_faces >> 8) & 0xFFu
		});
	}
#endif

	info = {};

	m_staged_valid = true;
	m_readback_pending = false;

	m_latest_gpu_body_data = m_staged_body_data;
	m_latest_gpu_body_count = m_staged_body_count;

	const auto& bo = m_body_layout.offsets;

	{
		const auto& bo2 = m_body_layout.offsets;
		float gpu_pos[3] = {}, gpu_vel[3] = {}, cpu_pos[3] = {};
		if (m_staged_body_count > 0) {
			gse::memcpy(gpu_pos, m_staged_body_data.data() + bo2.at("position"));
			gse::memcpy(gpu_vel, m_staged_body_data.data() + bo2.at("velocity"));
		}
		if (!m_upload_body_data.empty()) {
			gse::memcpy(cpu_pos, m_upload_body_data.data() + bo2.at("position"));
		}
		log::println("[STAGE_RB] gpu0=({:.3f},{:.3f},{:.3f}) vel0=({:.3f},{:.3f},{:.3f}) cpu0_after_patch=({:.3f},{:.3f},{:.3f}) contacts={}",
			gpu_pos[0], gpu_pos[1], gpu_pos[2], gpu_vel[0], gpu_vel[1], gpu_vel[2], cpu_pos[0], cpu_pos[1], cpu_pos[2], m_staged_contact_count);
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
			gse::memcpy(flags, elem + bo.at("flags"));
			if (flags & flag_locked) continue;

			float pos[3], vel[3], ang[3], orient[4];
			gse::memcpy(pos, elem + bo.at("position"));
			gse::memcpy(vel, elem + bo.at("velocity"));
			gse::memcpy(ang, elem + bo.at("angular_velocity"));
			gse::memcpy(orient, elem + bo.at("orientation"));

			const bool pos_nan = !std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2]);
			const bool vel_nan = !std::isfinite(vel[0]) || !std::isfinite(vel[1]) || !std::isfinite(vel[2]);
			if (pos_nan || vel_nan) { nan_count++; continue; }

			const float q_len_sq = orient[0]*orient[0] + orient[1]*orient[1] + orient[2]*orient[2] + orient[3]*orient[3];
			if (q_len_sq < 0.5f) bad_quat_count++;

			const float lin_spd = std::sqrt(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]);
			const float ang_spd = std::sqrt(ang[0]*ang[0] + ang[1]*ang[1] + ang[2]*ang[2]);

			if (lin_spd > max_lin_vel) { max_lin_vel = lin_spd; max_lin_body = i; }
			if (ang_spd > max_ang_vel) { max_ang_vel = ang_spd; max_ang_body = i; }
			if (pos[1] > max_pos_y) max_pos_y = pos[1];
			if (pos[1] < min_pos_y) min_pos_y = pos[1];
		}

		const bool trouble = nan_count > 0 || bad_quat_count > 0 || max_lin_vel > 20.f || max_ang_vel > 30.f;
		const bool periodic = m_frame_count % debug.log_interval == 0;

		if (trouble || periodic) {
			log::println("[GPU Phys] frame={} bodies={} contacts={} nan={} bad_q={} solve={:.2f}ms", m_frame_count, m_staged_body_count, m_staged_contact_count, nan_count, bad_quat_count, m_compute.solve_ms);
			log::println("  max_lin_vel={:.2f} (body {}) max_ang_vel={:.2f} (body {}) y_range=[{:.2f}, {:.2f}]", max_lin_vel, max_lin_body, max_ang_vel, max_ang_body, min_pos_y, max_pos_y);
		}

		if (trouble) {
			log::flush();
		}

		if (max_lin_vel > 20.f) {
			const auto* elem = m_staged_body_data.data() + max_lin_body * m_body_layout.stride;
			float pos[3], vel[3];
			gse::memcpy(pos, elem + bo.at("position"));
			gse::memcpy(vel, elem + bo.at("velocity"));
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
		gse::memcpy(orient, elem + bo.at("orientation"));
		const float q_len_sq = orient[0] * orient[0] + orient[1] * orient[1] + orient[2] * orient[2] + orient[3] * orient[3];
		if (q_len_sq < 0.5f) continue;

		float pos[3];
		gse::memcpy(pos, elem + bo.at("position"));
		if (!is_finite_vec3(pos)) continue;

		float vel[3];
		gse::memcpy(vel, elem + bo.at("velocity"));

		b.position = unitless::vec3(pos[0], pos[1], pos[2]) * meters(1.f);

		if (is_finite_vec3(vel)) {
			b.body_velocity = unitless::vec3(vel[0], vel[1], vel[2]) * meters_per_second(1.f);
		} else {
			b.body_velocity = {};
		}

		gse::memcpy(reinterpret_cast<std::byte*>(&b.orientation), orient);

		float av[3];
		gse::memcpy(av, elem + bo.at("angular_velocity"));
		if (is_finite_vec3(av)) {
			b.body_angular_velocity = unitless::vec3(av[0], av[1], av[2]) * radians_per_second(1.f);
		} else {
			b.body_angular_velocity = {};
		}

		gse::memcpy(b.sleep_counter, elem + bo.at("sleep_counter"));
	}

	const auto& co = m_contact_layout.offsets;
	contacts_out.clear();
	contacts_out.reserve(m_staged_contact_count);
	for (std::uint32_t i = 0; i < m_staged_contact_count; ++i) {
		const auto* elem = m_staged_contact_data.data() + i * m_contact_layout.stride;

		contact_readback_entry entry;
		gse::memcpy(entry.body_a, elem + co.at("body_a"));
		gse::memcpy(entry.body_b, elem + co.at("body_b"));
		std::uint32_t feature_hi = 0;
		std::uint32_t feature_lo = 0;
		gse::memcpy(feature_hi, elem + co.at("feature_key_hi"));
		gse::memcpy(feature_lo, elem + co.at("feature_key_lo"));
		entry.feature_key =
			(static_cast<std::uint64_t>(feature_hi) << 32) |
			static_cast<std::uint64_t>(feature_lo);

		std::uint32_t sticking = 0;
		gse::memcpy(sticking, elem + co.at("sticking"));
		entry.sticking = sticking != 0;

		gse::memcpy(entry.normal, elem + co.at("normal"));
		gse::memcpy(entry.tangent_u, elem + co.at("tangent_u"));
		gse::memcpy(entry.tangent_v, elem + co.at("tangent_v"));

		unitless::vec3 local_anchor_a{};
		gse::memcpy(local_anchor_a, elem + co.at("r_a"));
		entry.local_anchor_a = local_anchor_a * meters(1.f);

		unitless::vec3 local_anchor_b{};
		gse::memcpy(local_anchor_b, elem + co.at("r_b"));
		entry.local_anchor_b = local_anchor_b * meters(1.f);

		float c0_friction[4] = {};
		gse::memcpy(c0_friction, elem + co.at("c0_friction"));
		entry.c0 = { c0_friction[0], c0_friction[1], c0_friction[2] };
		entry.friction_coeff = c0_friction[3];

		float lambda[4] = {};
		gse::memcpy(lambda, elem + co.at("lambda"));
		entry.lambda = { lambda[0], lambda[1], lambda[2] };

		float penalty[4] = {};
		gse::memcpy(penalty, elem + co.at("penalty"));
		entry.penalty = { penalty[0], penalty[1], penalty[2] };

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

auto gse::vbd::gpu_solver::ready_to_dispatch() const -> bool {
	return !m_readback_pending;
}

auto gse::vbd::gpu_solver::mark_dispatched() -> void {
	m_readback_info = { m_body_count, m_contact_count };
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

	ctx.instantly_load(m_compute.predict);
	ctx.instantly_load(m_compute.solve_color);
	ctx.instantly_load(m_compute.update_lambda);
	ctx.instantly_load(m_compute.derive_velocities);
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
	m_warm_start_layout = extract_layout(m_compute.predict, "warm_starts");

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
			}
		};

		config.device_config().device.updateDescriptorSets(writes, nullptr);

		m_compute.descriptors_initialized = true;
	}

	if (m_body_count == 0) return;

	if (!m_first_submit) {
		static_cast<void>(device.waitForFences(*m_compute.fence, vk::True, std::numeric_limits<std::uint64_t>::max()));
	}
	device.resetFences(*m_compute.fence);

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
		sh->push(command, *m_compute.pipeline_layout, "push_constants",
			"body_count", m_body_count,
			"contact_count", max_contacts,
			"motor_count", m_motor_count,
			"color_offset", color_offset,
			"color_count", color_count,
			"warm_start_count", m_warm_start_count,
			"post_stabilize", cfg.post_stabilize ? 1u : 0u,
			"_pad0", 0u,
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

		if (debug.enable_solve_iterations) {
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
					bind_and_push(m_compute.derive_velocities, m_compute.derive_velocities_pipeline, 0u, 0u, sub, iterations, current_alpha);
					command.dispatch(body_workgroups, 1, 1);
					command.pipelineBarrier2(compute_dep);
				}
			}
		}
		else {
			bind_and_push(m_compute.derive_velocities, m_compute.derive_velocities_pipeline, 0u, 0u, sub, 0u, 0.f);
			command.dispatch(body_workgroups, 1, 1);
			command.pipelineBarrier2(compute_dep);
		}

		if (debug.enable_solve_iterations && cfg.iterations == 0u) {
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
	return m_contact_layout.offsets.at("lambda");
}

#if GSE_GPU_COMPARE
auto gse::vbd::gpu_solver::narrow_phase_debug_entries() const -> std::span<const narrow_phase_debug_entry> {
	return m_staged_narrow_phase_debug;
}
#endif
