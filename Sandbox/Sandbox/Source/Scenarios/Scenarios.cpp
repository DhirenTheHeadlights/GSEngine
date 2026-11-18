module sandbox:scenarios_impl;

import std;
import gse;
import gse.scenario;

import :ambient_sound;
import :audio_fixtures;
import :dev_spawn_system;
import :grass_field;
import :grass_probe;
import :grass_system;
import :scenario_camera;
import :scenarios;

namespace sandbox::scenarios {
	using namespace scenario_camera;

	constexpr gse::vec2i parity_extent{ 1280, 720 };

	auto build_stress_workload(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto build_audio_stress_workload(
		gse::scenario::context& ctx,
		std::chrono::milliseconds frame_pause
	) -> gse::async::task<>;

	struct audio_probe_segment {
		std::string_view kind;
		gse::vec3<gse::position> from;
		gse::vec3<gse::velocity> velocity;
		std::uint64_t frames = 45;
	};

	auto audio_probe_segments() -> std::array<audio_probe_segment, 9>;

	auto run_audio_spatial_script(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	struct audio_occlusion_segment {
		std::string_view kind;
		audio_occlusion_station station = audio_occlusion_station::wall;
		gse::vec3<gse::displacement> from;
		std::uint64_t lead_frames = 60;
		std::uint64_t travel_frames = 0;
		std::uint64_t tail_frames = 0;
	};

	auto run_audio_occlusion_script(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto wait_for_ambient_clips(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto run_audio_yard_solo(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto run_audio_contact_script(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto run_audio_modal_script(
		gse::scenario::context& ctx,
		audio_modal_material material
	) -> gse::async::task<>;

	struct audio_rub_probe {
		std::string_view label;
		std::string_view body;
		gse::length reach;
		std::array<gse::vec3<gse::velocity>, 3> launches;
		std::uint64_t lead_frames = 0;
	};

	auto open_audio_rub_probe(
		gse::scenario::context& ctx,
		std::string_view label
	) -> gse::async::task<>;

	auto run_audio_rub_launches(
		gse::scenario::context& ctx,
		audio_rub_probe probe
	) -> gse::async::task<>;

	struct audio_reverb_visit {
		audio_reverb_station station = audio_reverb_station::wood;
		std::uint64_t ring_frames = 0;
	};

	auto run_audio_doorway_script(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto capture_parity_frame(
		gse::scenario::context& ctx,
		std::string_view name,
		gse::camera::target target
	) -> gse::async::task<>;

	auto push_pyramid_impulses(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto press_key(
		gse::scenario::context& ctx,
		gse::key key
	) -> void;

	auto release_key(
		gse::scenario::context& ctx,
		gse::key key
	) -> void;

	auto walk_pattern(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto find_proxy_owner(
		gse::scenario::context& ctx
	) -> std::optional<gse::id>;

	auto find_proxy(
		gse::scenario::context& ctx
	) -> const gse::physics::transform_component*;

	auto expect_bodies_sane(
		gse::scenario::context& ctx
	) -> void;

	auto sample_body_positions(
		gse::scenario::context& ctx
	) -> std::flat_map<gse::id, gse::vec3<gse::position>>;

	auto displaced_bodies(
		const std::flat_map<gse::id, gse::vec3<gse::position>>& from,
		const std::flat_map<gse::id, gse::vec3<gse::position>>& to,
		gse::length tolerance
	) -> std::size_t;

	auto expect_reset_restores_spawn_poses(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto record_static_shot(
		gse::scenario::context& ctx,
		std::uint64_t settle_frames,
		std::uint64_t record_frames
	) -> gse::async::task<>;

	auto pitch_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto hold_cloud_probe(
		gse::scenario::context& ctx,
		gse::camera::target target,
		std::uint64_t frames
	) -> gse::async::task<>;

	auto capture_cloud_probe(
		gse::scenario::context& ctx,
		const std::filesystem::path& file
	) -> void;

	constexpr std::string_view persist_probe_category = "WebPersistProbe";
	constexpr std::string_view persist_probe_key = "token";
	constexpr std::string_view persist_probe_token = "7f3a9c";

	auto persist_probe_settings_path(
	) -> std::filesystem::path;

	auto persist_probe_save_path(
	) -> std::filesystem::path;

	auto persist_probe_settings(
	) -> std::string;

	auto persist_probe_save(
	) -> std::string;
}

auto sandbox::scenarios::build_stress_workload(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_stress_request>({});
	co_await gse::scenario::wait_frames(ctx, 1);
	ctx.channels().push<spawn_joints_request>({});
}

auto sandbox::scenarios::physics_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::physics_grow(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t wave_frames = 120;

	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_joints_request>({});
	co_await gse::scenario::wait_frames(ctx, wave_frames);
	expect_bodies_sane(ctx);

	ctx.channels().push<spawn_stress_request>({});
	co_await gse::scenario::wait_frames(ctx, wave_frames);
	expect_bodies_sane(ctx);

	ctx.channels().push<spawn_pyramid_request>({});
	co_await gse::scenario::wait_frames(ctx, wave_frames);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::grass_sleep_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::grass_render(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::grass_walk(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr float flatten_ratio = 0.3f;
	constexpr float recovered_ratio = 0.9f;
	constexpr std::uint32_t min_under_samples = 50;
	const auto recovery_window = gse::seconds(5.f);
	const auto walk_timeout = gse::seconds(6.f);
	const auto walk_depth = gse::meters(8.f);
	const auto sample_interval = gse::seconds(0.5f);
	const auto exit_margin = gse::meters(1.5f);
	const auto inside_margin = gse::meters(0.5f);
	const auto outside_margin = gse::meters(1.f);
	const auto no_force = gse::newtons(0.f);
	constexpr float max_weight_share = 0.1f;
	constexpr std::uint32_t min_inside_samples = 4;
	constexpr std::uint32_t min_outside_samples = 1;

	co_await gse::scenario::wait_settled(ctx);

	const auto fields = ctx.owners<sandbox::grass_field>();
	if (!ctx.expect(fields.size() == 1)) {
		co_return;
	}
	const auto field_owner = fields[0];
	const auto* field = ctx.try_component<sandbox::grass_field>(field_owner);
	if (!ctx.expect(field != nullptr)) {
		co_return;
	}
	const auto half_extent = sandbox::grass::field_extent(*field) * 0.5f;
	const auto contact_stiffness = field->contact_stiffness;

	ctx.channels().push<prefetch_character_request>({});
	co_await gse::scenario::wait(ctx, gse::seconds(2.f));
	ctx.channels().push<spawn_character_request>({});
	co_await gse::scenario::wait_frames(ctx, 2);
	const auto proxy_owner = find_proxy_owner(ctx);
	if (!ctx.expect(proxy_owner.has_value())) {
		co_return;
	}
	co_await gse::scenario::wait(ctx, gse::seconds(1.f));

	ctx.channels().push<sandbox::grass::probe_request>({
		.kind = sandbox::grass::probe_kind::track_start,
		.recovery_window = recovery_window,
		.flatten_ratio = flatten_ratio,
		.reaction_body = *proxy_owner,
	});

	std::uint32_t inside_samples = 0;
	std::uint32_t inside_pushed = 0;
	std::uint32_t outside_samples = 0;
	std::uint32_t outside_pushed = 0;
	auto peak_inside = no_force;
	const auto sample_reaction = [&](const std::string_view when, const gse::physics::transform_component& walker, const sandbox::grass::probe_state& sample) {
		const auto pushing = gse::magnitude(sample.reaction);
		const bool pushed = pushing > no_force;
		const auto z = walker.position.z();
		if (z < half_extent - inside_margin) {
			++inside_samples;
			inside_pushed += pushed ? 1u : 0u;
			peak_inside = std::max(peak_inside, pushing);
		}
		else if (z > half_extent + outside_margin) {
			++outside_samples;
			outside_pushed += pushed ? 1u : 0u;
		}
		gse::log::println(gse::log::category::general, "grass_walk: {} z {} grass force on the character {:.3f}", when, z, sample.reaction);
	};

	co_await gse::scenario::wait(ctx, sample_interval);
	if (const auto* walker = find_proxy(ctx); walker != nullptr) {
		if (const auto* sample = ctx.try_component<sandbox::grass::probe_state>(field_owner)) {
			sample_reaction("before walking", *walker, *sample);
		}
	}

	std::uint32_t max_awake_tiles = 0;
	const auto walk_leg = [&](const gse::key key, const auto arrived) -> gse::async::task<> {
		press_key(ctx, key);
		for (gse::time walked = gse::seconds(0.f); walked < walk_timeout; walked += sample_interval) {
			co_await gse::scenario::wait(ctx, sample_interval);
			const auto* walker = find_proxy(ctx);
			if (walker == nullptr) {
				break;
			}
			if (const auto* sample = ctx.try_component<sandbox::grass::probe_state>(field_owner)) {
				max_awake_tiles = std::max(max_awake_tiles, sample->awake_tiles);
				gse::log::println(gse::log::category::general, "grass_walk: z {} awake tiles {} slot overflow {}", walker->position.z(), sample->awake_tiles, sample->slot_overflow);
				sample_reaction("walking", *walker, *sample);
			}
			if (arrived(walker->position.z())) {
				break;
			}
		}
		release_key(ctx, key);
	};
	co_await walk_leg(gse::key::w, [&](const auto z) {
		return z < half_extent - walk_depth;
	});
	co_await walk_leg(gse::key::s, [&](const auto z) {
		return z > half_extent + exit_margin;
	});
	co_await gse::scenario::wait(ctx, recovery_window + gse::seconds(0.5f));
	bool recovered_pushed = true;
	if (const auto* walker = find_proxy(ctx); walker != nullptr) {
		if (const auto* sample = ctx.try_component<sandbox::grass::probe_state>(field_owner)) {
			sample_reaction("after recovery", *walker, *sample);
			recovered_pushed = gse::magnitude(sample->reaction) > no_force;
		}
	}
	ctx.channels().push<sandbox::grass::probe_request>({
		.kind = sandbox::grass::probe_kind::track_stop,
	});
	co_await gse::scenario::wait_frames(ctx, 30);

	const auto* probe = ctx.try_component<sandbox::grass::probe_state>(field_owner);
	const auto* proxy = find_proxy(ctx);
	const auto* proxy_motion = ctx.try_component<gse::physics::motion_component>(*proxy_owner);
	const auto* proxy_body = proxy_motion != nullptr ? std::get_if<gse::physics::dynamic_body>(&proxy_motion->body) : nullptr;
	if (!ctx.expect(probe != nullptr && proxy != nullptr && proxy_body != nullptr)) {
		co_return;
	}
	const auto proxy_weight = proxy_body->mass * gse::vbd::gravity_magnitude();

	gse::log::println(
		gse::log::category::general,
		"grass_walk: {} under-body samples, max tip ratio under a body {:.3f}, {} blades touched, {} recovery checks, min tip ratio {:.3f}, max bodies in a grid cell {}, max awake tiles {}, slot overflow {}, character ended at z {}",
		probe->under_samples,
		probe->under_max_tip_ratio,
		probe->blades_touched,
		probe->recovery_checks,
		probe->recovery_min_tip_ratio,
		probe->max_cell_bodies,
		max_awake_tiles,
		probe->slot_overflow,
		proxy->position.z()
	);
	gse::log::println(
		gse::log::category::general,
		"grass_walk: covered tip ratios <=0.3 {} <=0.6 {} <=0.9 {} >0.9 {}; tips inside a body {}, max penetration {} (penalty {}), max horizontal tip offset {}; tips above the limit still under the body {}, escaped beside it {}; planted {} samples max ratio {:.3f}, airborne {} samples max ratio {:.3f}",
		probe->under_ratio_bins[0],
		probe->under_ratio_bins[1],
		probe->under_ratio_bins[2],
		probe->under_ratio_bins[3],
		probe->under_tips_inside,
		probe->under_max_tip_penetration,
		contact_stiffness * probe->under_max_tip_penetration,
		probe->under_max_tip_offset,
		probe->high_tips_under,
		probe->high_tips_escaped,
		probe->planted_samples,
		probe->planted_max_tip_ratio,
		probe->airborne_samples,
		probe->airborne_max_tip_ratio
	);
	gse::log::println(
		gse::log::category::general,
		"grass_walk: recovery from last cover min {:.3f} over {} checks; from last contact min {:.3f} over {} checks; latest contact after cover {}; blades asleep inside their window {}",
		probe->recovery_min_tip_ratio,
		probe->recovery_checks,
		probe->contact_recovery_min_tip_ratio,
		probe->contact_recovery_checks,
		probe->max_contact_after_cover,
		probe->early_sleeps
	);
	gse::log::println(
		gse::log::category::general,
		"grass_walk: grass force on the character non-zero in {} of {} samples inside the field, in {} of {} samples outside it, after recovery {}; peak inside {} against a weight of {}",
		inside_pushed,
		inside_samples,
		outside_pushed,
		outside_samples,
		recovered_pushed ? "non-zero" : "zero",
		peak_inside,
		proxy_weight
	);

	ctx.expect(inside_samples >= min_inside_samples);
	ctx.expect(inside_pushed == inside_samples);
	ctx.expect(peak_inside <= proxy_weight * max_weight_share);
	ctx.expect(outside_samples >= min_outside_samples);
	ctx.expect(outside_pushed == 0);
	ctx.expect(!recovered_pushed);
	ctx.expect(probe->slot_overflow == 0);
	ctx.expect(probe->under_samples >= min_under_samples);
	ctx.expect(probe->under_max_tip_ratio <= flatten_ratio);
	ctx.expect(probe->blades_touched > 0);
	ctx.expect(probe->recovery_checks >= probe->blades_touched);
	ctx.expect(probe->recovery_min_tip_ratio >= recovered_ratio);
	ctx.expect(probe->contact_recovery_checks >= probe->blades_touched);
	ctx.expect(probe->contact_recovery_min_tip_ratio >= recovered_ratio);
	ctx.expect(probe->max_cell_bodies <= sandbox::grass::cell_capacity);
	ctx.expect(proxy->position.z() > half_extent);
}

auto sandbox::scenarios::grass_rigid_render(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::grass_look(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 240;
	constexpr std::uint64_t converge_frames = 240;
	constexpr std::uint64_t flush_frames = 70;
	const auto calm_drift = gse::millimeters(5.f);
	const auto calm_speed = gse::meters_per_second(0.005f);

	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, settle_frames);
	ctx.channels().push<sandbox::grass::probe_request>({
		.kind = sandbox::grass::probe_kind::track_start,
	});
	co_await gse::scenario::wait_frames(ctx, converge_frames);
	ctx.channels().push<sandbox::grass::probe_request>({
		.kind = sandbox::grass::probe_kind::track_stop,
	});
	ctx.channels().push<gse::renderer::capture::screenshot_request>({});
	co_await gse::scenario::wait_frames(ctx, flush_frames);
	ctx.channels().push<gse::renderer::capture::screenshot_request>({});
	co_await gse::scenario::wait_frames(ctx, flush_frames);

	const auto fields = ctx.owners<sandbox::grass_field>();
	if (!ctx.expect(fields.size() == 1)) {
		co_return;
	}
	const auto* probe = ctx.try_component<sandbox::grass::probe_state>(fields[0]);
	if (!ctx.expect(probe != nullptr)) {
		co_return;
	}

	gse::log::println(
		gse::log::category::general,
		"grass_look: untouched blades over {} frames, max tip drift from rest {}, max tip speed {}",
		converge_frames,
		probe->calm_max_tip_drift,
		probe->calm_max_tip_speed
	);

	ctx.expect(probe->calm_max_tip_drift <= calm_drift);
	ctx.expect(probe->calm_max_tip_speed <= calm_speed);
}

auto sandbox::scenarios::grass_pan(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t hold_frames = 180;
	constexpr std::uint64_t pan_frames = 120;
	constexpr std::uint64_t flush_frames = 70;
	const auto eye = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(1.4f), gse::meters(0.f));
	const auto pitch = gse::degrees(-15.f);
	const auto start = gse::degrees(0.f);
	const auto sweep = gse::degrees(120.f);
	const auto look = [&](const gse::angle yaw) -> gse::camera::target {
		return {
			.position = eye,
			.orientation = gse::from_axis_angle(gse::axis_y, yaw) * gse::from_axis_angle(gse::axis_x, pitch),
		};
	};

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < hold_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = look(start),
			.priority = 100,
			.blend_duration = {},
		});
		if (i == hold_frames - 1) {
			ctx.channels().push<gse::renderer::capture::screenshot_request>({});
		}
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	for (std::uint64_t i = 0; i < pan_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = look(start + sweep * (static_cast<float>(i) / static_cast<float>(pan_frames - 1))),
			.priority = 100,
			.blend_duration = {},
		});
		if (i == pan_frames / 2) {
			ctx.channels().push<gse::renderer::capture::screenshot_request>({});
		}
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	co_await gse::scenario::wait_frames(ctx, flush_frames);
}

auto sandbox::scenarios::grass_stride_look(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t walk_frames = 600;
	constexpr std::uint64_t hold_frames = 90;
	constexpr std::uint64_t flush_frames = 70;
	const auto stride_depth = gse::meters(3.f);
	const auto side_offset = gse::meters(2.5f);
	const auto eye_height = gse::meters(0.6f);
	const auto pitch = gse::degrees(-10.f);

	co_await gse::scenario::wait_settled(ctx);

	const auto fields = ctx.owners<sandbox::grass_field>();
	if (!ctx.expect(fields.size() == 1)) {
		co_return;
	}
	const auto* field = ctx.try_component<sandbox::grass_field>(fields[0]);
	if (!ctx.expect(field != nullptr)) {
		co_return;
	}
	const auto half_extent = sandbox::grass::field_extent(*field) * 0.5f;

	ctx.channels().push<prefetch_character_request>({});
	co_await gse::scenario::wait(ctx, gse::seconds(2.f));
	ctx.channels().push<spawn_character_request>({});
	co_await gse::scenario::wait_frames(ctx, 2);
	if (!ctx.expect(find_proxy(ctx) != nullptr)) {
		co_return;
	}
	co_await gse::scenario::wait(ctx, gse::seconds(1.f));

	const auto look_from_side = [&](const gse::physics::transform_component& walker, const float side) {
		ctx.channels().push<gse::camera::request>({
			.target = {
				.position = gse::vec3<gse::position>(walker.position.x() + side_offset * side, eye_height, walker.position.z()),
				.orientation = gse::from_axis_angle(gse::axis_y, gse::degrees(90.f) * side) * gse::from_axis_angle(gse::axis_x, pitch),
			},
			.priority = 100,
			.blend_duration = {},
		});
	};

	press_key(ctx, gse::key::w);
	for (std::uint64_t i = 0; i < walk_frames; ++i) {
		const auto* walker = find_proxy(ctx);
		if (walker == nullptr) {
			break;
		}
		look_from_side(*walker, 1.f);
		if (walker->position.z() < half_extent - stride_depth) {
			ctx.channels().push<gse::renderer::capture::screenshot_request>({});
			break;
		}
		co_await gse::scenario::wait_frames(ctx, 1);
	}
	release_key(ctx, gse::key::w);

	for (const float side : { 1.f, -1.f }) {
		for (std::uint64_t i = 0; i < hold_frames; ++i) {
			if (const auto* walker = find_proxy(ctx)) {
				look_from_side(*walker, side);
			}
			if (i == hold_frames - 1) {
				ctx.channels().push<gse::renderer::capture::screenshot_request>({});
			}
			co_await gse::scenario::wait_frames(ctx, 1);
		}
	}

	co_await gse::scenario::wait_frames(ctx, flush_frames);
	ctx.expect(find_proxy(ctx) != nullptr);
}

auto sandbox::scenarios::render_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::expect_bodies_sane(gse::scenario::context& ctx) -> void {
	const auto owners = ctx.owners<gse::physics::transform_component>();
	const auto transforms = ctx.components<gse::physics::transform_component>();
	const auto bound = gse::kilometers(10.f);

	std::size_t unsound = 0;
	for (std::size_t index = 0; index < transforms.size(); ++index) {
		const auto& position = transforms[index].position;
		if (gse::isfinite(position) && gse::magnitude(position) < bound) {
			continue;
		}
		++unsound;
		gse::log::println(
			gse::log::level::error,
			gse::log::category::physics,
			"scenario: body {} left the world at {}",
			owners[index],
			position
		);
	}

	ctx.expect(!transforms.empty());
	ctx.expect_eq(unsound, std::size_t{ 0 });
}

auto sandbox::scenarios::sample_body_positions(gse::scenario::context& ctx) -> std::flat_map<gse::id, gse::vec3<gse::position>> {
	const auto owners = ctx.owners<gse::physics::transform_component>();
	const auto transforms = ctx.components<gse::physics::transform_component>();

	std::vector<std::pair<gse::id, gse::vec3<gse::position>>> staging;
	staging.reserve(transforms.size());
	for (std::size_t index = 0; index < transforms.size(); ++index) {
		staging.emplace_back(owners[index], transforms[index].position);
	}

	std::flat_map<gse::id, gse::vec3<gse::position>> sampled;
	sampled.insert(staging.begin(), staging.end());
	return sampled;
}

auto sandbox::scenarios::displaced_bodies(const std::flat_map<gse::id, gse::vec3<gse::position>>& from, const std::flat_map<gse::id, gse::vec3<gse::position>>& to, const gse::length tolerance) -> std::size_t {
	std::size_t displaced = 0;
	for (const auto& [owner, position] : from) {
		const auto it = to.find(owner);
		if (it == to.end() || gse::magnitude(it->second - position) > tolerance) {
			++displaced;
		}
	}
	return displaced;
}

auto sandbox::scenarios::expect_reset_restores_spawn_poses(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 30;
	constexpr std::uint64_t strike_frames = 8;
	constexpr std::uint64_t collapse_frames = 60;
	constexpr std::uint64_t rebuild_frames = 4;
	const auto tolerance = gse::meters(0.25f);

	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, settle_frames);

	const auto spawned = sample_body_positions(ctx);

	for (std::uint64_t frame = 0; frame < strike_frames; ++frame) {
		ctx.channels().push<strike_pyramid_request>({});
		co_await gse::scenario::wait_frames(ctx, 1);
	}
	co_await gse::scenario::wait_frames(ctx, collapse_frames);

	const auto struck = sample_body_positions(ctx);

	ctx.channels().push<gse::reset_active_scene_request>({});
	co_await gse::scenario::wait_frames(ctx, rebuild_frames);

	const auto rebuilt = sample_body_positions(ctx);

	const auto knocked_over = displaced_bodies(spawned, struck, tolerance);
	const auto still_displaced = displaced_bodies(spawned, rebuilt, tolerance);

	gse::log::println(
		"scenario: scene reset rebuilt {} bodies from {}, strike moved {}, {} still away from their spawn pose",
		rebuilt.size(),
		spawned.size(),
		knocked_over,
		still_displaced
	);

	ctx.expect(!spawned.empty());
	ctx.expect_eq(rebuilt.size(), spawned.size());
	ctx.expect(knocked_over > 0);
	ctx.expect_eq(still_displaced, std::size_t{ 0 });
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::scene_reset(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await expect_reset_restores_spawn_poses(ctx);
}

auto sandbox::scenarios::scene_reset_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await expect_reset_restores_spawn_poses(ctx);
}

auto sandbox::scenarios::relaunch_probe(gse::scenario::context&) -> gse::async::task<> {
	gse::process::open_url("about:blank");
	gse::app::relaunch_self_on_exit();
	gse::app::add_relaunch_handoff({}, L"--engine-bench-scenario scene_reset_cpu");
	co_return;
}

auto sandbox::scenarios::web_host_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	auto last = gse::window::viewport({});
	auto last_minimized = gse::window::minimized({});
	ctx.channels().push<gse::ui_focus_request>({ .focus = true });
	gse::log::println("scenario: web host frame {} viewport {}x{} minimized {}", ctx.frame(), last.x(), last.y(), last_minimized);
	while (true) {
		co_await gse::scenario::wait_frames(ctx, 1);
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
		const auto now = gse::window::viewport({});
		const auto now_minimized = gse::window::minimized({});
		if (now != last || now_minimized != last_minimized) {
			gse::log::println("scenario: web host frame {} viewport {}x{} minimized {}", ctx.frame(), now.x(), now.y(), now_minimized);
			last = now;
			last_minimized = now_minimized;
		}
		else {
			gse::log::println("scenario: web host frame {}", ctx.frame());
		}
	}
}

auto sandbox::scenarios::web_pointer_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	ctx.channels().push<gse::ui_focus_request>({ .focus = true });
	ctx.channels().push<gse::cursor_capture_request>({ .capture = true });
	gse::log::println("scenario: web pointer requested capture at frame {}", ctx.frame());
	while (true) {
		co_await gse::scenario::wait_frames(ctx, 1);
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

auto sandbox::scenarios::web_idle_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	const auto start = gse::system_clock::now<gse::time_t<double, gse::seconds>>();
	const auto since_start = [start] {
		return gse::time_t<double, gse::seconds>(gse::system_clock::now<gse::time_t<double, gse::seconds>>() - start);
	};
	gse::log::println("scenario: web idle start at clock {:.1f:ms}", start);
	std::jthread waker([&since_start](const std::stop_token stop) {
		for (int post = 1; post <= 5 && !stop.stop_requested(); ++post) {
			std::this_thread::sleep_for(std::chrono::seconds(2));
			gse::log::println("scenario: web idle post {} at {:.1f:ms}", post, since_start());
			gse::frame_demand::request_redraw();
		}
	});
	while (true) {
		co_await gse::scenario::wait_frames(ctx, 1);
		gse::log::println("scenario: web idle frame {} at {:.1f:ms} minimized {}", ctx.frame(), since_start(), gse::window::minimized({}));
	}
}

auto sandbox::scenarios::persist_probe_settings_path() -> std::filesystem::path {
	return gse::config::user_config_dir() / "web_persist_probe.ini";
}

auto sandbox::scenarios::persist_probe_save_path() -> std::filesystem::path {
	return gse::config::user_state_dir() / "saves" / "web_persist_probe.sav";
}

auto sandbox::scenarios::persist_probe_settings() -> std::string {
	return std::format("[{}]\n{} = {}\n", persist_probe_category, persist_probe_key, persist_probe_token);
}

auto sandbox::scenarios::persist_probe_save() -> std::string {
	constexpr std::size_t size = 2 * 4096 + 77;
	std::string bytes(size, '\0');
	for (std::size_t index = 0; index < size; ++index) {
		bytes[index] = static_cast<char>((index * 167 + index / 256) & 0xff);
	}
	return bytes;
}

auto sandbox::scenarios::web_persist_write(gse::scenario::context& ctx) -> gse::async::task<> {
	const auto settings = gse::fs::write_text(persist_probe_settings_path(), persist_probe_settings());
	const auto save = gse::fs::write_text(persist_probe_save_path(), persist_probe_save());
	gse::log::println(
		"scenario: web persist wrote settings {} ({}) save {} ({} bytes)",
		persist_probe_settings_path().generic_display_string(),
		settings ? "ok" : settings.error(),
		persist_probe_save_path().generic_display_string(),
		save ? std::to_string(persist_probe_save().size()) : save.error()
	);
	ctx.expect(settings.has_value());
	ctx.expect(save.has_value());
	gse::app::relaunch_self_on_exit();
	gse::app::add_relaunch_handoff({}, L"--engine-bench-scenario web_persist_read");
	co_return;
}

auto sandbox::scenarios::web_persist_read(gse::scenario::context& ctx) -> gse::async::task<> {
	const auto token = gse::save::registry::read_one<std::string>(persist_probe_settings_path(), persist_probe_category, persist_probe_key);
	const bool settings_match = gse::fs::read_text(persist_probe_settings_path()) == persist_probe_settings() && token == persist_probe_token;
	const auto save = gse::fs::read_text(persist_probe_save_path());
	const bool save_match = save == persist_probe_save();
	gse::log::println(
		"scenario: web persist settings {} token '{}' save {} bytes {}",
		settings_match ? "match" : "mismatch",
		token,
		save_match ? "match" : "mismatch",
		save.size()
	);
	ctx.expect(settings_match);
	ctx.expect(save_match);
	co_return;
}

auto sandbox::scenarios::record_static_shot(gse::scenario::context& ctx, const std::uint64_t settle_frames, const std::uint64_t record_frames) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, settle_frames);

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
	co_await gse::scenario::wait_frames(ctx, record_frames);
	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::carousel_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 250);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::machines_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 350);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::dominoes_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 250);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::swings_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 350);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::carousel_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await record_static_shot(ctx, 90, 900);
}

