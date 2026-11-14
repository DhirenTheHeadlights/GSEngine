module sandbox:grass_probe_impl;

import std;

import gse;
import gse.gpu;
import gse.gpu_record;

import :grass_field;
import :grass_probe;
import :grass_system;

namespace sandbox::grass {
	using namespace gse;
	using namespace gse::physics;
}

namespace sandbox::grass::probe {
	struct probe_counters {
		std::uint32_t digest_a = 0;
		std::uint32_t digest_b = 0;
		std::uint32_t digest_step = 0;
		std::uint32_t digest_valid = 0;
		std::uint32_t under_samples = 0;
		std::uint32_t under_max_tip_ratio_bits = 0;
		std::uint32_t blades_touched = 0;
		std::uint32_t recovery_checks = 0;
		std::uint32_t recovery_max_deficit_bits = 0;
		std::uint32_t max_cell_bodies = 0;
		std::uint32_t calm_max_drift_bits = 0;
		std::uint32_t calm_max_speed_bits = 0;
		std::uint32_t awake_tiles = 0;
		std::uint32_t slot_overflow = 0;
		std::uint32_t under_bin_30 = 0;
		std::uint32_t under_bin_60 = 0;
		std::uint32_t under_bin_90 = 0;
		std::uint32_t under_bin_rest = 0;
		std::uint32_t under_tip_inside = 0;
		std::uint32_t under_max_penetration_bits = 0;
		std::uint32_t under_max_offset_bits = 0;
		std::uint32_t planted_samples = 0;
		std::uint32_t planted_max_ratio_bits = 0;
		std::uint32_t airborne_samples = 0;
		std::uint32_t airborne_max_ratio_bits = 0;
		std::uint32_t contact_recovery_checks = 0;
		std::uint32_t contact_recovery_max_deficit_bits = 0;
		std::uint32_t contact_after_cover_steps = 0;
		std::uint32_t early_sleeps = 0;
		std::uint32_t high_under = 0;
		std::uint32_t high_escaped = 0;
		std::array<std::uint32_t, vbd::limits.external_force_words> reaction{};
	};

	template <std::meta::info Member>
	consteval auto counter_slot() -> std::uint32_t {
		return static_cast<std::uint32_t>(std::meta::offset_of(Member).bytes / sizeof(std::uint32_t));
	}

