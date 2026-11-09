export module gse.physics:grass_probe;

import std;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;

import :grass_field;
import :grass_system;
import :system;

export namespace gse::physics::grass {
	enum class probe_kind : std::uint8_t {
		digest,
		track_start,
		track_stop
	};

	struct probe_request {
		probe_kind kind = probe_kind::digest;
		time recovery_window = seconds(5.f);
		float flatten_ratio = 0.3f;
		id reaction_body;
	};

	struct probe_state {
		std::uint64_t digest = 0;
		std::uint32_t digest_step = 0;
		bool digested = false;
		std::uint32_t under_samples = 0;
		float under_max_tip_ratio = 0.f;
		std::uint32_t blades_touched = 0;
		std::uint32_t recovery_checks = 0;
		float recovery_min_tip_ratio = 1.f;
		std::uint32_t max_cell_bodies = 0;
		length calm_max_tip_drift;
		velocity calm_max_tip_speed;
		std::uint32_t awake_tiles = 0;
		std::uint32_t slot_overflow = 0;
		std::array<std::uint32_t, 4> under_ratio_bins{};
		std::uint32_t under_tips_inside = 0;
		length under_max_tip_penetration;
		length under_max_tip_offset;
		std::uint32_t planted_samples = 0;
		float planted_max_tip_ratio = 0.f;
		std::uint32_t airborne_samples = 0;
		float airborne_max_tip_ratio = 0.f;
		std::uint32_t contact_recovery_checks = 0;
		float contact_recovery_min_tip_ratio = 1.f;
		time max_contact_after_cover;
		std::uint32_t early_sleeps = 0;
		std::uint32_t high_tips_under = 0;
		std::uint32_t high_tips_escaped = 0;
		vec3<force> reaction;
	};
}

export namespace gse::physics::grass::probe {
	struct probe_pass {};

	struct field_probe {
		gpu::buffer counters;
		gpu::buffer last_under;
		gpu::buffer last_contact;
		gpu::readback_channel readback;
		std::uint32_t blade_count = 0;
		std::uint64_t generation = 0;
		std::uint64_t applied_generation = 0;
		bool cleared = false;
	};

	struct [[= system_state<"GrassProbe">{}]] data {
		gpu::shader_program pipeline;

		std::flat_map<id, field_probe> fields;
		std::flat_set<id> state_requested;
		std::uint32_t step = 0;
		std::uint32_t recovery_steps = 0;
		float flatten_ratio = 0.f;
		id reaction_body;
		std::uint32_t reaction_index = std::numeric_limits<std::uint32_t>::max();
		bool tracking = false;
		bool track_reset = false;
		bool digest_due = false;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto update(
		context& ctx,
		data& d,
		channel_read<probe_request> requests_in,
		channel_read<interpolation_state> interp_in,
		shared_view<physics::data> phys,
		read<grass_field> fields,
		write<probe_state> states
	) -> async::task<>;

	[[= system_frame{}]]
	[[= runs_after<^^grass::data>{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<grass::data> grass_s,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}