auto sandbox::scenarios::dominoes_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await record_static_shot(ctx, 60, 420);
}

auto sandbox::scenarios::pyramid(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 500);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::rollback_reference(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::rollback_replay(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 70);
	ctx.channels().push<gse::physics::rollback_request>({
		.steps = 60,
	});
}

auto sandbox::scenarios::rollback_reference_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::rollback_replay_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 70);
	ctx.channels().push<gse::physics::rollback_request>({
		.steps = 20,
	});
}

auto sandbox::scenarios::push_pyramid_impulses(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 50);
	ctx.channels().push<gse::physics::impulse_request>({
		.target = gse::find_or_generate_id("PyramidBlock_0_0"),
		.impulse = gse::vec3<gse::impulse>(gse::newton_seconds(60.f), gse::newton_seconds(240.f), gse::newton_seconds(0.f)),
	});
	co_await gse::scenario::wait_frames(ctx, 10);
	ctx.channels().push<gse::physics::impulse_request>({
		.target = gse::find_or_generate_id("PyramidBlock_1_0"),
		.impulse = gse::vec3<gse::impulse>(gse::newton_seconds(0.f), gse::newton_seconds(200.f), gse::newton_seconds(80.f)),
	});
}

auto sandbox::scenarios::rollback_impulse_reference(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await push_pyramid_impulses(ctx);
}

