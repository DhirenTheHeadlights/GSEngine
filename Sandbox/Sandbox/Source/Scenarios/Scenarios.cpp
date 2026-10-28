module sandbox:scenarios_impl;

import std;
import gse;
import gse.scenario;

import :dev_spawn_system;
import :scenario_camera;
import :scenarios;

namespace sandbox::scenarios {
	using namespace scenario_camera;

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

	auto pitch_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;
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

auto sandbox::scenarios::relaunch_probe(gse::scenario::context&) -> gse::async::task<> {
	gse::process::open_url("about:blank");
	gse::app::relaunch_self_on_exit();
	gse::app::add_relaunch_handoff({}, L"--engine-bench-scenario scene_reset_cpu");
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

auto sandbox::scenarios::parity_overlap(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_shapes(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}

auto sandbox::scenarios::parity_domino(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
}
