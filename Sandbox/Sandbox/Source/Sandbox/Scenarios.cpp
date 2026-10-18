module sandbox:scenarios_impl;

import std;
import gse;
import gse.scenario;

import :dev_spawn_system;
import :scenarios;

namespace sandbox::scenarios {
	auto build_stress_workload(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto push_pyramid_impulses(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto walk_pattern(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

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

	auto orbit_at(
		const gse::vec3<gse::position>& center,
		gse::length radius,
		gse::length height,
		gse::angle theta
	) -> gse::camera::target;

	auto orbit_target(
		float progress
	) -> gse::camera::target;

	auto pyramid_target(
		float progress
	) -> gse::camera::target;

	auto sky_target(
		float progress
	) -> gse::camera::target;

	auto sun_at(
		float progress
	) -> gse::renderer::atmosphere::sun_request;

	auto lighting_target(
		float progress
	) -> gse::camera::target;

	auto hall_target(
		float progress
	) -> gse::camera::target;

	auto horizon_probe_target(
		float progress
	) -> gse::camera::target;

	auto glare_probe_target(
		float progress
	) -> gse::camera::target;

	auto pitch_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	auto sunset_target(
		float progress
	) -> gse::camera::target;

	auto sunset_sun_at(
		float progress
	) -> gse::renderer::atmosphere::sun_request;

	auto sunset_weather_at(
		float progress
	) -> gse::renderer::cloud::weather_request;

	auto cloud_target(
		float progress
	) -> gse::camera::target;

	auto cloud_sun_at(
		float progress
	) -> gse::renderer::atmosphere::sun_request;

	auto cloud_weather_at(
		float progress
	) -> gse::renderer::cloud::weather_request;

	auto tumbler_target(
		float progress
	) -> gse::camera::target;
}

auto sandbox::scenarios::lighting_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(14.f), gse::meters(0.f)),
		gse::meters(62.f),
		gse::meters(16.f),
		gse::degrees(20.f) + gse::degrees(130.f) * progress
	);
}

auto sandbox::scenarios::hall_target(const float progress) -> gse::camera::target {
	const auto sweep = gse::degrees(360.f) * progress;
	const auto yaw = gse::degrees(24.f) * gse::cos(sweep);
	const auto pitch = gse::degrees(-3.f);

	return {
		.position = gse::vec3<gse::position>(
			gse::meters(9.f) * gse::sin(sweep),
			gse::meters(5.5f),
			gse::meters(74.f) - gse::meters(110.f) * progress
		),
		.orientation = gse::from_axis_angle(gse::axis_y, yaw) * gse::from_axis_angle(gse::axis_x, pitch),
	};
}

auto sandbox::scenarios::horizon_probe_target(const float progress) -> gse::camera::target {
	const auto pitch = gse::degrees(45.f) - gse::degrees(105.f) * progress;

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
		.orientation = gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(70.f),
	};
}

auto sandbox::scenarios::glare_probe_target(const float progress) -> gse::camera::target {
	const int hold = std::min(static_cast<int>(progress * 3.f), 2);
	const auto pitch = gse::degrees(-6.f) - gse::degrees(22.f) * static_cast<float>(hold);

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, gse::degrees(-135.f)) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(70.f),
	};
}

auto sandbox::scenarios::sky_target(const float progress) -> gse::camera::target {
	const auto sun = sun_at(progress);
	const auto pitch = std::clamp(sun.elevation * 0.5f, gse::degrees(0.f), gse::degrees(37.f));

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, sun.azimuth) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(75.f),
	};
}

auto sandbox::scenarios::tumbler_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(13.f), gse::meters(0.f)),
		gse::meters(82.f),
		gse::meters(40.f),
		gse::degrees(-45.f) + gse::degrees(115.f) * progress
	);
}

auto sandbox::scenarios::cloud_target(const float progress) -> gse::camera::target {
	const auto yaw = gse::degrees(55.f) + gse::degrees(135.f) * progress;
	const auto pitch = gse::degrees(20.f);

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(30.f), gse::meters(0.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, yaw) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(70.f),
	};
}

auto sandbox::scenarios::cloud_sun_at(const float progress) -> gse::renderer::atmosphere::sun_request {
	return {
		.elevation = gse::degrees(38.f) - gse::degrees(14.f) * progress,
		.azimuth = gse::degrees(70.f) + gse::degrees(85.f) * progress,
	};
}