auto sandbox::scenarios::rollback_impulse_replay(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await push_pyramid_impulses(ctx);
	co_await gse::scenario::wait_frames(ctx, 15);
	ctx.channels().push<gse::physics::rollback_request>({
		.steps = 20,
	});
}

auto sandbox::scenarios::rollback_impulse_reference_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await push_pyramid_impulses(ctx);
}

auto sandbox::scenarios::rollback_impulse_replay_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await push_pyramid_impulses(ctx);
	co_await gse::scenario::wait_frames(ctx, 15);
	ctx.channels().push<gse::physics::rollback_request>({
		.steps = 20,
	});
}

auto sandbox::scenarios::press_key(gse::scenario::context& ctx, const gse::key key) -> void {
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = key },
	});
}

auto sandbox::scenarios::release_key(gse::scenario::context& ctx, const gse::key key) -> void {
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_released{ .key_code = key },
	});
}

auto sandbox::scenarios::find_proxy_owner(gse::scenario::context& ctx) -> std::optional<gse::id> {
	for (const auto owner : ctx.owners<gse::physics::motor_component>()) {
		if (owner.tag().ends_with(".Proxy")) {
			return owner;
		}
	}
	return std::nullopt;
}

auto sandbox::scenarios::find_proxy(gse::scenario::context& ctx) -> const gse::physics::transform_component* {
	const auto owner = find_proxy_owner(ctx);
	return owner ? ctx.try_component<gse::physics::transform_component>(*owner) : nullptr;
}

