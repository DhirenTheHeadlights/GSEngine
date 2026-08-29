module gse.physics:vbd_shadow_step_impl;

import std;

import gse.core;
import gse.math;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.log;
import gse.gpu;
import gse.gpu_record;

import :motion_component;
import :collision_component;
import :transform_component;
import :system;
import :vbd_constraints;
import :vbd_gpu_solver;
import :vbd_shadow_step;

namespace gse::physics::shadow_step {
	auto capture_world(
		read<transform_component>& transform,
		read<motion_component>& motion
	) -> std::vector<body_sample>;

	auto samples_align(
		std::span<const body_sample> a,
		std::span<const body_sample> b
	) -> bool;

	auto report_step(
		const data& d,
		const pending_step& record,
		std::span<const vbd::body_state> solved,
		const vbd::solver_diagnostics& diag,
		std::span<const std::uint32_t> grounded,
		std::span<const std::uint32_t> narrow_debug,
		std::span<const vbd::contact_constraint> contact_dump,
		std::span<const std::uint32_t> adjacency_meta,
		std::span<const std::uint32_t> adjacency_dump
	) -> void;

	auto resolve_retired(
		data& d
	) -> void;
}

auto gse::physics::shadow_step::capture_world(read<transform_component>& transform, read<motion_component>& motion) -> std::vector<body_sample> {
	std::vector<body_sample> snapshot;
	snapshot.reserve(motion.size());

	const auto motion_ids = motion.owner_ids();
	for (std::size_t i = 0; i < motion.size(); ++i) {
		if (is_static(motion[i])) {
			continue;
		}
		const auto* tc = transform.find(motion_ids[i]);
		if (!tc) {
			continue;
		}
		snapshot.push_back({
			.owner = motion_ids[i],
			.body_index = static_cast<std::uint32_t>(i),
			.position = tc->position,
			.velocity = motion[i].current_velocity,
		});
	}

	return snapshot;
}

auto gse::physics::shadow_step::samples_align(const std::span<const body_sample> a, const std::span<const body_sample> b) -> bool {
	return std::ranges::equal(
		a,
		b,
		[](const body_sample& x, const body_sample& y) {
			return x.owner == y.owner && x.body_index == y.body_index;
		}
	);
}

