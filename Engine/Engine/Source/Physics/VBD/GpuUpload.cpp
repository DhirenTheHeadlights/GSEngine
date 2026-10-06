module gse.physics:vbd_gpu_upload_impl;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.log;
import gse.math;
import gse.time;
import std;

import :collision_component;
import :joint_drive_component;
import :motion_component;
import :motor_component;
import :muscle_component;
import :system;
import :transform_component;
import :vbd_constraints;
import :vbd_gpu_solver;

auto gse::physics::copy_muscles_with_inputs(const shared_view<data> phys, read<muscle_component>& muscles, std::vector<muscle_definition>& out) -> void {
	const auto owners = phys.muscles.ids();
	const auto definitions = phys.muscles.items();
	out.assign(definitions.begin(), definitions.end());
	for (std::size_t i = 0; i < out.size(); ++i) {
		if (const auto* muscle = muscles.find(owners[i])) {
			apply_muscle_excitation(out[i], *muscle);
		}
	}
}

auto gse::physics::copy_joints_with_inputs(const shared_view<data> phys, read<joint_drive_component>& drives, std::vector<joint_definition>& out) -> void {
	const auto owners = phys.joints.ids();
	const auto definitions = phys.joints.items();
	if (out.size() != definitions.size()) {
		out.resize(definitions.size());
	}
	auto* out_data = out.data();
	const auto* definition_data = definitions.data();
	task::coarse_parallel(
		definitions.size(),
		256,
		[out_data, definition_data](const std::size_t i) {
			out_data[i] = definition_data[i];
		},
		trace_id<"vbd_gpu::build_joints::copy">()
	);
	task::coarse_parallel(
		out.size(),
		64,
		[out_data, owners, &drives](const std::size_t i) {
			if (const auto* drive = drives.find(owners[i])) {
				apply_joint_drive(out_data[i], *drive);
			}
		},
		trace_id<"vbd_gpu::build_joints::apply_inputs">()
	);
}