auto sandbox::scenarios::walk_pattern(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait(ctx, gse::seconds(6.f));
	press_key(ctx, gse::key::w);
	co_await gse::scenario::wait(ctx, gse::seconds(4.f));
	release_key(ctx, gse::key::w);
	press_key(ctx, gse::key::a);
	co_await gse::scenario::wait(ctx, gse::seconds(2.f));
	release_key(ctx, gse::key::a);
	press_key(ctx, gse::key::w);
	press_key(ctx, gse::key::d);
	co_await gse::scenario::wait(ctx, gse::seconds(3.f));
	release_key(ctx, gse::key::w);
	release_key(ctx, gse::key::d);
	co_await gse::scenario::wait(ctx, gse::seconds(1.f));
	press_key(ctx, gse::key::s);
	co_await gse::scenario::wait(ctx, gse::seconds(3.f));
	release_key(ctx, gse::key::s);
}

auto sandbox::scenarios::net_walk(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await walk_pattern(ctx);
}

auto sandbox::scenarios::rollback_reference_character(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_character_request>({});
}

auto sandbox::scenarios::rollback_replay_character(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_character_request>({});
	co_await gse::scenario::wait_frames(ctx, 360);
	ctx.channels().push<gse::physics::rollback_request>({
		.steps = 30,
	});
}

auto sandbox::scenarios::rollback_reference_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::rollback_replay_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
	co_await gse::scenario::wait_frames(ctx, 150);
	ctx.channels().push<gse::physics::rollback_request>({
		.steps = 60,
	});
}

auto sandbox::scenarios::pyramid16k_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::physics_stress_via_input(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = gse::key::f5 },
	});
	co_await gse::scenario::wait_frames(ctx, 1);
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_released{ .key_code = gse::key::f5 },
	});
}

auto sandbox::scenarios::record_clip(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t lead_in_frames = 60;
	constexpr std::uint64_t record_frames = 300;

	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_stress_request>({});

	for (std::uint64_t i = 0; i < lead_in_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = orbit_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = orbit_target(static_cast<float>(i) / static_cast<float>(record_frames - 1)),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::solver_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 180;
	constexpr std::uint64_t hold_frames = 120;
	constexpr std::uint64_t strike_frames = 12;
	constexpr std::uint64_t collapse_frames = 408;

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = pyramid_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	const auto total = static_cast<float>(hold_frames + strike_frames + collapse_frames - 1);

	for (std::uint64_t i = 0; i < hold_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = pyramid_target(static_cast<float>(i) / total),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	for (std::uint64_t f = 0; f < strike_frames; ++f) {
		ctx.channels().push<strike_pyramid_request>({});
		ctx.channels().push<gse::camera::request>({
			.target = pyramid_target(static_cast<float>(hold_frames + f) / total),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	for (std::uint64_t i = 0; i < collapse_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = pyramid_target(static_cast<float>(hold_frames + strike_frames + i) / total),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::sky_render_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t driven_frames = 700;
	constexpr float fixed_progress = 0.35f;

	const auto target = sky_target(fixed_progress);
	const auto sun = sun_at(fixed_progress);

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < driven_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = target,
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sun);
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::light_hall_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t driven_frames = 700;
	constexpr float fixed_progress = 0.25f;

	const auto target = hall_target(fixed_progress);

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < driven_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = target,
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::atmosphere_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 60;
	constexpr std::uint64_t record_frames = 600;

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = sky_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sun_at(0.f));
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		const float progress = static_cast<float>(i) / static_cast<float>(record_frames - 1);
		ctx.channels().push<gse::camera::request>({
			.target = sky_target(progress),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sun_at(progress));
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::tumbler_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 90;
	constexpr std::uint64_t record_frames = 1000;

	const gse::renderer::atmosphere::sun_request sun{
		.elevation = gse::degrees(34.f),
		.azimuth = gse::degrees(126.f),
	};

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = tumbler_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sun);
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = tumbler_target(static_cast<float>(i) / static_cast<float>(record_frames - 1)),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sun);
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::cloud_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 60;
	constexpr std::uint64_t record_frames = 1050;

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = cloud_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(cloud_sun_at(0.f));
		ctx.channels().push<gse::renderer::cloud::weather_request>(cloud_weather_at(0.f));
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		const float progress = static_cast<float>(i) / static_cast<float>(record_frames - 1);
		ctx.channels().push<gse::camera::request>({
			.target = cloud_target(progress),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(cloud_sun_at(progress));
		ctx.channels().push<gse::renderer::cloud::weather_request>(cloud_weather_at(progress));
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::sunset_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 60;
	constexpr std::uint64_t record_frames = 1575;

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = sunset_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sunset_sun_at(0.f));
		ctx.channels().push<gse::renderer::cloud::weather_request>(sunset_weather_at(0.f));
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		const float progress = static_cast<float>(i) / static_cast<float>(record_frames - 1);
		ctx.channels().push<gse::camera::request>({
			.target = sunset_target(progress),
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(sunset_sun_at(progress));
		ctx.channels().push<gse::renderer::cloud::weather_request>(sunset_weather_at(progress));
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::light_hall_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 120;
	constexpr std::uint64_t record_frames = 720;

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = hall_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = hall_target(static_cast<float>(i) / static_cast<float>(record_frames - 1)),
			.priority = 100,
			.blend_duration = {},
		});
		if (i % 180 == 60) {
			ctx.channels().push<gse::renderer::capture::screenshot_request>({});
		}
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::sky_horizon_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await pitch_probe(ctx);
}

auto sandbox::scenarios::hall_horizon_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await pitch_probe(ctx);
}

auto sandbox::scenarios::pitch_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 120;
	constexpr std::uint64_t record_frames = 780;

	co_await gse::scenario::wait_settled(ctx);

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = horizon_probe_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = horizon_probe_target(static_cast<float>(i) / static_cast<float>(record_frames - 1)),
			.priority = 100,
			.blend_duration = {},
		});
		if (i % 180 == 0) {
			ctx.channels().push<gse::renderer::capture::screenshot_request>({});
		}
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	constexpr std::uint64_t glare_frames = 180;
	for (std::uint64_t i = 0; i < glare_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = glare_probe_target(static_cast<float>(i) / static_cast<float>(glare_frames)),
			.priority = 100,
			.blend_duration = {},
		});
		if (i % 60 == 45) {
			ctx.channels().push<gse::renderer::capture::screenshot_request>({});
		}
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::hold_cloud_probe(gse::scenario::context& ctx, const gse::camera::target target, const std::uint64_t frames) -> gse::async::task<> {
	for (std::uint64_t i = 0; i < frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = target,
			.priority = 100,
			.blend_duration = {},
		});
		ctx.channels().push<gse::renderer::atmosphere::sun_request>(cloud_probe_sun());
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::capture_cloud_probe(gse::scenario::context& ctx, const std::filesystem::path& file) -> void {
	ctx.channels().push<gse::renderer::capture::screenshot_request>({
		.path = file,
	});
	gse::log::println("scenario: cloud_quality_probe capture {} frame {}", file.generic_display_string(), ctx.frame());
}

auto sandbox::scenarios::cloud_quality_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t hold_frames = 120;
	constexpr std::uint64_t recapture_frames = 30;
	constexpr std::uint64_t flush_frames = 10;
	constexpr std::uint64_t pan_settle_frames = 60;
	constexpr std::uint64_t pan_frames = 240;
	const auto pan_start = gse::degrees(50.f);
	const auto pan_sweep = gse::degrees(80.f);

	using named_angle = std::pair<std::string_view, gse::angle>;

	const std::array<named_angle, 3> pitches = {
		named_angle{ "pitch80", gse::degrees(80.f) },
		named_angle{ "pitch35", gse::degrees(35.f) },
		named_angle{ "pitch05", gse::degrees(5.f) },
	};
	const std::array<named_angle, 4> headings = {
		named_angle{ "toward_sun", gse::degrees(0.f) },
		named_angle{ "plus90", gse::degrees(90.f) },
		named_angle{ "away_from_sun", gse::degrees(180.f) },
		named_angle{ "minus90", gse::degrees(-90.f) },
	};

	const std::filesystem::path run_dir = std::format("cloud_quality_probe_{}", gse::system_clock::timestamp_filename());

	co_await gse::scenario::wait_settled(ctx);

	for (const auto& [pitch_name, pitch] : pitches) {
		for (const auto& [heading_name, heading] : headings) {
			const auto target = cloud_probe_target(pitch, heading);

			ctx.channels().push<gse::camera::temporal_reset_request>({});
			co_await hold_cloud_probe(ctx, target, hold_frames);
			capture_cloud_probe(ctx, run_dir / std::format("{}_{}_a.png", pitch_name, heading_name));
			co_await hold_cloud_probe(ctx, target, recapture_frames);
			capture_cloud_probe(ctx, run_dir / std::format("{}_{}_b.png", pitch_name, heading_name));
			co_await hold_cloud_probe(ctx, target, flush_frames);
		}
	}

	for (std::size_t index = 1; index < pitches.size(); ++index) {
		const auto& [pitch_name, pitch] = pitches[index];

		ctx.channels().push<gse::camera::temporal_reset_request>({});
		co_await hold_cloud_probe(ctx, cloud_probe_target(pitch, pan_start), pan_settle_frames);

		for (std::uint64_t i = 0; i < pan_frames; ++i) {
			const auto yaw = pan_start + pan_sweep * (static_cast<float>(i) / static_cast<float>(pan_frames - 1));
			if (i == pan_frames / 2) {
				capture_cloud_probe(ctx, run_dir / std::format("{}_pan.png", pitch_name));
			}
			co_await hold_cloud_probe(ctx, cloud_probe_target(pitch, yaw), 1);
		}

		co_await hold_cloud_probe(ctx, cloud_probe_target(pitch, pan_start + pan_sweep), flush_frames);
	}
}

