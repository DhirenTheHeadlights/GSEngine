module gse.physics:vbd_cpu_contact_trace_impl;

import std;

import gse.core;
import gse.math;
import gse.concurrency;
import gse.ecs;
import gse.log;

import :system;
import :vbd_constraint_graph;
import :vbd_constraints;
import :vbd_solver;
import :vbd_cpu_contact_trace;

auto gse::physics::cpu_contact_trace::run(data& d, const shared_view<physics::data> phys) -> async::task<> {
	if (d.body == 0 || gpu_solver_active(phys)) {
		co_return;
	}

	const auto tick = d.tick++;

	const auto owners = phys.vbd_solver.body_ids();
	const auto bodies = phys.vbd_solver.body_states();

	std::uint32_t body_index = 0;
	bool found = false;
	for (std::uint32_t i = 0; i < owners.size() && i < bodies.size(); ++i) {
		if (owners[i].number() == d.body) {
			body_index = i;
			found = true;
			break;
		}
	}

	if (!found) {
		if (!owners.empty() && d.reports++ < 8) {
			std::string keys;
			for (std::uint32_t i = 0; i < owners.size(); ++i) {
				keys += std::format(" {}:{}", owners[i].number(), i);
			}
			log::println(
				log::level::warning,
				log::category::physics,
				"cpu contact trace tick {}: body {} not among the solver's {} bodies (contacts {}) owners{}",
				tick,
				d.body,
				owners.size(),
				phys.vbd_solver.graph().contact_constraints().size(),
				keys
			);
		}
		co_return;
	}

	const auto contacts = phys.vbd_solver.graph().contact_constraints();

	std::uint32_t touching = 0;
	for (const auto& c : contacts) {
		if (c.body_a != c.body_b && (c.body_a == body_index || c.body_b == body_index)) {
			++touching;
		}
	}

	const auto& body = bodies[body_index];
	log::println(
		log::category::physics,
		"cpu contact trace tick {} body {} idx {}: pos {:.8f} vel {:.8f} avel {:.8f} sleep {} contacts {} of {}",
		tick,
		d.body,
		body_index,
		body.position,
		body.velocity,
		body.angular_velocity,
		body.sleep_counter,
		touching,
		contacts.size()
	);

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
			"cpu contact trace tick {} ci {}: a {} b {} feat {:#x} stick {} replayed {} lambda {:.8f} pen {:.1f} floor {:.1f} c0 {:.8f} n {:.6f} appr {:.8f} ra {:.8f} rb {:.8f}",
			tick,
			ci,
			c.body_a,
			c.body_b,
			c.feature_key,
			c.sticking,
			c.replayed,
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