	struct [[= shaders::shader_constant_block]] probe_slots {
		std::uint32_t probe_digest_a = counter_slot<^^probe_counters::digest_a>();
		std::uint32_t probe_digest_b = counter_slot<^^probe_counters::digest_b>();
		std::uint32_t probe_digest_step = counter_slot<^^probe_counters::digest_step>();
		std::uint32_t probe_digest_valid = counter_slot<^^probe_counters::digest_valid>();
		std::uint32_t probe_under_samples = counter_slot<^^probe_counters::under_samples>();
		std::uint32_t probe_under_max_ratio = counter_slot<^^probe_counters::under_max_tip_ratio_bits>();
		std::uint32_t probe_blades_touched = counter_slot<^^probe_counters::blades_touched>();
		std::uint32_t probe_recovery_checks = counter_slot<^^probe_counters::recovery_checks>();
		std::uint32_t probe_recovery_deficit = counter_slot<^^probe_counters::recovery_max_deficit_bits>();
		std::uint32_t probe_max_cell_bodies = counter_slot<^^probe_counters::max_cell_bodies>();
		std::uint32_t probe_calm_max_drift = counter_slot<^^probe_counters::calm_max_drift_bits>();
		std::uint32_t probe_calm_max_speed = counter_slot<^^probe_counters::calm_max_speed_bits>();
		std::uint32_t probe_awake_tiles = counter_slot<^^probe_counters::awake_tiles>();
		std::uint32_t probe_slot_overflow = counter_slot<^^probe_counters::slot_overflow>();
		std::uint32_t probe_under_bin_30 = counter_slot<^^probe_counters::under_bin_30>();
		std::uint32_t probe_under_bin_60 = counter_slot<^^probe_counters::under_bin_60>();
		std::uint32_t probe_under_bin_90 = counter_slot<^^probe_counters::under_bin_90>();
		std::uint32_t probe_under_bin_rest = counter_slot<^^probe_counters::under_bin_rest>();
		std::uint32_t probe_under_tip_inside = counter_slot<^^probe_counters::under_tip_inside>();
		std::uint32_t probe_under_max_penetration = counter_slot<^^probe_counters::under_max_penetration_bits>();
		std::uint32_t probe_under_max_offset = counter_slot<^^probe_counters::under_max_offset_bits>();
		std::uint32_t probe_planted_samples = counter_slot<^^probe_counters::planted_samples>();
		std::uint32_t probe_planted_max_ratio = counter_slot<^^probe_counters::planted_max_ratio_bits>();
		std::uint32_t probe_airborne_samples = counter_slot<^^probe_counters::airborne_samples>();
		std::uint32_t probe_airborne_max_ratio = counter_slot<^^probe_counters::airborne_max_ratio_bits>();
		std::uint32_t probe_contact_recovery_checks = counter_slot<^^probe_counters::contact_recovery_checks>();
		std::uint32_t probe_contact_recovery_deficit = counter_slot<^^probe_counters::contact_recovery_max_deficit_bits>();
		std::uint32_t probe_contact_after_cover = counter_slot<^^probe_counters::contact_after_cover_steps>();
		std::uint32_t probe_early_sleeps = counter_slot<^^probe_counters::early_sleeps>();
		std::uint32_t probe_high_under = counter_slot<^^probe_counters::high_under>();
		std::uint32_t probe_high_escaped = counter_slot<^^probe_counters::high_escaped>();
		std::uint32_t probe_reaction = counter_slot<^^probe_counters::reaction>();
		std::uint32_t probe_checked_bit = 0x80000000u;
		std::uint32_t probe_early_bit = 0x40000000u;
		std::uint32_t probe_stamp_mask = 0x3fffffffu;
		std::uint32_t probe_no_body = 0xffffffffu;
	};

	struct [[= shaders::shader_struct]] probe_push_constants {
		std::uint32_t step;
		std::uint32_t recovery_steps;
		std::uint32_t digest;
		std::uint32_t track;
		std::uint32_t reset;
		float flatten_ratio;
		std::uint32_t reaction_body;
	};

	struct probe_bindings {
		shaders::ssbo_readonly<grass_params> params_data;
		shaders::ssbo_readonly<vbd::body_state> body_data;
		shaders::ssbo_readonly<std::uint32_t> grid_read_data;
		shaders::ssbo_readonly<grass_node> node_data;
		shaders::ssbo_readonly<grass_tile> tile_data;
		shaders::ssbo_readonly<grass_status> status_data;
		shaders::ssbo_readwrite<std::uint32_t> counter_data;
		shaders::ssbo_readwrite<std::uint32_t> last_under_data;
		shaders::ssbo_readonly<std::uint32_t> contact_read_data;
		shaders::ssbo_readwrite<std::uint32_t> last_contact_data;
		shaders::ssbo_readonly<std::uint32_t> reaction_read_data;
	};

	using probe_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_probe">,
		gpu::types<vbd::shader_types, shader_types, type_pack<probe_slots>>,
		gpu::bindings<probe_bindings>,
		gpu::helpers<"VBDPhysics/vbd_shared", "Grass/grass_shape", "Grass/grass_shared">,
		gpu::threads<layout.grass_sim_threads>,
		gpu::push_constant<probe_push_constants>
	>;

	constexpr std::size_t digest_clear_bytes = std::meta::offset_of(^^probe_counters::under_samples).bytes;

	auto create_field_probe(
		gpu::device& device,
		gpu::render_graph& graph,
		std::uint32_t blade_count,
		field_probe& fp
	) -> void;

	auto apply_counters(
		const probe_counters& c,
		probe_state& state,
		state_digest& digest
	) -> void;
}