auto sandbox::scenarios::lighting_showcase(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 90;
	constexpr std::uint64_t record_frames = 540;

	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_lights_request>({});

	for (std::uint64_t i = 0; i < settle_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = lighting_target(0.f),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});

	for (std::uint64_t i = 0; i < record_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = lighting_target(static_cast<float>(i) / static_cast<float>(record_frames - 1)),
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
}

auto sandbox::scenarios::capture_parity_frame(gse::scenario::context& ctx, const std::string_view name, const gse::camera::target target) -> gse::async::task<> {
	constexpr std::uint64_t converge_frames = 240;

	ctx.channels().push<gse::camera::temporal_reset_request>({});

	for (std::uint64_t i = 0; i < converge_frames; ++i) {
		ctx.channels().push<gse::camera::request>({
			.target = target,
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}

	ctx.channels().push<gse::renderer::capture::screenshot_request>({});
	gse::log::println("scenario: {} capture frame {}", name, ctx.frame());

	while (true) {
		ctx.channels().push<gse::camera::request>({
			.target = target,
			.priority = 100,
			.blend_duration = {},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::lighting_parity(gse::scenario::context& ctx) -> gse::async::task<> {
	ctx.channels().push<gse::window_resize_request>({ .size = parity_extent });
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_lights_request>({});
	co_await capture_parity_frame(ctx, "lighting_parity", lighting_target(0.f));
}

auto sandbox::scenarios::textured_parity(gse::scenario::context& ctx) -> gse::async::task<> {
	ctx.channels().push<gse::window_resize_request>({ .size = parity_extent });
	co_await gse::scenario::wait_settled(ctx);
	co_await capture_parity_frame(ctx, "textured_parity", orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(1.f), gse::meters(0.f)),
		gse::meters(9.f),
		gse::meters(3.f),
		gse::degrees(20.f)
	));
}

auto sandbox::scenarios::parity_stress_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::parity_stress_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::parity_drop(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_pair(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_stack(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_cluster(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_heap(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_mound(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_pile(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_hull_pile(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_nojoints_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_stress_request>({});
}

auto sandbox::scenarios::parity_empty_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_jointsonly(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_joints_request>({});
}

auto sandbox::scenarios::locomotion(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait(ctx, gse::seconds(2.f));
	ctx.channels().push<spawn_character_request>({});

	co_await gse::scenario::wait(ctx, gse::seconds(1.f));
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = gse::key::left_shift },
	});
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = gse::key::w },
	});
}

auto sandbox::scenarios::mouse_latch_look(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_character_request>({});
	co_await gse::scenario::wait(ctx, gse::seconds(1.f));

	for (int frame = 0; frame < 240; ++frame) {
		ctx.channels().push<gse::input::synthetic_input_request>({
			.value = gse::input::mouse_raw_moved{
				.x_delta = frame < 120 ? 6.0 : -6.0,
				.y_delta = frame % 60 < 30 ? 2.0 : -2.0,
			},
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::character_gpu_bench(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_character_request>({});
}

auto sandbox::scenarios::character_gpu_bench_headless(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await character_gpu_bench(ctx);
}

auto sandbox::scenarios::parity_overlap(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_shapes(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_domino(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::build_audio_stress_workload(gse::scenario::context& ctx, const std::chrono::milliseconds frame_pause) -> gse::async::task<> {
	constexpr std::uint64_t clip_load_frames = 120;
	constexpr std::uint64_t sustain_frames = 180;

	constexpr std::chrono::milliseconds clip_load_pause(10);

	const auto hold = [&](const std::uint64_t frames, const std::chrono::milliseconds pause) -> gse::async::task<> {
		for (std::uint64_t frame = 0; frame < frames; ++frame) {
			co_await gse::scenario::wait_frames(ctx, 1);
			std::this_thread::sleep_for(pause);
		}
	};

	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<prefetch_audio_stress_request>({});
	co_await hold(clip_load_frames, std::max(frame_pause, clip_load_pause));
	ctx.channels().push<spawn_audio_stress_request>({});
	co_await hold(sustain_frames, frame_pause);
	ctx.channels().push<cycle_audio_stress_request>({});
}

auto sandbox::scenarios::audio_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_audio_stress_workload(ctx, std::chrono::milliseconds(0));
}

auto sandbox::scenarios::audio_stress_device(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::chrono::milliseconds frame_pause(16);

	co_await build_audio_stress_workload(ctx, frame_pause);
	while (true) {
		co_await gse::scenario::wait_frames(ctx, 1);
		std::this_thread::sleep_for(frame_pause);
	}
}

auto sandbox::scenarios::audio_probe_segments() -> std::array<audio_probe_segment, 9> {
	constexpr std::uint64_t travel_frames = 120;
	const auto speed = gse::meters_per_second(20.f);
	const auto still = gse::meters_per_second(0.f);
	const auto level = gse::meters(0.f);
	const auto ahead = [level](const gse::length distance) {
		return gse::vec3<gse::position>(level, level, -distance);
	};

	return {
		audio_probe_segment{ .kind = "stationary", .from = ahead(gse::meters(2.f)) },
		audio_probe_segment{ .kind = "stationary", .from = ahead(gse::meters(4.f)) },
		audio_probe_segment{ .kind = "stationary", .from = ahead(gse::meters(8.f)) },
		audio_probe_segment{ .kind = "stationary", .from = ahead(gse::meters(16.f)) },
		audio_probe_segment{ .kind = "stationary", .from = ahead(gse::meters(32.f)) },
		audio_probe_segment{ .kind = "side", .from = gse::vec3<gse::position>(gse::meters(4.f), level, level) },
		audio_probe_segment{ .kind = "side", .from = gse::vec3<gse::position>(gse::meters(-4.f), level, level) },
		audio_probe_segment{
			.kind = "moving",
			.from = ahead(gse::meters(60.f)),
			.velocity = gse::vec3<gse::velocity>(still, still, speed),
			.frames = travel_frames,
		},
		audio_probe_segment{
			.kind = "moving",
			.from = ahead(gse::meters(20.f)),
			.velocity = gse::vec3<gse::velocity>(still, still, -speed),
			.frames = travel_frames,
		},
	};
}

auto sandbox::scenarios::audio_spatial_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_spatial_script(ctx);
}

auto sandbox::scenarios::audio_binaural_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_spatial_script(ctx);
}

auto sandbox::scenarios::run_audio_spatial_script(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t settle_frames = 10;
	constexpr std::uint64_t gap_frames = 15;
	const auto pitch = gse::hertz(1000.f);
	const auto gain = gse::percentage<float>::fraction(0.5f);
	const auto emitter = gse::find_or_generate_id(audio_probe_emitter);

	const auto stop = [&](const gse::channel_future<gse::voice_handle>& voice) {
		const auto* handle = voice.try_get();
		if (ctx.expect(handle != nullptr)) {
			ctx.channels().push<gse::audio::stop_request>({
				.handle = *handle,
			});
		}
	};

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto* listener = ctx.try_component<gse::physics::transform_component>(listeners[0]);
	if (!ctx.expect(listener != nullptr)) {
		co_return;
	}
	gse::log::println(
		gse::log::category::general,
		"audio_spatial_probe: speed of sound {:.2f}, listener at {:.3f}, fixed step {:.6f:ms}",
		gse::audio::speed_of_sound(),
		listener->position,
		gse::system_clock::fixed_dt()
	);

	gse::log::println(gse::log::category::general, "audio_spatial_probe: sync frame {} pitch {:.1f:Hz}", ctx.frame(), pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = pitch,
		.gain = gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	stop(sync);
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	for (const auto& [kind, from, velocity, frames] : audio_probe_segments()) {
		const auto move_to = [&](const std::uint64_t step) {
			ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
				.target = emitter,
				.value = from + velocity * (gse::system_clock::fixed_dt() * static_cast<float>(step)),
			});
		};

		move_to(0);
		co_await gse::scenario::wait_frames(ctx, settle_frames);

		gse::log::println(
			gse::log::category::general,
			"audio_spatial_probe: segment {} frame {} position {:.3f} velocity {:.3f} pitch {:.1f:Hz} frames {}",
			kind,
			ctx.frame(),
			from,
			velocity,
			pitch,
			frames
		);
		const auto voice = ctx.channels().push<gse::audio::play_tone_request>({
			.pitch = pitch,
			.gain = gain,
			.emitter = emitter,
		});
		for (std::uint64_t step = 1; step <= frames; ++step) {
			move_to(step);
			co_await gse::scenario::wait_frames(ctx, 1);
		}
		stop(voice);
		co_await gse::scenario::wait_frames(ctx, gap_frames);
	}
}

auto sandbox::scenarios::audio_occlusion_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_occlusion_script(ctx);
}

auto sandbox::scenarios::audio_occlusion_probe_headphones(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_occlusion_script(ctx);
}

auto sandbox::scenarios::run_audio_occlusion_script(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t settle_frames = 10;
	constexpr std::uint64_t gap_frames = 15;
	const auto sync_pitch = gse::hertz(1000.f);
	const auto low = gse::hertz(200.f);
	const auto high = gse::hertz(6000.f);
	const auto sync_gain = gse::percentage<float>::fraction(0.5f);
	const auto gain = gse::percentage<float>::fraction(0.25f);
	const auto still = gse::meters_per_second(0.f);
	const auto walk = gse::vec3<gse::velocity>(gse::meters_per_second(1.f), still, still);
	const auto level = gse::meters(0.f);
	const auto clear_side = gse::vec3<gse::displacement>(gse::meters(2.f), level, level);
	const auto hidden_side = gse::vec3<gse::displacement>(gse::meters(-2.f), level, level);
	const auto emitter = gse::find_or_generate_id(audio_probe_emitter);

	const auto behind_emitter = gse::vec3<gse::displacement>(level, level, gse::meters(8.f));

	using at = audio_occlusion_station;
	const std::array segments{
		audio_occlusion_segment{ .kind = "reference", .from = clear_side, .lead_frames = 30, .tail_frames = 30 },
		audio_occlusion_segment{ .kind = "occluded", .from = hidden_side, .lead_frames = 30, .tail_frames = 30 },
		audio_occlusion_segment{ .kind = "walk", .from = hidden_side, .lead_frames = 30, .travel_frames = 240, .tail_frames = 30 },
		audio_occlusion_segment{ .kind = "floor", .station = at::floor, .from = clear_side, .lead_frames = 30, .tail_frames = 30 },
		audio_occlusion_segment{ .kind = "sphere", .station = at::sphere, .from = clear_side, .lead_frames = 30, .tail_frames = 30 },
		audio_occlusion_segment{ .kind = "capsule", .station = at::capsule, .from = clear_side, .lead_frames = 30, .tail_frames = 30 },
		audio_occlusion_segment{ .kind = "hull", .station = at::hull, .from = clear_side, .lead_frames = 30, .tail_frames = 30 },
	};

	const auto stop = [&](const gse::channel_future<gse::voice_handle>& voice) {
		const auto* handle = voice.try_get();
		if (ctx.expect(handle != nullptr)) {
			ctx.channels().push<gse::audio::stop_request>({
				.handle = *handle,
			});
		}
	};

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto listener = listeners[0];
	const auto* source = ctx.try_component<gse::physics::transform_component>(emitter);
	if (!ctx.expect(source != nullptr)) {
		co_return;
	}
	const auto* prism = ctx.try_component<gse::physics::collision_component>(gse::find_or_generate_id(audio_occlusion_hull));
	if (!ctx.expect(prism != nullptr && std::holds_alternative<gse::physics::hull_shape>(prism->shape.value))) {
		co_return;
	}
	gse::log::println(
		gse::log::category::general,
		"audio_occlusion_probe: emitter at {:.3f}, low {:.1f:Hz}, high {:.1f:Hz}, fixed step {:.6f:ms}",
		source->position,
		low,
		high,
		gse::system_clock::fixed_dt()
	);

	gse::log::println(gse::log::category::general, "audio_occlusion_probe: sync frame {} pitch {:.1f:Hz}", ctx.frame(), sync_pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = sync_gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	stop(sync);
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	for (const auto& [kind, station, from, lead_frames, travel_frames, tail_frames] : segments) {
		const auto emitter_position = audio_occlusion_emitter_position(station);
		const auto start = emitter_position + behind_emitter + from;
		const auto stand_at = [&](const std::uint64_t step) {
			ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
				.target = listener,
				.value = start + walk * (gse::system_clock::fixed_dt() * static_cast<float>(step)),
			});
		};

		ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
			.target = emitter,
			.value = emitter_position,
		});
		stand_at(0);
		co_await gse::scenario::wait_frames(ctx, settle_frames);

		gse::log::println(
			gse::log::category::general,
			"audio_occlusion_probe: segment {} frame {} listener {:.3f} emitter {:.3f} velocity {:.3f} lead {} travel {} tail {}",
			kind,
			ctx.frame(),
			start,
			emitter_position,
			walk,
			lead_frames,
			travel_frames,
			tail_frames
		);
		const auto low_voice = ctx.channels().push<gse::audio::play_tone_request>({
			.pitch = low,
			.gain = gain,
			.emitter = emitter,
		});
		const auto high_voice = ctx.channels().push<gse::audio::play_tone_request>({
			.pitch = high,
			.gain = gain,
			.emitter = emitter,
		});
		co_await gse::scenario::wait_frames(ctx, lead_frames);
		for (std::uint64_t step = 1; step <= travel_frames; ++step) {
			stand_at(step);
			co_await gse::scenario::wait_frames(ctx, 1);
		}
		co_await gse::scenario::wait_frames(ctx, tail_frames);
		stop(low_voice);
		stop(high_voice);
		co_await gse::scenario::wait_frames(ctx, gap_frames);
	}
}

auto sandbox::scenarios::audio_occlusion_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t spawn_frames = 30;
	constexpr std::size_t voices = 64;
	constexpr float tones_per_octave = 16.f;
	const auto lowest = gse::hertz(110.f);
	const auto gain = gse::percentage<float>::ratio(1.f, static_cast<float>(voices));

	co_await build_stress_workload(ctx);
	co_await gse::scenario::wait_frames(ctx, spawn_frames);

	const auto owners = ctx.owners<gse::physics::motion_component>();
	const auto motions = ctx.components<gse::physics::motion_component>();
	std::size_t started = 0;
	for (std::size_t index = 0; index < owners.size() && started < voices; ++index) {
		if (!gse::physics::is_dynamic(motions[index])) {
			continue;
		}
		ctx.channels().push<gse::audio::play_tone_request>({
			.pitch = lowest * std::pow(2.f, static_cast<float>(started) / tones_per_octave),
			.gain = gain,
			.emitter = owners[index],
		});
		++started;
	}
	ctx.expect(started == voices);
	gse::log::println(
		gse::log::category::general,
		"audio_occlusion_stress: frame {}, {} positioned voices on dynamic bodies, {} colliders, {} listeners",
		ctx.frame(),
		started,
		ctx.owners<gse::physics::collision_component>().size(),
		ctx.owners<gse::audio::listener_component>().size()
	);
}

auto sandbox::scenarios::wait_for_ambient_clips(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t clip_load_frames = 120;
	constexpr std::chrono::milliseconds clip_load_pause(10);

	for (std::uint64_t frame = 0; frame < clip_load_frames; ++frame) {
		co_await gse::scenario::wait_frames(ctx, 1);
		std::this_thread::sleep_for(clip_load_pause);
	}
}

auto sandbox::scenarios::run_audio_yard_solo(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);

	const auto emitters = ctx.owners<ambient_sound::component>();
	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(emitters.size() == 1 && listeners.size() == 1)) {
		co_return;
	}
	const auto* sound = ctx.try_component<ambient_sound::component>(emitters[0]);
	const auto* source = ctx.try_component<gse::physics::transform_component>(emitters[0]);
	const auto* listener = ctx.try_component<gse::physics::transform_component>(listeners[0]);
	if (!ctx.expect(sound != nullptr && source != nullptr && listener != nullptr)) {
		co_return;
	}
	gse::log::println(
		gse::log::category::general,
		"audio_yard_solo: frame {} emitter {:.3f} listener {:.3f} clip {} gain {:.3f}",
		ctx.frame(),
		source->position,
		listener->position,
		sound->clip,
		sound->gain.value()
	);

	co_await wait_for_ambient_clips(ctx);
}

auto sandbox::scenarios::audio_yard_solo_open(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_yard_solo(ctx);
}

auto sandbox::scenarios::audio_yard_solo_wall(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_yard_solo(ctx);
}

auto sandbox::scenarios::audio_yard_solo_pillar(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_yard_solo(ctx);
}

auto sandbox::scenarios::audio_contact_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_contact_script(ctx);
}