auto sandbox::scenarios::cloud_weather_at(const float progress) -> gse::renderer::cloud::weather_request {
	return {
		.phase = progress,
	};
}

auto sandbox::scenarios::sunset_target(const float progress) -> gse::camera::target {
	const auto yaw = gse::degrees(82.f) + gse::degrees(247.5f) * progress;
	const auto pitch = gse::degrees(11.f);

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(45.f), gse::meters(0.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, yaw) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(66.f),
	};
}

auto sandbox::scenarios::sunset_sun_at(const float progress) -> gse::renderer::atmosphere::sun_request {
	return {
		.elevation = gse::degrees(24.f) - gse::degrees(22.f) * progress,
		.azimuth = gse::degrees(94.f) + gse::degrees(190.f) * progress,
	};
}

auto sandbox::scenarios::sunset_weather_at(const float progress) -> gse::renderer::cloud::weather_request {
	return {
		.phase = 0.51f * progress,
	};
}

auto sandbox::scenarios::sun_at(const float progress) -> gse::renderer::atmosphere::sun_request {
	const auto phase = gse::radians(std::numbers::pi_v<float> * progress);

	return {
		.elevation = gse::degrees(-14.f) + gse::degrees(84.f) * gse::sin(phase),
		.azimuth = gse::degrees(40.f) + gse::degrees(140.f) * progress,
	};
}

auto sandbox::scenarios::orbit_at(const gse::vec3<gse::position>& center, const gse::length radius, const gse::length height, const gse::angle theta) -> gse::camera::target {
	const auto pitch = gse::atan2(-height, radius);

	return {
		.position = center + gse::vec3<gse::length>(radius * gse::sin(theta), height, radius * gse::cos(theta)),
		.orientation = gse::from_axis_angle(gse::axis_y, theta) * gse::from_axis_angle(gse::axis_x, pitch),
	};
}

auto sandbox::scenarios::orbit_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(3.f), gse::meters(0.f)),
		gse::meters(48.f),
		gse::meters(20.f),
		gse::degrees(35.f) + gse::degrees(70.f) * progress
	);
}

auto sandbox::scenarios::pyramid_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(22.f), gse::meters(0.f)),
		gse::meters(115.f),
		gse::meters(30.f),
		gse::degrees(-28.f) + gse::degrees(56.f) * progress
	);
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

auto sandbox::scenarios::pyramid_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	co_await gse::scenario::wait_frames(ctx, 500);
	expect_bodies_sane(ctx);
}

auto sandbox::scenarios::pyramid_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
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

auto sandbox::scenarios::walk_pattern(gse::scenario::context& ctx) -> gse::async::task<> {
	const auto press = [&ctx](const gse::key key) {
		ctx.channels().push<gse::input::synthetic_input_request>({
			.value = gse::input::key_pressed{ .key_code = key },
		});
	};
	const auto release = [&ctx](const gse::key key) {
		ctx.channels().push<gse::input::synthetic_input_request>({
			.value = gse::input::key_released{ .key_code = key },
		});
	};

	co_await gse::scenario::wait(ctx, gse::seconds(6.f));
	press(gse::key::w);
	co_await gse::scenario::wait(ctx, gse::seconds(4.f));
	release(gse::key::w);
	press(gse::key::a);
	co_await gse::scenario::wait(ctx, gse::seconds(2.f));
	release(gse::key::a);
	press(gse::key::w);
	press(gse::key::d);
	co_await gse::scenario::wait(ctx, gse::seconds(3.f));
	release(gse::key::w);
	release(gse::key::d);
	co_await gse::scenario::wait(ctx, gse::seconds(1.f));
	press(gse::key::s);
	co_await gse::scenario::wait(ctx, gse::seconds(3.f));
	release(gse::key::s);
}

auto sandbox::scenarios::net_walk_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await walk_pattern(ctx);
}

auto sandbox::scenarios::net_walk_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
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

auto sandbox::scenarios::parity_stress_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::parity_stress_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::parity_drop_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_drop_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_pair_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_pair_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_stack_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_stack_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_cluster_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_cluster_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_heap_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_heap_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_mound_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_mound_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_pile_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_hull_pile_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_hull_pile_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_pile_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_nojoints_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_stress_request>({});
}

auto sandbox::scenarios::parity_empty_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_jointsonly_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_joints_request>({});
}

auto sandbox::scenarios::parity_jointsonly_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
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

auto sandbox::scenarios::parity_overlap_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_overlap_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_shapes_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_shapes_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_domino_cpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_domino_gpu(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}
