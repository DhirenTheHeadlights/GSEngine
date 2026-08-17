module gse.physics:vbd_contact_trace_impl;

import std;

import gse.core;
import gse.math;
import gse.concurrency;
import gse.ecs;
import gse.log;

import :system;
import :vbd_constraints;
import :vbd_gpu_solver;
import :vbd_contact_trace;

auto gse::physics::contact_trace::run(data& d, const shared_view<physics::data> phys) -> async::task<> {
	if (d.body == 0 || !gpu_solver_active(phys)) {
		co_return;
	}

	const auto generation = phys.gpu_solver.retired_generation();
	if (generation == 0 || generation == d.last_generation) {
		co_return;
	}
	d.last_generation = generation;

	if (phys.gpu_solver.solver_cfg().trace_hashes == 0 && !d.hashes_warned) {
		d.hashes_warned = true;
		log::println(
			log::level::warning,
			log::category::physics,
			"contact trace: Physics.trace_hashes is off, so the per-tick pass-hash line will read all zeros; enable it to trace hashes"
		);
	}

	const auto it = phys.id_to_body_index.find(generate_temp_id(d.body));
	if (it == phys.id_to_body_index.end()) {
		if (!d.missing_reported) {
			d.missing_reported = true;
			log::println(
				log::level::warning,
				log::category::physics,
				"contact trace: body {} is not in the solver body map",
				d.body
			);
		}
		co_return;
	}
	const auto body_index = it->second;

	const auto bodies = phys.gpu_solver.read_body_states();
	if (body_index >= bodies.size()) {
		co_return;
	}

	const auto contacts = phys.gpu_solver.read_contact_dump();
	const auto grounded = phys.gpu_solver.read_grounded();
	const auto diag = phys.gpu_solver.diagnostics();

	if (diag.attempted_contacts > contacts.size() && !d.overflow_reported) {
		d.overflow_reported = true;
		log::println(
			log::level::warning,
			log::category::physics,
			"contact trace: {} contacts exceed the {}-entry dump, so some of this body's contacts may be missing from the trace",
			diag.attempted_contacts,
			contacts.size()
		);
	}

	const auto word = body_index / 32u;
	const auto grounded_bit = word < grounded.size() ? (grounded[word] >> (body_index % 32u)) & 1u : 0u;

	const auto& body = bodies[body_index];
	log::println(
		log::category::physics,
		"contact trace gen {} body {} idx {}: pos {:.6f} vel {:.6f} avel {:.6f} aw {:.2f} sleep {} grounded {} contacts {}",
		generation,
		d.body,
		body_index,
		body.position,
		body.velocity,
		body.angular_velocity,
		body.accel_weight,
		body.sleep_counter,
		grounded_bit,
		diag.attempted_contacts
	);

	const auto narrow_debug = phys.gpu_solver.read_narrow_phase_debug();
	if (narrow_debug.size() >= vbd::limits.collision_state_header_uints) {
		const auto* hashes = narrow_debug.data() + vbd::limits.state_pass_hash_base_index;
		const auto n = vbd::limits.pass_hash_stage_count;
		std::string line;
		for (std::uint32_t s = 0; s < 2; ++s) {
			line += s == 0 ? "s0" : " s1";
			for (std::uint32_t k = 0; k < n; ++k) {
				line += std::format(" {:08x}", hashes[s * n + k]);
			}
		}
		line += std::format(" pregrid {:08x} regrid {:08x} entry {:08x} warm0 {:08x} warm1 {:08x} adj {:08x} readj {:08x} colors {:08x} recolors {:08x} colors2 {:08x} reentry {:08x} srcbody {:08x} dstbody {:08x} entryalt {:08x} bodyentry {:08x}", hashes[2 * n], hashes[2 * n + 1], hashes[2 * n + 2], hashes[2 * n + 3], hashes[2 * n + 4], hashes[2 * n + 5], hashes[2 * n + 6], hashes[2 * n + 7], hashes[2 * n + 8], hashes[2 * n + 9], hashes[2 * n + 10], hashes[2 * n + 11], hashes[2 * n + 12], hashes[2 * n + 13], hashes[2 * n + 14]);
		log::println(
			log::category::physics,
			"contact trace gen {} pass hashes: {}",
			generation,
			line
		);
	}

	for (std::uint32_t ci = 0; ci < contacts.size(); ++ci) {
		const auto& c = contacts[ci];
		if (c.body_a == c.body_b) {
			continue;
		}
		if (c.body_a != body_index && c.body_b != body_index) {
			continue;
		}
		log::println(
			log::category::physics,
			"contact trace gen {} ci {}: a {} b {} feat {:#x} stick {} lambda {:.6f} pen {:.1f} floor {:.1f} c0 {:.6f} n {:.4f} appr {:.6f} ra {:.6f} rb {:.6f}",
			generation,
			ci,
			c.body_a,
			c.body_b,
			c.feature_key,
			c.sticking,
			c.lambda,
			c.penalty,
			c.penalty_floor,
			c.c0,
			c.normal,
			c.approach_speed,
			c.local_anchor_a,
			c.local_anchor_b
		);
	}

	co_return;
}