auto sandbox::scenarios::audio_contact_probe_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_contact_script(ctx);
}

auto sandbox::scenarios::run_audio_contact_script(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t gap_frames = 15;
	constexpr std::uint64_t fall_frames = 120;
	constexpr std::uint64_t settle_frames = 60;
	constexpr std::uint64_t rest_frames = 300;
	const auto sync_pitch = gse::hertz(1000.f);
	const auto sync_gain = gse::percentage<float>::fraction(0.5f);
	const auto falling = gse::meters_per_second(1.f);

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto* listener = ctx.try_component<gse::physics::transform_component>(listeners[0]);
	if (!ctx.expect(listener != nullptr)) {
		co_return;
	}
	gse::log::println(
		gse::log::category::general,
		"audio_contact_probe: speed of sound {:.2f}, listener at {:.3f}, fixed step {:.6f:ms}, {} contact sounds",
		gse::audio::speed_of_sound(),
		listener->position,
		gse::system_clock::fixed_dt(),
		ctx.owners<gse::audio::contact_sound_component>().size()
	);

	co_await wait_for_ambient_clips(ctx);

	gse::log::println(gse::log::category::general, "audio_contact_probe: sync frame {} pitch {:.1f:Hz}", ctx.frame(), sync_pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = sync_gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	if (const auto* handle = sync.try_get(); ctx.expect(handle != nullptr)) {
		ctx.channels().push<gse::audio::stop_request>({
			.handle = *handle,
		});
	}
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	for (int index = 0; index < audio_contact_fallers; ++index) {
		const auto faller = gse::find_or_generate_id(audio_contact_faller_name(index));
		gse::log::println(
			gse::log::category::general,
			"audio_contact_probe: faller {} body {} spawned frame {} height {:.3f}",
			index,
			faller,
			ctx.frame(),
			audio_contact_faller_height(index)
		);
		ctx.channels().push<spawn_audio_contact_faller_request>({
			.index = index,
		});

		gse::velocity fastest;
		bool arrested = false;
		for (std::uint64_t frame = 0; frame < fall_frames; ++frame) {
			co_await gse::scenario::wait_frames(ctx, 1);
			const auto* motion = ctx.try_component<gse::physics::motion_component>(faller);
			if (arrested || motion == nullptr) {
				continue;
			}
			const gse::velocity fall = -motion->current_velocity.y();
			fastest = std::max(fastest, fall);
			if (fastest > falling && fall < fastest * 0.5f) {
				arrested = true;
				gse::log::println(
					gse::log::category::general,
					"audio_contact_probe: faller {} arrested frame {} after falling at {:.4f}",
					index,
					ctx.frame(),
					fastest
				);
			}
		}
		ctx.expect(arrested);
	}

	co_await gse::scenario::wait_frames(ctx, settle_frames);
	gse::log::println(gse::log::category::general, "audio_contact_probe: rest window frame {} frames {}", ctx.frame(), rest_frames);
	co_await gse::scenario::wait_frames(ctx, rest_frames);
	gse::log::println(gse::log::category::general, "audio_contact_probe: rest window closed frame {}", ctx.frame());
}

auto sandbox::scenarios::audio_contact_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t spawn_frames = 30;

	co_await build_stress_workload(ctx);
	co_await gse::scenario::wait_frames(ctx, spawn_frames);
	ctx.channels().push<add_contact_sounds_request>({});
	co_await gse::scenario::wait_frames(ctx, spawn_frames);
	gse::log::println(
		gse::log::category::general,
		"audio_contact_stress: frame {}, {} contact sounds, {} colliders, {} listeners",
		ctx.frame(),
		ctx.owners<gse::audio::contact_sound_component>().size(),
		ctx.owners<gse::physics::collision_component>().size(),
		ctx.owners<gse::audio::listener_component>().size()
	);
}

auto sandbox::scenarios::audio_modal_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_modal_script(ctx, audio_modal_material::steel);
}

auto sandbox::scenarios::audio_modal_probe_wood(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_modal_script(ctx, audio_modal_material::wood);
}

auto sandbox::scenarios::audio_modal_probe_glass(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_modal_script(ctx, audio_modal_material::glass);
}