auto sandbox::grass::probe::create_field_probe(gpu::device& device, gpu::render_graph& graph, const std::uint32_t blade_count, field_probe& fp) -> void {
	constexpr gpu::buffer_usage counter_usage{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_src, gpu::buffer_flag::transfer_dst };
	constexpr gpu::buffer_usage stamp_usage{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst };

	fp.counters = device.create_buffer(
		{
			.size = sizeof(probe_counters),
			.stride = sizeof(std::uint32_t),
			.usage = counter_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.probe.counters"
	);
	fp.last_under = device.create_buffer(
		{
			.size = static_cast<std::size_t>(blade_count) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = stamp_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.probe.last_under"
	);
	fp.last_contact = device.create_buffer(
		{
			.size = static_cast<std::size_t>(blade_count) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = stamp_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.probe.last_contact"
	);
	fp.readback = graph.create_readback_channel(sizeof(probe_counters), "grass.probe.readback");
	fp.blade_count = blade_count;
	fp.cleared = false;
}

auto sandbox::grass::probe::apply_counters(const probe_counters& c, probe_state& state, state_digest& digest) -> void {
	if (c.digest_valid != 0) {
		digest = {
			.value = static_cast<std::uint64_t>(c.digest_b) << 32 | c.digest_a,
			.step = c.digest_step,
			.taken = true,
		};
	}
	state.under_samples = c.under_samples;
	state.under_max_tip_ratio = std::bit_cast<float>(c.under_max_tip_ratio_bits);
	state.blades_touched = c.blades_touched;
	state.recovery_checks = c.recovery_checks;
	state.recovery_min_tip_ratio = 1.f - std::bit_cast<float>(c.recovery_max_deficit_bits);
	state.max_cell_bodies = c.max_cell_bodies;
	state.calm_max_tip_drift = meters(std::bit_cast<float>(c.calm_max_drift_bits));
	state.calm_max_tip_speed = meters_per_second(std::bit_cast<float>(c.calm_max_speed_bits));
	state.awake_tiles = c.awake_tiles;
	state.slot_overflow = c.slot_overflow;
	state.under_ratio_bins = { c.under_bin_30, c.under_bin_60, c.under_bin_90, c.under_bin_rest };
	state.under_tips_inside = c.under_tip_inside;
	state.under_max_tip_penetration = meters(std::bit_cast<float>(c.under_max_penetration_bits));
	state.under_max_tip_offset = meters(std::bit_cast<float>(c.under_max_offset_bits));
	state.planted_samples = c.planted_samples;
	state.planted_max_tip_ratio = std::bit_cast<float>(c.planted_max_ratio_bits);
	state.airborne_samples = c.airborne_samples;
	state.airborne_max_tip_ratio = std::bit_cast<float>(c.airborne_max_ratio_bits);
	state.contact_recovery_checks = c.contact_recovery_checks;
	state.contact_recovery_min_tip_ratio = 1.f - std::bit_cast<float>(c.contact_recovery_max_deficit_bits);
	state.max_contact_after_cover = system_clock::fixed_dt() * static_cast<float>(c.contact_after_cover_steps);
	state.early_sleeps = c.early_sleeps;
	state.high_tips_under = c.high_under;
	state.high_tips_escaped = c.high_escaped;

	const auto decode = [&c](const std::size_t axis) {
		const auto bits = static_cast<std::uint64_t>(c.reaction[2 * axis + 1]) << 32 | c.reaction[2 * axis];
		return vbd::limits.external_force_resolution * static_cast<float>(std::bit_cast<std::int64_t>(bits));
	};
	state.reaction = vec3<force>(decode(0), decode(1), decode(2));
}

auto sandbox::grass::probe::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_program(*gpu_s.device, probe_entry::pod);
	return {};
}

auto sandbox::grass::probe::update(context& ctx, data& d, const channel_read<probe_request, state_digest_request> requests_in, const channel_read<interpolation_state> interp_in, const shared_view<physics::data> phys, read<grass_field> fields, write<probe_state> states, write<state_digest> digests) -> async::task<> {
	if (const auto& published = interp_in.of<interpolation_state>(); !published.empty()) {
		d.step += static_cast<std::uint32_t>(std::max(published[0].steps, 0));
	}

	d.digest_due = !requests_in.of<state_digest_request>().empty();
	for (const auto& request : requests_in.of<probe_request>()) {
		switch (request.kind) {
			case probe_kind::track_start: {
				d.tracking = true;
				d.track_reset = true;
				d.recovery_steps = static_cast<std::uint32_t>(std::lround(request.recovery_window / system_clock::fixed_dt()));
				d.flatten_ratio = request.flatten_ratio;
				d.reaction_body = request.reaction_body;
				break;
			}
			case probe_kind::track_stop: {
				d.tracking = false;
				break;
			}
		}
	}

	const auto reaction_index = phys.id_to_body_index.find(d.reaction_body);
	d.reaction_index = reaction_index != phys.id_to_body_index.end() ? reaction_index->second : probe_slots{}.probe_no_body;

	std::erase_if(d.fields, [&](const auto& entry) {
		return fields.find(entry.first) == nullptr;
	});
	std::erase_if(d.state_requested, [&](const id owner) {
		return fields.find(owner) == nullptr;
	});

	for (const auto owner : fields.owner_ids()) {
		auto* state = states.find(owner);
		auto* digest = digests.find(owner);
		if (state == nullptr || digest == nullptr) {
			if (d.state_requested.insert(owner).second) {
				ctx.add_component<probe_state>(owner);
				ctx.add_component<state_digest>(owner);
			}
			continue;
		}

		const auto found = d.fields.find(owner);
		if (found == d.fields.end()) {
			continue;
		}
		auto& fp = found->second;
		const auto view = fp.readback.latest();
		if (view.generation <= fp.applied_generation) {
			continue;
		}
		fp.applied_generation = view.generation;

		probe_counters counters;
		std::memcpy(&counters, view.bytes.data(), sizeof(counters));
		apply_counters(counters, *state, *digest);
	}

	return {};
}

auto sandbox::grass::probe::frame(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d, const shared_view<grass::data> grass_s, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!d.tracking && !d.digest_due) {
		co_return;
	}

	std::vector<id> probed;
	for (const auto& [owner, fb] : grass_s.buffers) {
		auto& fp = d.fields[owner];
		if (fp.blade_count != fb.blade_count) {
			create_field_probe(*gpu_s.device, *gpu_s.render_graph, fb.blade_count, fp);
		}
		probed.push_back(owner);
	}

	if (probed.empty()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	auto rec = co_await gpu::pass<^^probe_pass>(pass_out)
		.pipeline(d.pipeline)
		.after<^^grid_pass, ^^simulate_pass>();

	for (const auto owner : probed) {
		const auto& fb = grass_s.buffers.at(owner);
		auto& fp = d.fields.at(owner);

		const bool reset = d.track_reset || !fp.cleared;
		if (reset) {
			rec.fill_buffer(fp.counters, 0, fp.counters.size());
			rec.fill_buffer(fp.last_under, 0, fp.last_under.size());
			rec.fill_buffer(fp.last_contact, 0, fp.last_contact.size());
			fp.cleared = true;
		}
		else {
			rec.fill_buffer(fp.counters, 0, digest_clear_bytes);
		}

		rec.dispatch<probe_entry>(
			{
				.step = d.step,
				.recovery_steps = d.recovery_steps,
				.digest = d.digest_due ? 1u : 0u,
				.track = d.tracking ? 1u : 0u,
				.reset = reset ? 1u : 0u,
				.flatten_ratio = d.flatten_ratio,
				.reaction_body = d.reaction_index,
			},
			{
				.params_data = fb.params[frame_index],
				.body_data = grass_s.bodies.slot,
				.grid_read_data = fb.grid,
				.node_data = fb.nodes,
				.tile_data = fb.tiles,
				.status_data = fb.status,
				.counter_data = fp.counters,
				.last_under_data = fp.last_under,
				.contact_read_data = fb.contact,
				.last_contact_data = fp.last_contact,
				.reaction_read_data = grass_s.reaction_views[frame_index],
			},
			vec3u{ (layout.grass_tile_blade_count + layout.grass_sim_threads - 1) / layout.grass_sim_threads, fb.tile_count, 1u }
		);

		rec.copy_buffer(fp.counters, fp.readback.publish_target(++fp.generation, sizeof(probe_counters)), sizeof(probe_counters));
	}

	d.track_reset = false;
}