auto gse::physics::shadow_step::report_step(const data& d, const pending_step& record, const std::span<const vbd::body_state> solved, const vbd::solver_diagnostics& diag, const std::span<const std::uint32_t> grounded, const std::span<const std::uint32_t> narrow_debug, const std::span<const vbd::contact_constraint> contact_dump, const std::span<const std::uint32_t> adjacency_meta, const std::span<const std::uint32_t> adjacency_dump) -> void {
	const velocity micro = meters_per_second(1e-6f);
	const velocity milli = meters_per_second(1e-3f);

	length max_position;
	velocity max_velocity;
	std::uint64_t worst_position_owner = 0;
	std::uint64_t worst_velocity_owner = 0;
	velocity worst_cpu_change;
	velocity worst_gpu_change;
	std::size_t compared = 0;
	std::size_t over_micro = 0;
	std::size_t over_milli = 0;

	for (std::size_t i = 0; i < record.pre.size(); ++i) {
		const auto& sample = record.pre[i];
		if (sample.body_index >= solved.size()) {
			continue;
		}
		const auto& gpu = solved[sample.body_index];
		const auto& cpu = record.cpu_post[i];

		const length position_residual = distance(gpu.position, cpu.position);
		const velocity velocity_residual = magnitude(gpu.velocity - cpu.velocity);

		++compared;
		if (velocity_residual > micro) {
			++over_micro;
		}
		if (velocity_residual > milli) {
			++over_milli;
		}
		if (position_residual > max_position) {
			max_position = position_residual;
			worst_position_owner = sample.owner.number();
		}
		if (velocity_residual > max_velocity) {
			max_velocity = velocity_residual;
			worst_velocity_owner = sample.owner.number();
			worst_cpu_change = magnitude(cpu.velocity - sample.velocity);
			worst_gpu_change = magnitude(gpu.velocity - sample.velocity);
		}
	}

	log::println(
		log::category::physics,
		"shadow step {}: bodies {} | max dp {:.9f:m} on {} | max dv {:.9f} on {} (cpu dv {:.6f} gpu dv {:.6f}) | dv over 1e-6 {} over 1e-3 {} | contacts {} ws {}",
		record.step,
		compared,
		max_position,
		worst_position_owner,
		max_velocity,
		worst_velocity_owner,
		worst_cpu_change,
		worst_gpu_change,
		over_micro,
		over_milli,
		diag.attempted_contacts,
		diag.warm_start_hits
	);

	if (d.detail_step < 0 || record.step != static_cast<std::uint32_t>(d.detail_step)) {
		return;
	}

	log::println(
		log::category::physics,
		"shadow step {} detail: every body follows",
		record.step
	);

	for (std::size_t i = 0; i < record.pre.size(); ++i) {
		const auto& sample = record.pre[i];
		if (sample.body_index >= solved.size()) {
			continue;
		}
		const auto& gpu = solved[sample.body_index];
		const auto& cpu = record.cpu_post[i];
		const auto word = sample.body_index / 32u;
		const auto grounded_bit = word < grounded.size() ? (grounded[word] >> (sample.body_index % 32u)) & 1u : 0u;
		log::println(
			log::category::physics,
			"shadow detail body {}: dp {:.9f:m} dv {:.9f} cpu dv {:.6f} gpu dv {:.6f} pre speed {:.6f} | aw {:.2f} sleep {} grounded {}",
			sample.owner.number(),
			distance(gpu.position, cpu.position),
			magnitude(gpu.velocity - cpu.velocity),
			magnitude(cpu.velocity - sample.velocity),
			magnitude(gpu.velocity - sample.velocity),
			magnitude(sample.velocity),
			gpu.accel_weight,
			gpu.sleep_counter,
			grounded_bit
		);
	}

	if (narrow_debug.size() > vbd::limits.state_debug_count_index) {
		const auto debug_count = std::min(narrow_debug[vbd::limits.state_debug_count_index], vbd::limits.max_narrow_phase_debug_records);
		for (std::uint32_t r = 0; r < debug_count; ++r) {
			const auto base = vbd::limits.collision_state_header_uints + r * vbd::limits.narrow_phase_debug_record_uints;
			if (base + 7 >= narrow_debug.size()) {
				break;
			}
			log::println(
				log::category::physics,
				"shadow narrow debug {}: bodies {} {} flags {:#x} clipped {} raw {} unique {} emitted {} faces {:#06x}",
				r,
				narrow_debug[base + 0],
				narrow_debug[base + 1],
				narrow_debug[base + 2],
				narrow_debug[base + 3],
				narrow_debug[base + 4],
				narrow_debug[base + 5],
				narrow_debug[base + 6],
				narrow_debug[base + 7]
			);
		}
	}

	const velocity interesting = meters_per_second(0.05f);
	const auto meta_half = adjacency_meta.size() / 2;
	for (std::size_t i = 0; i < record.pre.size(); ++i) {
		const auto& sample = record.pre[i];
		if (sample.body_index >= solved.size()) {
			continue;
		}
		const auto& gpu = solved[sample.body_index];
		const auto& cpu = record.cpu_post[i];
		if (magnitude(gpu.velocity - cpu.velocity) <= interesting) {
			continue;
		}
		const auto bi = sample.body_index;
		const auto cnt = bi < meta_half ? adjacency_meta[bi] : 0u;
		const auto off = bi < meta_half ? adjacency_meta[meta_half + bi] : 0u;
		log::println(
			log::category::physics,
			"shadow adjacency body {} (idx {}): count {} offset {}",
			sample.owner.number(),
			bi,
			cnt,
			off
		);
		for (std::uint32_t k = 0; k < std::min(cnt, 8u); ++k) {
			const auto slot = static_cast<std::size_t>(off) + k;
			const auto ci = slot < adjacency_dump.size() ? adjacency_dump[slot] : 0xFFFFFFFFu;
			if (ci < contact_dump.size()) {
				const auto& c = contact_dump[ci];
				log::println(
					log::category::physics,
					"  adj[{}] ci {}: a {} b {} ny {:.3f} c0 {:.6f} lambda {:.4f} pen {:.1f} floor {:.1f} stick {:#x} feature {:#x}",
					k,
					ci,
					c.body_a,
					c.body_b,
					c.normal.y(),
					c.c0.x(),
					c.lambda.x(),
					c.penalty.x(),
					c.penalty_floor,
					c.sticking,
					c.feature_key
				);
			}
			else {
				log::println(log::category::physics, "  adj[{}] ci {} beyond dump range", k, ci);
			}
		}
	}

	for (std::uint32_t ci = 0; ci < contact_dump.size(); ++ci) {
		const auto& c = contact_dump[ci];
		if (c.body_a == c.body_b) {
			continue;
		}
		log::println(
			log::category::physics,
			"shadow contact ci {}: a {} b {} ny {:.3f} c0 {:.6f} lambda {:.4f} pen {:.1f} floor {:.1f} stick {:#x}",
			ci,
			c.body_a,
			c.body_b,
			c.normal.y(),
			c.c0.x(),
			c.lambda.x(),
			c.penalty.x(),
			c.penalty_floor,
			c.sticking
		);
	}
}