auto sandbox::scenarios::audio_modal_stress_device(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::chrono::milliseconds frame_pause(16);

	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<prefetch_audio_modal_request>({});
	co_await wait_for_ambient_clips(ctx);
	ctx.channels().push<spawn_audio_modal_field_request>({});
	gse::log::println(
		gse::log::category::general,
		"audio_modal_stress_device: frame {}, a field of {} hopping bars requested, {} listeners",
		ctx.frame(),
		audio_modal_field_bars,
		ctx.owners<gse::audio::listener_component>().size()
	);
	while (true) {
		co_await gse::scenario::wait_frames(ctx, 1);
		std::this_thread::sleep_for(frame_pause);
	}
}

auto sandbox::scenarios::run_audio_modal_script(gse::scenario::context& ctx, const audio_modal_material material) -> gse::async::task<> {
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t gap_frames = 15;
	constexpr std::uint64_t ring_frames = 300;
	const auto sync_pitch = gse::hertz(1000.f);
	const auto sync_gain = gse::percentage<float>::fraction(0.5f);

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto* listener = ctx.try_component<gse::physics::transform_component>(listeners[0]);
	if (!ctx.expect(listener != nullptr)) {
		co_return;
	}
	gse::log::println(
		gse::log::category::general,
		"audio_modal_probe: speed of sound {:.2f}, listener at {:.3f}, fixed step {:.6f:ms}",
		gse::audio::speed_of_sound(),
		listener->position,
		gse::system_clock::fixed_dt()
	);

	ctx.channels().push<prefetch_audio_modal_request>({});
	co_await wait_for_ambient_clips(ctx);

	gse::log::println(gse::log::category::general, "audio_modal_probe: sync frame {} pitch {:.1f:Hz}", ctx.frame(), sync_pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = sync_gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	if (const auto* handle = sync.try_get(); ctx.expect(handle != nullptr)) {
		ctx.channels().push<gse::audio::stop_request>({
			.handle = *handle,
		});
	}
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	for (int index = 0; index < audio_modal_bars; ++index) {
		gse::log::println(
			gse::log::category::general,
			"audio_modal_probe: bar {} body {} spawned frame {} height {:.3f} tilt {:.1f:deg} at {:.4f}",
			index,
			gse::find_or_generate_id(audio_modal_bar_name(index)),
			ctx.frame(),
			audio_modal_bar_height(index),
			audio_modal_bar_tilt(index),
			audio_modal_bar_position(index)
		);
		ctx.channels().push<spawn_audio_modal_bar_request>({
			.index = index,
			.material = material,
		});
		co_await gse::scenario::wait_frames(ctx, ring_frames);
	}
	gse::log::println(gse::log::category::general, "audio_modal_probe: finished frame {}", ctx.frame());
}

auto sandbox::scenarios::audio_slide_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::string_view label = "audio_slide_probe";
	const auto down_the_slope = audio_slide_direction();

	co_await open_audio_rub_probe(ctx, label);
	ctx.channels().push<spawn_audio_slide_plate_request>({});
	co_await run_audio_rub_launches(
		ctx,
		{
			.label = label,
			.body = audio_slide_plate,
			.reach = gse::meters(0.25f),
			.launches = {
				down_the_slope * gse::meters_per_second(1.f),
				down_the_slope * gse::meters_per_second(2.f),
				down_the_slope * gse::meters_per_second(3.f),
			},
			.lead_frames = 2,
		}
	);
}

auto sandbox::scenarios::audio_roll_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::string_view label = "audio_roll_probe";
	const gse::vec3f along(1.f, 0.f, 0.f);

	co_await open_audio_rub_probe(ctx, label);
	ctx.channels().push<spawn_audio_roll_ball_request>({});
	co_await run_audio_rub_launches(
		ctx,
		{
			.label = label,
			.body = audio_roll_ball,
			.reach = audio_roll_ball_radius(),
			.launches = {
				along * gse::meters_per_second(1.f),
				along * gse::meters_per_second(-1.4f),
				along * gse::meters_per_second(1.75f),
			},
			.lead_frames = 8,
		}
	);
}

auto sandbox::scenarios::open_audio_rub_probe(gse::scenario::context& ctx, const std::string_view label) -> gse::async::task<> {
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t gap_frames = 15;
	const auto sync_pitch = gse::hertz(1000.f);
	const auto sync_gain = gse::percentage<float>::fraction(0.5f);

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto* listener = ctx.try_component<gse::physics::transform_component>(listeners[0]);
	if (!ctx.expect(listener != nullptr)) {
		co_return;
	}
	gse::log::println(
		gse::log::category::general,
		"{}: speed of sound {:.2f}, listener at {:.3f}, fixed step {:.6f:ms}",
		label,
		gse::audio::speed_of_sound(),
		listener->position,
		gse::system_clock::fixed_dt()
	);

	ctx.channels().push<prefetch_audio_modal_request>({});
	co_await wait_for_ambient_clips(ctx);

	gse::log::println(gse::log::category::general, "{}: sync frame {} pitch {:.1f:Hz}", label, ctx.frame(), sync_pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = sync_gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	if (const auto* handle = sync.try_get(); ctx.expect(handle != nullptr)) {
		ctx.channels().push<gse::audio::stop_request>({
			.handle = *handle,
		});
	}
	co_await gse::scenario::wait_frames(ctx, gap_frames);
}

auto sandbox::scenarios::run_audio_rub_launches(gse::scenario::context& ctx, const audio_rub_probe probe) -> gse::async::task<> {
	constexpr std::uint64_t settle_frames = 90;
	constexpr std::uint64_t quiet_frames = 30;
	constexpr std::uint64_t longest_run_frames = 480;
	const auto at_rest = gse::meters_per_second(0.01f);

	const auto body = gse::find_or_generate_id(probe.body);
	co_await gse::scenario::wait_frames(ctx, settle_frames);
	gse::log::println(gse::log::category::general, "{}: body {} quiet frame {} frames {}", probe.label, body, ctx.frame(), quiet_frames);
	co_await gse::scenario::wait_frames(ctx, quiet_frames);

	for (std::size_t index = 0; index < probe.launches.size(); ++index) {
		const auto* resting = ctx.try_component<gse::physics::motion_component>(body);
		if (!ctx.expect(resting != nullptr)) {
			co_return;
		}
		const auto& launch = probe.launches[index];
		ctx.channels().push<gse::physics::impulse_request>({
			.target = body,
			.impulse = launch * gse::physics::mass_of(*resting),
		});
		gse::log::println(gse::log::category::general, "{}: launch {} frame {} velocity {:.3f}", probe.label, index, ctx.frame(), launch);

		gse::velocity fastest;
		gse::velocity summed;
		std::uint64_t counted = 0;
		std::uint64_t window_begin = 0;
		bool arrested = false;
		bool rested = false;
		for (std::uint64_t frame = 0; frame < longest_run_frames && !rested; ++frame) {
			co_await gse::scenario::wait_frames(ctx, 1);
			const auto* motion = ctx.try_component<gse::physics::motion_component>(body);
			if (!ctx.expect(motion != nullptr)) {
				co_return;
			}
			const gse::velocity travel = gse::magnitude(motion->current_velocity);
			const gse::velocity surface = gse::magnitude(motion->angular_velocity) * probe.reach / gse::rad;
			if (!arrested && frame >= probe.lead_frames) {
				fastest = std::max(fastest, travel);
				if (travel < fastest * 0.5f) {
					arrested = true;
					if (ctx.expect(counted > 0)) {
						gse::log::println(
							gse::log::category::general,
							"{}: launch {} window frames {} {} mean speed {:.4f}",
							probe.label,
							index,
							window_begin,
							ctx.frame(),
							summed / static_cast<float>(counted)
						);
					}
				}
				else {
					window_begin = counted == 0 ? ctx.frame() : window_begin;
					summed += travel;
					++counted;
				}
			}
			rested = arrested && travel < at_rest && surface < at_rest;
		}
		if (ctx.expect(rested)) {
			gse::log::println(gse::log::category::general, "{}: launch {} rest frame {}", probe.label, index, ctx.frame());
		}
		co_await gse::scenario::wait_frames(ctx, quiet_frames);
	}
	gse::log::println(gse::log::category::general, "{}: finished frame {}", probe.label, ctx.frame());
}

auto sandbox::scenarios::audio_reverb_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t gap_frames = 15;
	constexpr std::uint64_t settle_frames = 10;
	const auto sync_pitch = gse::hertz(1000.f);
	const auto sync_gain = gse::percentage<float>::fraction(0.5f);
	const auto emitter = gse::find_or_generate_id(audio_reverb_emitter);
	const std::array visits{
		audio_reverb_visit{ .station = audio_reverb_station::wood, .ring_frames = 150 },
		audio_reverb_visit{ .station = audio_reverb_station::cork, .ring_frames = 90 },
		audio_reverb_visit{ .station = audio_reverb_station::open, .ring_frames = 60 },
	};

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto listener = listeners[0];
	gse::log::println(
		gse::log::category::general,
		"audio_reverb_probe: speed of sound {:.2f}, fixed step {:.6f:ms}",
		gse::audio::speed_of_sound(),
		gse::system_clock::fixed_dt()
	);

	ctx.channels().push<prefetch_audio_reverb_request>({});
	co_await wait_for_ambient_clips(ctx);

	gse::log::println(gse::log::category::general, "audio_reverb_probe: sync frame {} pitch {:.1f:Hz}", ctx.frame(), sync_pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = sync_gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	if (const auto* handle = sync.try_get(); ctx.expect(handle != nullptr)) {
		ctx.channels().push<gse::audio::stop_request>({
			.handle = *handle,
		});
	}
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	for (const auto& [station, ring_frames] : visits) {
		const auto heard_from = audio_reverb_listener_position(station);
		const auto sounded_at = audio_reverb_emitter_position(station);
		ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
			.target = listener,
			.value = heard_from,
		});
		ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
			.target = emitter,
			.value = sounded_at,
		});
		co_await gse::scenario::wait_frames(ctx, settle_frames);

		gse::log::println(
			gse::log::category::general,
			"audio_reverb_probe: station {} frame {} interior {:.3f} lining {} listener {:.3f} emitter {:.3f} frames {}",
			station,
			ctx.frame(),
			audio_reverb_room_size(),
			station == audio_reverb_station::open ? std::string("none") : audio_reverb_lining(station),
			heard_from,
			sounded_at,
			ring_frames
		);
		ctx.channels().push<play_audio_reverb_knock_request>({
			.emitter = emitter,
		});
		co_await gse::scenario::wait_frames(ctx, ring_frames);
		gse::log::println(gse::log::category::general, "audio_reverb_probe: station {} closed frame {}", station, ctx.frame());
		co_await gse::scenario::wait_frames(ctx, gap_frames);
	}
}

