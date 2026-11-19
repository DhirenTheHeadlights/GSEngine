module gse.gpu:shared_output_impl;

import gse.assert;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.time;
import std;

import :backend_state;
import :context;
import :device;
import :frame_output;
import :render_graph;
import :shared_output;
import :swap_chain;

namespace gse::gpu {
	constexpr time shared_slot_timeout = seconds(2.f);

	auto create_shared_ring(
		shared_output_state& s,
		render_graph& graph,
		vec2u extent
	) -> void;
}

auto gse::gpu::create_shared_ring(shared_output_state& s, render_graph& graph, const vec2u extent) -> void {
	const auto created = graph.set_offscreen_output({ .extent = extent, .slots = shared_output_slots, .exportable = true });
	assert(created.has_value(), "shared output: ring creation failed: {}", created ? std::string() : created.error());
	++s.revision;
	log::println(log::category::render, "shared output: created {}-surface ring at {}x{} on {} (revision {})", shared_output_slots, extent.x(), extent.y(), active_backend, s.revision);
}

auto gse::gpu::context::request_shared_output(data& d) -> void {
	d.shared_output.stage.enter(shared_output_stage::requested);
}

auto gse::gpu::context::abandon_shared_output(data& d) -> void {
	d.shared_output.stage.enter(shared_output_stage::abandoned);
}

auto gse::gpu::context::await_shared_output_slot(data& d) -> void {
	if (d.shared_output.stage.state() != shared_output_stage::ready) {
		return;
	}
	const clock wait_timer;
	(void)d.render_graph->wait_output_ready(shared_slot_timeout);
	d.shared_output.waited += wait_timer.elapsed();
}

auto gse::gpu::context::shared_output(const data& d) -> std::optional<shared_output_announcement> {
	if (d.shared_output.stage.state() != shared_output_stage::ready) {
		return std::nullopt;
	}
	return shared_output_announcement{
		.revision = d.shared_output.revision,
		.output = *d.render_graph->exported_output(),
	};
}

auto gse::gpu::context::advance_shared_output(data& d) -> void {
	auto& s = d.shared_output;
	const shared_output_stage stage = s.stage.state();
	if (stage == shared_output_stage::off || stage == shared_output_stage::abandoned || !d.swapchain) {
		return;
	}

	if (const vec2u extent = d.swapchain->extent(); extent.x() > 0 && extent.y() > 0) {
		if (stage == shared_output_stage::requested) {
			if (!d.device->capabilities().shared_surface_export) {
				log::println(log::category::runtime, "attached: the {} device cannot export shared surfaces; presenting into the game's own window", active_backend);
				s.stage.enter(shared_output_stage::abandoned);
				return;
			}
			create_shared_ring(s, *d.render_graph, extent);
			s.stage.enter(shared_output_stage::ready);
		}
		else if (extent != d.render_graph->exported_output()->extent) {
			create_shared_ring(s, *d.render_graph, extent);
		}
	}

	if (s.stage.state() != shared_output_stage::ready) {
		return;
	}

	if (d.render_graph->claim_output()) {
		++s.frames_presented;
	}
	else {
		++s.frames_withheld;
	}

	if (s.report.tick()) {
		log::println(
			log::category::runtime,
			"attached surface: {} frames handed to the editor, {} rendered but withheld because no slot was free, {:.1f:ms} blocked on the editor in the last window",
			s.frames_presented,
			s.frames_withheld,
			s.waited
		);
		s.frames_presented = 0;
		s.frames_withheld = 0;
		s.waited = {};
	}
}