auto gse::physics::shadow_step::resolve_retired(data& d) -> void {
	if (d.pending.empty()) {
		return;
	}

	const auto retired = d.solver.retired_generation();
	if (retired == 0) {
		return;
	}

	const auto solved = d.solver.read_body_states();
	const auto diag = d.solver.diagnostics();
	const auto grounded = d.solver.read_grounded();
	const auto narrow_debug = d.solver.read_narrow_phase_debug();
	const auto contact_dump = d.solver.read_contact_dump();
	const auto adjacency_meta = d.solver.read_adjacency_meta();
	const auto adjacency_dump = d.solver.read_contact_adjacency_dump();

	std::size_t consumed = 0;
	for (const auto& record : d.pending) {
		if (record.generation == 0 || record.generation > retired) {
			break;
		}
		++consumed;

		if (record.generation < retired) {
			log::println(
				log::level::warning,
				log::category::physics,
				"shadow step {}: dispatch {} was overwritten before its readback was consumed (retired {}); no residual for this step",
				record.step,
				record.generation,
				retired
			);
			continue;
		}

		if (!record.cpu_recorded || solved.empty()) {
			continue;
		}

		if (!samples_align(record.pre, record.cpu_post)) {
			log::println(
				log::level::warning,
				log::category::physics,
				"shadow step {}: the body set changed between the pre-step and post-step captures; no residual for this step",
				record.step
			);
			continue;
		}

		report_step(d, record, solved, diag, grounded, narrow_debug, contact_dump, adjacency_meta, adjacency_dump);
	}

	d.pending.erase(d.pending.begin(), d.pending.begin() + static_cast<std::ptrdiff_t>(consumed));
}

auto gse::physics::shadow_step::init(context& ctx, const std::optional<shared_view<gpu::context::data>> gpu_s, data& d) -> async::task<> {
	if (!d.enabled) {
		co_return;
	}

	if (!gpu_s) {
		log::println(log::level::error, log::category::physics, "shadow step: there is no gpu context, so the harness cannot run");
		co_return;
	}

	co_await d.solver.initialize_compute(ctx, *gpu_s);
	d.solver.set_preserve_warm_starts(true);
	d.buffers_ready = d.solver.buffers_created();

	log::println(
		log::category::physics,
		"shadow step: gpu solver ready ({}). The cpu solver drives the world; the gpu is re-seeded from its pre-step state every tick and its result is discarded. Joints, motors and impulses are not shadowed, and the device is stalled once per tick so this run's timings are not measurements",
		d.buffers_ready
	);

	co_return;
}

auto gse::physics::shadow_step::run(data& d, const shared_view<physics::data> phys, read<transform_component> transform, read<motion_component> motion, read<collision_component> collision) -> async::task<> {
	if (!d.enabled || !d.buffers_ready || motion.empty()) {
		co_return;
	}

	if (gpu_solver_active(phys)) {
		if (!d.conflict_reported) {
			d.conflict_reported = true;
			log::println(
				log::level::error,
				log::category::physics,
				"shadow step: physics is running on the gpu solver, so there is no cpu reference to compare against. Disable Physics.use_gpu_solver for this run"
			);
		}
		co_return;
	}

	trace::scope_guard sg{ trace_id<"vbd_shadow::capture">() };

	auto snapshot = capture_world(transform, motion);

	for (auto& record : d.pending) {
		if (!record.cpu_recorded) {
			record.cpu_post = snapshot;
			record.cpu_recorded = true;
		}
	}

	resolve_retired(d);

	const int steps = system_clock::fixed_steps_this_frame();
	if (steps <= 0) {
		co_return;
	}

	std::vector<vbd::body_state> bodies;
	std::vector<std::uint8_t> has_transform;
	std::flat_map<id, std::uint32_t> id_to_body_index;

	body_build_view view{
		.motion_owners = motion.owner_ids(),
		.motions = { motion.data(), motion.size() },
		.transform_owners = transform.owner_ids(),
		.transforms = { transform.data(), transform.size() },
		.collision_owners = collision.owner_ids(),
		.collisions = { collision.data(), collision.size() },
		.hulls = phys.hulls,
	};

	std::vector<mass_properties> body_props;
	build_mass_properties(view, body_props);
	view.mass_props = body_props;

	build_body_states(view, phys.sleep_counters, bodies, id_to_body_index, has_transform);
	build_body_bounds(view, id_to_body_index, has_transform, bodies);

	const auto& cfg = phys.vbd_solver.config();

	const auto dt = system_clock::fixed_dt();
	const int substeps = std::max(phys.physics_substeps, 1);

	d.solver.upload({
		.bodies = std::move(bodies),
		.solver_cfg = cfg,
		.dt = dt * static_cast<float>(steps),
		.steps = steps * substeps,
		.refresh_joints = true,
		.force_reseed = true,
	});

	d.pending.push_back({
		.step = d.step_index,
		.pre = std::move(snapshot),
	});
	++d.step_index;

	co_return;
}

auto gse::physics::shadow_step::frame(context& ctx, const std::optional<shared_view<gpu::context::data>> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!d.enabled || !gpu_s || !d.buffers_ready || !d.solver.pending_dispatch()) {
		co_return;
	}

	if (gpu_s->device) {
		trace::scope_guard sg{ trace_id<"vbd_shadow::wait_idle">() };
		gpu_s->device->wait_idle();
	}

	d.solver.commit_upload();
	co_await d.solver.dispatch_compute(ctx, pass_out);

	if (!d.pending.empty() && d.pending.back().generation == 0) {
		d.pending.back().generation = d.solver.dispatch_generation();
	}
}