auto sandbox::scenarios::audio_reverb_doorway_probe(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_doorway_script(ctx);
}

auto sandbox::scenarios::audio_reverb_doorway_probe_dry(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await run_audio_doorway_script(ctx);
}

auto sandbox::scenarios::run_audio_doorway_script(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::string_view label = "audio_reverb_doorway_probe";
	constexpr std::uint64_t sync_frames = 15;
	constexpr std::uint64_t gap_frames = 15;
	constexpr std::uint64_t settle_frames = 10;
	constexpr std::uint64_t ring_frames = 120;
	constexpr std::uint64_t jump_after_frames = 6;
	constexpr std::uint64_t away_frames = 90;
	constexpr std::uint64_t lead_frames = 30;
	constexpr std::uint64_t travel_frames = 236;
	constexpr std::array harmonics{ 1.f, 2.f, 3.f, 5.f, 7.f, 10.f, 14.f, 20.f, 28.f, 40.f, 56.f, 80.f };
	const auto fundamental = gse::hertz(100.f);
	const auto harmonic_gain = gse::percentage<float>::fraction(0.05f);
	const auto sync_pitch = gse::hertz(1000.f);
	const auto sync_gain = gse::percentage<float>::fraction(0.5f);
	const auto still = gse::meters_per_second(0.f);
	const auto outward = gse::vec3<gse::velocity>(still, still, gse::meters_per_second(1.4f));
	const auto level = gse::meters(0.f);
	const auto room = audio_doorway_room();
	const auto emitter = gse::find_or_generate_id(audio_doorway_emitter);
	const auto inside = audio_doorway_listener_position();
	const auto leaving_from = audio_doorway_threshold() + gse::vec3<gse::displacement>(level, level, gse::meters(-0.3f));
	const auto walking_from = audio_doorway_threshold() + gse::vec3<gse::displacement>(level, level, gse::meters(-2.5f));
	const auto far_away = audio_doorway_threshold() + gse::vec3<gse::displacement>(gse::meters(200.f), level, level);

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto listener = listeners[0];
	const auto stand_at = [&](const gse::vec3<gse::position>& at) {
		ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
			.target = listener,
			.value = at,
		});
	};
	const auto walked = [&](const gse::vec3<gse::position>& from, const std::uint64_t step) {
		return from + outward * (gse::system_clock::fixed_dt() * static_cast<float>(step));
	};
	gse::log::println(
		gse::log::category::general,
		"{}: speed of sound {:.2f}, fixed step {:.6f:ms}, interior {:.3f} lining {} doorway {:.3f} by {:.3f}",
		label,
		gse::audio::speed_of_sound(),
		gse::system_clock::fixed_dt(),
		room.interior,
		room.lining,
		room.doorway,
		room_door_height()
	);

	ctx.channels().push<prefetch_audio_reverb_request>({});
	co_await wait_for_ambient_clips(ctx);

	gse::log::println(gse::log::category::general, "{}: sync frame {} pitch {:.1f:Hz}", label, ctx.frame(), sync_pitch);
	const auto sync = ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = sync_gain,
	});
	co_await gse::scenario::wait_frames(ctx, sync_frames);
	if (const auto* handle = sync.try_get(); ctx.expect(handle != nullptr)) {
		ctx.channels().push<gse::audio::stop_request>({
			.handle = *handle,
		});
	}
	ctx.channels().push<gse::audio::play_tone_request>({
		.pitch = sync_pitch,
		.gain = gse::percentage<float>::fraction(0.f),
		.emitter = emitter,
	});
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	stand_at(inside);
	co_await gse::scenario::wait_frames(ctx, settle_frames);
	gse::log::println(
		gse::log::category::general,
		"{}: inside frame {} listener {:.3f} emitter {:.3f} frames {}",
		label,
		ctx.frame(),
		inside,
		audio_doorway_emitter_position(),
		ring_frames
	);
	ctx.channels().push<play_audio_reverb_knock_request>({
		.emitter = emitter,
	});
	co_await gse::scenario::wait_frames(ctx, ring_frames + gap_frames);

	stand_at(leaving_from);
	co_await gse::scenario::wait_frames(ctx, settle_frames);
	gse::log::println(
		gse::log::category::general,
		"{}: stay frame {} listener {:.3f} frames {}",
		label,
		ctx.frame(),
		leaving_from,
		ring_frames
	);
	ctx.channels().push<play_audio_reverb_knock_request>({
		.emitter = emitter,
	});
	co_await gse::scenario::wait_frames(ctx, ring_frames + gap_frames);

	gse::log::println(
		gse::log::category::general,
		"{}: leave frame {} listener {:.3f} velocity {:.3f} frames {}",
		label,
		ctx.frame(),
		leaving_from,
		outward,
		ring_frames
	);
	ctx.channels().push<play_audio_reverb_knock_request>({
		.emitter = emitter,
	});
	for (std::uint64_t step = 1; step <= ring_frames; ++step) {
		stand_at(walked(leaving_from, step));
		co_await gse::scenario::wait_frames(ctx, 1);
	}
	co_await gse::scenario::wait_frames(ctx, gap_frames);

	stand_at(inside);
	co_await gse::scenario::wait_frames(ctx, settle_frames);
	gse::log::println(
		gse::log::category::general,
		"{}: jump frame {} listener {:.3f} frames {}",
		label,
		ctx.frame(),
		inside,
		jump_after_frames + away_frames
	);
	ctx.channels().push<play_audio_reverb_knock_request>({
		.emitter = emitter,
	});
	co_await gse::scenario::wait_frames(ctx, jump_after_frames);
	stand_at(far_away);
	gse::log::println(gse::log::category::general, "{}: jumped frame {} listener {:.3f}", label, ctx.frame(), far_away);
	co_await gse::scenario::wait_frames(ctx, away_frames);
	stand_at(walking_from);
	gse::log::println(gse::log::category::general, "{}: returned frame {} listener {:.3f}", label, ctx.frame(), walking_from);
	co_await gse::scenario::wait_frames(ctx, gap_frames + settle_frames);

	gse::log::println(
		gse::log::category::general,
		"{}: walk frame {} listener {:.3f} velocity {:.3f} lead {} travel {}, {} harmonics of {:.1f:Hz}",
		label,
		ctx.frame(),
		walking_from,
		outward,
		lead_frames,
		travel_frames,
		harmonics.size(),
		fundamental
	);
	for (const float harmonic : harmonics) {
		ctx.channels().push<gse::audio::play_tone_request>({
			.pitch = fundamental * harmonic,
			.gain = harmonic_gain,
			.emitter = emitter,
		});
	}
	co_await gse::scenario::wait_frames(ctx, lead_frames);
	for (std::uint64_t step = 1; step <= travel_frames; ++step) {
		stand_at(walked(walking_from, step));
		co_await gse::scenario::wait_frames(ctx, 1);
	}
	gse::log::println(gse::log::category::general, "{}: turned frame {}", label, ctx.frame());
	for (std::uint64_t step = travel_frames; step > 0; --step) {
		stand_at(walked(walking_from, step - 1));
		co_await gse::scenario::wait_frames(ctx, 1);
	}
	co_await gse::scenario::wait_frames(ctx, lead_frames);
	gse::log::println(gse::log::category::general, "{}: finished frame {}", label, ctx.frame());
}

auto sandbox::scenarios::audio_reverb_stress_device(gse::scenario::context& ctx) -> gse::async::task<> {
	constexpr std::chrono::milliseconds frame_pause(16);

	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<prefetch_audio_modal_request>({});
	ctx.channels().push<prefetch_audio_reverb_request>({});
	co_await wait_for_ambient_clips(ctx);
	ctx.channels().push<spawn_audio_modal_field_request>({});
	gse::log::println(
		gse::log::category::general,
		"audio_reverb_stress_device: frame {}, a field of {} hopping bars requested in a hall of {:.3f}, {} listeners",
		ctx.frame(),
		audio_modal_field_bars,
		audio_reverb_hall_size(),
		ctx.owners<gse::audio::listener_component>().size()
	);
	while (true) {
		co_await gse::scenario::wait_frames(ctx, 1);
		std::this_thread::sleep_for(frame_pause);
	}
}

auto sandbox::scenarios::audio_reverb_flight(gse::scenario::context& ctx) -> gse::async::task<> {
	const auto flight_speed = gse::meters_per_second(24.f);
	const auto turn_at = gse::meters(6.f);
	const auto level = gse::meters(0.f);
	const auto start = audio_reverb_hall_listener_position();
	constexpr std::uint64_t flight_frames = 470;

	co_await gse::scenario::wait_settled(ctx);

	const auto listeners = ctx.owners<gse::audio::listener_component>();
	if (!ctx.expect(listeners.size() == 1)) {
		co_return;
	}
	const auto listener = listeners[0];

	ctx.channels().push<prefetch_audio_modal_request>({});
	ctx.channels().push<prefetch_audio_reverb_request>({});
	co_await wait_for_ambient_clips(ctx);
	ctx.channels().push<spawn_audio_modal_field_request>({});
	gse::log::println(
		gse::log::category::general,
		"audio_reverb_flight: frame {}, listener flying at {:.1f} between {:.1f} either side of {:.3f}, a field of {} hopping bars requested",
		ctx.frame(),
		flight_speed,
		turn_at,
		start,
		audio_modal_field_bars
	);

	auto along = level;
	auto heading = flight_speed;
	for (std::uint64_t flown = 0; flown < flight_frames; ++flown) {
		along += heading * gse::system_clock::fixed_dt();
		if (along > turn_at || along < -turn_at) {
			heading = -heading;
		}
		ctx.channels().push<gse::physics::set_request<^^gse::physics::transform_component::position>>({
			.target = listener,
			.value = start + gse::vec3<gse::displacement>(along, level, level),
		});
		co_await gse::scenario::wait_frames(ctx, 1);
	}
}

auto sandbox::scenarios::audio_yard_listen(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await wait_for_ambient_clips(ctx);
	gse::log::println(
		gse::log::category::general,
		"audio_yard_listen: frame {}, {} ambient emitters, {} listeners",
		ctx.frame(),
		ctx.owners<ambient_sound::component>().size(),
		ctx.owners<gse::audio::listener_component>().size()
	);
}
