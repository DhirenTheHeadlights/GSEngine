module gse.physics:vbd_gpu_solver_diagnostics_impl;

import std;

import gse.core;
import gse.math;
import gse.concurrency;
import gse.ecs;
import gse.log;

import :system;
import :vbd_constraints;
import :vbd_gpu_solver;
import :vbd_gpu_solver_diagnostics;

auto gse::physics::gpu_diagnostics::run(data& d, const shared_view<physics::data> phys) -> async::task<> {
	if (!d.enabled || !gpu_solver_active(phys)) {
		co_return;
	}

	const auto diag = phys.gpu_solver.diagnostics();

	++d.ticks;
	d.conflict_total += diag.coloring_conflicts;
	d.stale_read_total += diag.stale_reads;
	d.stale_check_total += diag.stale_checks;
	if (diag.stale_reads != 0 && d.stale_first_tick == 0) {
		d.stale_first_tick = d.ticks;
	}

	if (phys.gpu_solver.reseeding()) {
		log::println(
			log::category::physics,
			"vbd gpu reseed at tick {}: body_count {}",
			d.ticks,
			phys.gpu_solver.body_count()
		);
	}

	const vbd::solver_diagnostics peak{
		.attempted_contacts = std::max(diag.attempted_contacts, d.peak.attempted_contacts),
		.max_used_color = std::max(diag.max_used_color, d.peak.max_used_color),
		.coloring_fallbacks = std::max(diag.coloring_fallbacks, d.peak.coloring_fallbacks),
		.coloring_conflicts = std::max(diag.coloring_conflicts, d.peak.coloring_conflicts),
		.contact_duplicates = std::max(diag.contact_duplicates, d.peak.contact_duplicates),
		.warm_start_hits = std::max(diag.warm_start_hits, d.peak.warm_start_hits),
		.stale_reads = std::max(diag.stale_reads, d.peak.stale_reads),
		.stale_checks = std::max(diag.stale_checks, d.peak.stale_checks),
		.sweep_bails = std::max(diag.sweep_bails, d.peak.sweep_bails),
		.broad_nodes_walked = std::max(diag.broad_nodes_walked, d.peak.broad_nodes_walked),
		.broad_aabb_tests = std::max(diag.broad_aabb_tests, d.peak.broad_aabb_tests),
		.broad_pairs = std::max(diag.broad_pairs, d.peak.broad_pairs),
		.joint_lambda_max = std::max(diag.joint_lambda_max, d.peak.joint_lambda_max),
		.joint_penalty_max = std::max(diag.joint_penalty_max, d.peak.joint_penalty_max),
		.joint_c_max = std::max(diag.joint_c_max, d.peak.joint_c_max),
		.joint_c0_max = std::max(diag.joint_c0_max, d.peak.joint_c0_max),
	};

	if (peak == d.peak && d.ticks % 60 != 0) {
		co_return;
	}

	d.peak = peak;
	log::println(
		log::category::physics,
		"vbd gpu peak: contacts {}/{} dropped {} dup {} | colors {}/{} fallback {} conflicts {} total {} over {} ticks | stale {} of {} first {} | sweep bails {} | broad nodes {} tests {} pairs {} | joint lambda {:.3f} penalty {:.3f} c {:.6f} c0 {:.6f}",
		peak.attempted_contacts,
		vbd::limits.max_contacts,
		peak.attempted_contacts > vbd::limits.max_contacts ? peak.attempted_contacts - vbd::limits.max_contacts : 0u,
		peak.contact_duplicates,
		peak.max_used_color,
		vbd::limits.max_colors,
		peak.coloring_fallbacks,
		peak.coloring_conflicts,
		d.conflict_total,
		d.ticks,
		peak.stale_reads,
		d.stale_check_total,
		d.stale_first_tick,
		peak.sweep_bails,
		peak.broad_nodes_walked,
		peak.broad_aabb_tests,
		peak.broad_pairs,
		peak.joint_lambda_max,
		peak.joint_penalty_max,
		peak.joint_c_max,
		peak.joint_c0_max
	);

	co_return;
}