auto gse::physics::gpu_upload::run(data& d, const shared_view<physics::data> phys, const channel_write<gpu_upload_report, gpu_upload_payload, vbd::solver_upload> out, read<transform_component> transform, read<motion_component> motion, read<collision_component> collision, read<motor_component> motor, read<joint_drive_component> drives, read<muscle_component> muscles) -> async::task<> {
	const auto& plan = phys.gpu_plan;
	if (!gpu_solver_active(phys) || !plan.active || plan.generation == d.built_generation) {
		co_return;
	}
	const bool consecutive_plan = plan.generation == d.built_generation + 1;
	d.built_generation = plan.generation;

	auto& slot = d.scratch[d.scratch_slot];
	d.scratch_slot = (d.scratch_slot + 1) % d.scratch.size();
	auto& bodies = slot.bodies;
	std::vector<std::uint8_t> has_transform;
	slot.body_scan.resize(motion.size());
	auto scan_fill = physics::body_scan_fill{
		.entries = slot.body_scan,
		.sparse = !plan.reset && phys.gpu_solver.accepts_sparse_bodies(static_cast<std::uint32_t>(motion.size())),
	};
	bool refresh_joints = false;
	{
		trace::scope_guard _{ trace_id<"vbd_gpu::build_bodies">() };
		if (phys.hulls.size() != d.packed_hull_count) {
			trace::scope_guard _{ trace_id<"vbd_gpu::build_bodies::pack_hulls">() };
			pack_gpu_hulls(phys.hulls, d.hull_data, d.hull_offsets);
			d.packed_hull_count = phys.hulls.size();
			++d.hull_generation;
		}
		slot.hull_data.assign(d.hull_data.begin(), d.hull_data.end());

		body_build_view view{
			.motion_owners = motion.owner_ids(),
			.motions = { motion.data(), motion.size() },
			.transform_owners = transform.owner_ids(),
			.transforms = { transform.data(), transform.size() },
			.collision_owners = collision.owner_ids(),
			.collisions = { collision.data(), collision.size() },
			.hulls = phys.hulls,
			.hull_offsets = d.hull_offsets,
		};

		auto& body_props = slot.body_props;
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_bodies::mass_props">() };
			build_mass_properties(view, body_props);
		}
		view.mass_props = body_props;

		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_bodies::states">() };
			build_body_states(view, phys.sleep_counters, bodies, d.body_index, d.body_index_entries, has_transform, scan_fill);
			refresh_joints = plan.reset || d.force_full_joints || phys.joints_generation != d.uploaded_joints_generation || d.uploaded_body_count != bodies.size() ||
				d.joint_slots.size() != phys.joints.size() || d.joint_body_index_entries != d.body_index_entries ||
				phys.muscles_generation != d.uploaded_muscles_generation || d.muscle_slots.size() != phys.muscles.size();
			if (refresh_joints && scan_fill.sparse) {
				scan_fill.sparse = false;
				build_body_states(view, phys.sleep_counters, bodies, d.body_index, d.body_index_entries, has_transform, scan_fill);
			}
		}
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_bodies::bounds">() };
			build_body_bounds(view, d.body_index, has_transform, bodies, scan_fill);
		}
	}

	const auto replay = static_cast<std::size_t>(plan.replay_ticks);
	const auto frame_inputs = gather_step_inputs(motor, transform, motion, std::span<const impulse_request>{});
	auto per_tick = plan.per_tick;
	for (std::size_t t = replay; t < per_tick.size(); ++t) {
		per_tick[t].motors = frame_inputs.motors;
		if (t == replay) {
			per_tick[t].kinematics = frame_inputs.kinematics;
		}
	}

	auto& gpu_impulses = slot.impulses;
	gpu_impulses.clear();
	std::vector<std::uint32_t> impulse_counts(per_tick.size(), 0u);
	{
		trace::scope_guard _{ trace_id<"vbd_gpu::build_impulses">() };
		for (std::size_t t = 0; t < per_tick.size(); ++t) {
			for (const auto& req : per_tick[t].impulses) {
				const auto it = d.body_index.find(req.target);
				if (it == d.body_index.end()) {
					continue;
				}
				const auto* mc = motion.find(req.target);
				if (!mc || !is_dynamic(*mc)) {
					continue;
				}
				gpu_impulses.push_back(
					vbd::impulse_constraint{
						.body_index = it->second,
						.delta_velocity = req.impulse / mass_of(*mc),
					}
				);
				++impulse_counts[t];
			}
		}
	}

	auto& motors = slot.motors;
	motors.clear();
	std::uint32_t motors_per_tick = 0;
	{
		trace::scope_guard _{ trace_id<"vbd_gpu::build_motors">() };
		std::vector<vbd::velocity_motor_constraint> tick_motors;
		d.motor_body_slots.resize(frame_inputs.motors.size(), std::numeric_limits<std::uint32_t>::max());
		const physics::body_slot_cache motor_slots{
			.body_owners = motion.owner_ids(),
			.has_transform = has_transform,
			.slots = d.motor_body_slots,
		};
		bool uniform = true;
		for (std::size_t t = 0; t < per_tick.size(); ++t) {
			build_motor_constraints(per_tick[t].motors, d.body_index, phys.body_airborne, bodies, tick_motors, motor_slots, scan_fill);
			if (t == 0) {
				motors_per_tick = static_cast<std::uint32_t>(tick_motors.size());
			}
			else if (!std::ranges::equal(tick_motors, std::span(motors).first(motors_per_tick), {}, &vbd::velocity_motor_constraint::body_index, &vbd::velocity_motor_constraint::body_index)) {
				uniform = false;
				break;
			}
			motors.insert(motors.end(), tick_motors.begin(), tick_motors.end());
		}
		if (!uniform) {
			log::println(log::level::warning, log::category::physics, "gpu replay: the motor set changed inside the replay window; every tick uses this frame's motors");
			build_motor_constraints(frame_inputs.motors, d.body_index, phys.body_airborne, bodies, tick_motors, motor_slots, scan_fill);
			motors_per_tick = static_cast<std::uint32_t>(tick_motors.size());
			motors.clear();
			for (std::size_t t = 0; t < per_tick.size(); ++t) {
				motors.insert(motors.end(), tick_motors.begin(), tick_motors.end());
			}
		}
		assert(
			motors.size() <= phys.gpu_solver.capacities().max_motors,
			"per-tick motor count {} exceeds Physics.gpu_max_motors = {}",
			motors.size(),
			phys.gpu_solver.capacities().max_motors
		);

		std::size_t wake_at = 0;
		for (std::size_t t = 0; t <= replay && t < impulse_counts.size(); ++t) {
			wake_at += impulse_counts[t];
		}
		if (replay < impulse_counts.size()) {
			constexpr auto no_wake = std::numeric_limits<std::uint32_t>::max();
			std::vector<std::uint32_t> wake_bodies(frame_inputs.motors.size(), no_wake);
			d.wake_body_slots.resize(frame_inputs.motors.size(), no_wake);
			const physics::body_slot_cache wake_slots{
				.body_owners = motion.owner_ids(),
				.has_transform = has_transform,
				.slots = d.wake_body_slots,
			};
			const auto* motion_data = motion.data();
			task::coarse_parallel(
				frame_inputs.motors.size(),
				64,
				[&](const std::size_t i) {
					const auto& [owner, mt] = frame_inputs.motors[i];
					if (magnitude(mt.velocity_drive_target) <= meters_per_second(.01f)) {
						return;
					}
					const auto slot = resolve_body_slot(d.body_index, wake_slots, i, owner);
					if (!slot || !is_dynamic(motion_data[*slot])) {
						return;
					}
					const auto idx = *slot;
					if (mt.requires_ground_contact && idx < phys.body_airborne.size() && phys.body_airborne[idx] != 0) {
						return;
					}
					if (idx < phys.body_sleeping.size() && phys.body_sleeping[idx] != 0) {
						wake_bodies[i] = idx;
					}
				},
				trace_id<"vbd_gpu::build_motors::wake_scan">()
			);
			for (const auto idx : wake_bodies) {
				if (idx == no_wake) {
					continue;
				}
				gpu_impulses.insert(gpu_impulses.begin() + static_cast<std::ptrdiff_t>(wake_at), {
					.body_index = idx,
					.delta_velocity = {},
				});
				++impulse_counts[replay];
				++wake_at;
			}
		}
		assert(
			gpu_impulses.size() <= phys.gpu_solver.capacities().max_impulses,
			"impulse count {} exceeds Physics.gpu_max_impulses = {}",
			gpu_impulses.size(),
			phys.gpu_solver.capacities().max_impulses
		);
	}

	auto& gpu_joints = slot.joints;
	auto& gpu_joint_inputs = slot.joint_inputs;
	gpu_joint_inputs.clear();
	auto& gpu_joint_input_slots = slot.joint_input_slots;
	gpu_joint_input_slots.clear();
	auto& gpu_muscles = slot.muscles;
	auto& gpu_muscle_points = slot.muscle_points;
	auto& gpu_muscle_excitations = slot.muscle_excitations;
	gpu_muscle_excitations.clear();
	std::vector<joint_rest_orientation> rest_orientations;
	const bool refresh_joint_inputs = phys.joint_inputs_generation != d.uploaded_joint_inputs_generation || d.joint_inputs_full_pending;
	const bool full_joint_inputs = plan.full_joint_inputs || !consecutive_plan || d.joint_inputs_full_pending;
	const auto joint_drive_input_of = [](const joint_definition& jd, const joint_drive_component* drive) {
		auto input = vbd::joint_drive_input{
			.drive_target = jd.drive_target,
			.drive_stiffness = jd.drive_stiffness,
			.drive_damping = jd.drive_damping,
			.drive_max_torque = jd.drive_max_torque,
		};
		if (drive) {
			if (drive->enabled) {
				input.drive_target = drive->target;
				input.drive_stiffness = drive->stiffness;
				input.drive_damping = drive->damping;
				input.drive_max_torque = drive->max_torque;
				input.device_target = drive->device_target ? 1u : 0u;
				input.device_stiffness = drive->device_stiffness ? 1u : 0u;
			}
			else {
				input.drive_stiffness = {};
			}
		}
		return input;
	};
	if (refresh_joints) {
		trace::scope_guard _{ trace_id<"vbd_gpu::build_joints">() };
		assert(
			phys.joints.size() <= phys.gpu_solver.capacities().max_joints,
			"joint count {} exceeds Physics.gpu_max_joints = {}",
			phys.joints.size(),
			phys.gpu_solver.capacities().max_joints
		);
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_joints::copy_inputs">() };
			copy_joints_with_inputs(phys, drives, d.joints);
		}
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_muscles">() };
			copy_muscles_with_inputs(phys, muscles, d.muscles);
			build_muscle_constraints(d.muscles, d.body_index, gpu_muscles, gpu_muscle_points, &d.muscle_slots);
			assert(
				gpu_muscles.size() <= phys.gpu_solver.capacities().max_muscles && gpu_muscle_points.size() <= phys.gpu_solver.capacities().max_muscle_points,
				"{} muscles with {} path points exceed Physics.gpu_max_muscles = {} or Physics.gpu_max_muscle_points = {}",
				gpu_muscles.size(),
				gpu_muscle_points.size(),
				phys.gpu_solver.capacities().max_muscles,
				phys.gpu_solver.capacities().max_muscle_points
			);
			gpu_muscle_excitations.resize(gpu_muscles.size());
			for (std::size_t i = 0; i < gpu_muscles.size(); ++i) {
				gpu_muscle_excitations[i] = gpu_muscles[i].excitation;
			}
			d.uploaded_muscle_count = static_cast<std::uint32_t>(gpu_muscles.size());
		}
		std::vector<std::size_t> uninitialized;
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_joints::scan_uninit">() };
			for (std::size_t i = 0; i < d.joints.size(); ++i) {
				if (!d.joints[i].rest_orientation_initialized) {
					uninitialized.push_back(i);
				}
			}
		}
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_joints::constraints">() };
			build_joint_constraints(d.joints, d.body_index, bodies, gpu_joints, &d.joint_slots);
		}
		const auto owners = phys.joints.ids();
		for (const auto i : uninitialized) {
			if (d.joints[i].rest_orientation_initialized) {
				rest_orientations.push_back({
					.owner = owners[i],
					.rest_orientation = d.joints[i].rest_orientation,
				});
			}
		}
		{
			trace::scope_guard _{ trace_id<"vbd_gpu::build_joints::slot_map">() };
			constexpr auto unresolved = std::numeric_limits<std::uint32_t>::max();
			std::vector<std::pair<id, std::uint32_t>> slot_entries;
			slot_entries.reserve(d.joint_slots.size());
			for (std::size_t i = 0; i < d.joint_slots.size(); ++i) {
				if (d.joint_slots[i] != unresolved) {
					slot_entries.emplace_back(owners[i], d.joint_slots[i]);
				}
			}
			d.joint_gpu_slots.clear();
			d.joint_gpu_slots.insert(slot_entries.begin(), slot_entries.end());
			++d.joint_gpu_slots_generation;
		}
		d.force_full_joints = !rest_orientations.empty();
		d.joint_body_index_entries = d.body_index_entries;
		d.uploaded_joint_count = static_cast<std::uint32_t>(gpu_joints.size());
		d.joint_inputs_full_pending = true;
	}
	else if (refresh_joint_inputs && !full_joint_inputs) {
		trace::scope_guard _{ trace_id<"vbd_gpu::build_joint_inputs::sparse">() };
		constexpr auto unresolved = std::numeric_limits<std::uint32_t>::max();
		const auto owners = phys.joints.ids();
		const auto definitions = phys.joints.items();
		gpu_joint_inputs.reserve(plan.joint_drive_changes.size());
		gpu_joint_input_slots.reserve(plan.joint_drive_changes.size());
		for (const auto [joint, drive] : plan.joint_drive_changes) {
			const auto slot_index = d.joint_slots[joint];
			if (slot_index == unresolved) {
				continue;
			}
			const auto* component = drive < drives.size() && drives.owner_id_at(drive) == owners[joint] ? drives.data() + drive : drives.find(owners[joint]);
			gpu_joint_inputs.push_back(joint_drive_input_of(definitions[joint], component));
			gpu_joint_input_slots.push_back(slot_index);
		}
	}
	else if (refresh_joint_inputs) {
		trace::scope_guard _{ trace_id<"vbd_gpu::build_joint_inputs">() };
		constexpr auto unresolved = std::numeric_limits<std::uint32_t>::max();
		d.joint_inputs_full_pending = false;
		gpu_joint_inputs.resize(d.uploaded_joint_count);
		const auto owners = phys.joints.ids();
		const auto definitions = phys.joints.items();
		const auto* definition_data = definitions.data();
		const auto* slot_data = d.joint_slots.data();
		auto* input_data = gpu_joint_inputs.data();
		const auto drives_aligned = std::ranges::equal(drives.owner_ids(), owners);
		const auto* drive_data = drives.data();
		const auto drive_count = drives.size();
		const auto write_input = [definition_data, slot_data, input_data, &joint_drive_input_of](const std::size_t i, const joint_drive_component* drive) {
			const auto slot_index = slot_data[i];
			if (slot_index == unresolved) {
				return;
			}
			input_data[slot_index] = joint_drive_input_of(definition_data[i], drive);
		};

		if (drives_aligned) {
			task::coarse_parallel(
				definitions.size(),
				64,
				[&write_input, &drives](const std::size_t i) {
					write_input(i, std::addressof(drives[i]));
				},
				trace_id<"vbd_gpu::build_joint_inputs::records">()
			);
		}
		else {
			constexpr auto no_drive = std::numeric_limits<std::uint32_t>::max();
			std::atomic<bool> drive_slots_stale = d.joint_drive_slots.size() != definitions.size() || d.joint_drive_count != drive_count;
			if (!drive_slots_stale.load(std::memory_order_relaxed)) {
				const auto* drive_slots = d.joint_drive_slots.data();
				task::coarse_parallel(
					definitions.size(),
					64,
					[&write_input, &drives, &drive_slots_stale, drive_slots, drive_data, drive_count, owners](const std::size_t i) {
						const auto drive_slot = drive_slots[i];
						if (drive_slot == no_drive) {
							write_input(i, nullptr);
							return;
						}
						if (drive_slot < drive_count && drives.owner_id_at(drive_slot) == owners[i]) {
							write_input(i, drive_data + drive_slot);
							return;
						}
						drive_slots_stale.store(true, std::memory_order_relaxed);
						write_input(i, drives.find(owners[i]));
					},
					trace_id<"vbd_gpu::build_joint_inputs::records">()
				);
			}
			if (drive_slots_stale.load(std::memory_order_relaxed)) {
				d.joint_drive_slots.assign(definitions.size(), no_drive);
				auto* drive_slots = d.joint_drive_slots.data();
				task::coarse_parallel(
					definitions.size(),
					64,
					[&write_input, &drives, drive_slots, drive_data, owners](const std::size_t i) {
						const auto* drive = drives.find(owners[i]);
						if (drive) {
							drive_slots[i] = static_cast<std::uint32_t>(drive - drive_data);
						}
						write_input(i, drive);
					},
					trace_id<"vbd_gpu::build_joint_inputs::records">()
				);
				d.joint_drive_count = static_cast<std::size_t>(std::ranges::count_if(d.joint_drive_slots, [](const std::uint32_t s) { return s != no_drive; }));
			}
		}
	}

	if (!refresh_joints && phys.muscle_inputs_generation != d.uploaded_muscle_inputs_generation && d.uploaded_muscle_count > 0) {
		trace::scope_guard _{ trace_id<"vbd_gpu::build_muscle_inputs">() };
		constexpr auto unresolved = std::numeric_limits<std::uint32_t>::max();
		gpu_muscle_excitations.resize(d.uploaded_muscle_count);
		const auto owners = phys.muscles.ids();
		const auto definitions = phys.muscles.items();
		for (std::size_t i = 0; i < definitions.size() && i < d.muscle_slots.size(); ++i) {
			const auto slot_index = d.muscle_slots[i];
			if (slot_index == unresolved) {
				continue;
			}
			const auto* muscle = muscles.find(owners[i]);
			gpu_muscle_excitations[slot_index] = muscle ? muscle->excitation : definitions[i].excitation;
		}
	}

	{
		trace::scope_guard _{ trace_id<"vbd_gpu::upload">() };
		d.uploaded_joints_generation = phys.joints_generation;
		d.uploaded_joint_inputs_generation = phys.joint_inputs_generation;
		d.uploaded_muscles_generation = phys.muscles_generation;
		d.uploaded_muscle_inputs_generation = phys.muscle_inputs_generation;
		d.uploaded_body_count = static_cast<std::uint32_t>(bodies.size());

		auto upload = vbd::solver_upload{
			.bodies = bodies,
			.body_scan = slot.body_scan,
			.motors = motors,
			.joints = refresh_joints ? std::span<const vbd::joint_constraint>(gpu_joints) : std::span<const vbd::joint_constraint>{},
			.joint_inputs = gpu_joint_inputs,
			.joint_input_slots = gpu_joint_input_slots,
			.muscles = refresh_joints ? std::span<const vbd::muscle_constraint>(gpu_muscles) : std::span<const vbd::muscle_constraint>{},
			.muscle_points = refresh_joints ? std::span<const vbd::muscle_path_point>(gpu_muscle_points) : std::span<const vbd::muscle_path_point>{},
			.muscle_excitations = gpu_muscle_excitations,
			.hull_data = slot.hull_data,
			.hull_generation = d.hull_generation,
			.impulses = gpu_impulses,
			.solver_cfg = phys.vbd_solver.config(),
			.dt = system_clock::fixed_dt() * static_cast<float>(plan.total_ticks),
			.steps = plan.total_ticks * std::max(phys.physics_substeps, 1),
			.ticks = plan.total_ticks,
			.refresh_joints = refresh_joints,
			.force_reseed = plan.reset,
			.bodies_complete = !scan_fill.sparse,
			.first_tick = plan.first_tick,
			.restore_tick = plan.restore_tick,
			.motors_per_tick = motors_per_tick,
			.island_pack_min_islands = static_cast<std::uint32_t>(phys.gpu_island_pack_min_islands),
			.impulse_counts = std::move(impulse_counts),
		};
		if (phys.gpu_sync_readback || phys.gpu_same_frame_upload) {
			out.push<gpu_upload_payload>({
				.upload = std::move(upload),
			});
		}
		else {
			out.push<vbd::solver_upload>(std::move(upload));
		}
	}

	auto report = gpu_upload_report{
		.body_index = d.body_index,
		.rest_orientations = std::move(rest_orientations),
		.recorded = {},
	};
	if (!phys.rollback_ring.empty()) {
		report.recorded.reserve(per_tick.size() - replay);
		for (std::size_t t = replay; t < per_tick.size(); ++t) {
			report.recorded.push_back({
				.tick = plan.first_tick + static_cast<std::uint64_t>(t) + 1,
				.inputs = std::move(per_tick[t]),
			});
		}
	}
	out.push<gpu_upload_report>(std::move(report));

	co_return;
}