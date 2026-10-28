export module sandbox:scenario_camera;

import std;
import gse;

export namespace sandbox::scenario_camera {
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

	auto tumbler_target(
		float progress
	) -> gse::camera::target;

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

	auto sunset_target(
		float progress
	) -> gse::camera::target;

	auto cloud_target(
		float progress
	) -> gse::camera::target;

	auto sun_at(
		float progress
	) -> gse::renderer::atmosphere::sun_request;

	auto sunset_sun_at(
		float progress
	) -> gse::renderer::atmosphere::sun_request;

	auto cloud_sun_at(
		float progress
	) -> gse::renderer::atmosphere::sun_request;

	auto sunset_weather_at(
		float progress
	) -> gse::renderer::cloud::weather_request;

	auto cloud_weather_at(
		float progress
	) -> gse::renderer::cloud::weather_request;
}

auto sandbox::scenario_camera::lighting_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(14.f), gse::meters(0.f)),
		gse::meters(62.f),
		gse::meters(16.f),
		gse::degrees(20.f) + gse::degrees(130.f) * progress
	);
}

auto sandbox::scenario_camera::hall_target(const float progress) -> gse::camera::target {
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

auto sandbox::scenario_camera::horizon_probe_target(const float progress) -> gse::camera::target {
	const auto pitch = gse::degrees(45.f) - gse::degrees(105.f) * progress;

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
		.orientation = gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(70.f),
	};
}

auto sandbox::scenario_camera::glare_probe_target(const float progress) -> gse::camera::target {
	const int hold = std::min(static_cast<int>(progress * 3.f), 2);
	const auto pitch = gse::degrees(-6.f) - gse::degrees(22.f) * static_cast<float>(hold);

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, gse::degrees(-135.f)) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(70.f),
	};
}

auto sandbox::scenario_camera::sky_target(const float progress) -> gse::camera::target {
	const auto sun = sun_at(progress);
	const auto pitch = std::clamp(sun.elevation * 0.5f, gse::degrees(0.f), gse::degrees(37.f));

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, sun.azimuth) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(75.f),
	};
}

auto sandbox::scenario_camera::tumbler_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(13.f), gse::meters(0.f)),
		gse::meters(82.f),
		gse::meters(40.f),
		gse::degrees(-45.f) + gse::degrees(115.f) * progress
	);
}

auto sandbox::scenario_camera::cloud_target(const float progress) -> gse::camera::target {
	const auto yaw = gse::degrees(55.f) + gse::degrees(135.f) * progress;
	const auto pitch = gse::degrees(20.f);

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(30.f), gse::meters(0.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, yaw) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(70.f),
	};
}

auto sandbox::scenario_camera::cloud_sun_at(const float progress) -> gse::renderer::atmosphere::sun_request {
	return {
		.elevation = gse::degrees(38.f) - gse::degrees(14.f) * progress,
		.azimuth = gse::degrees(70.f) + gse::degrees(85.f) * progress,
	};
}

auto sandbox::scenario_camera::cloud_weather_at(const float progress) -> gse::renderer::cloud::weather_request {
	return {
		.phase = progress,
	};
}

auto sandbox::scenario_camera::sunset_target(const float progress) -> gse::camera::target {
	const auto yaw = gse::degrees(82.f) + gse::degrees(247.5f) * progress;
	const auto pitch = gse::degrees(11.f);

	return {
		.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(45.f), gse::meters(0.f)),
		.orientation = gse::from_axis_angle(gse::axis_y, yaw) * gse::from_axis_angle(gse::axis_x, pitch),
		.fov = gse::degrees(66.f),
	};
}

auto sandbox::scenario_camera::sunset_sun_at(const float progress) -> gse::renderer::atmosphere::sun_request {
	return {
		.elevation = gse::degrees(24.f) - gse::degrees(22.f) * progress,
		.azimuth = gse::degrees(94.f) + gse::degrees(190.f) * progress,
	};
}

auto sandbox::scenario_camera::sunset_weather_at(const float progress) -> gse::renderer::cloud::weather_request {
	return {
		.phase = 0.51f * progress,
	};
}

auto sandbox::scenario_camera::sun_at(const float progress) -> gse::renderer::atmosphere::sun_request {
	const auto phase = gse::radians(std::numbers::pi_v<float> * progress);

	return {
		.elevation = gse::degrees(-14.f) + gse::degrees(84.f) * gse::sin(phase),
		.azimuth = gse::degrees(40.f) + gse::degrees(140.f) * progress,
	};
}

auto sandbox::scenario_camera::orbit_at(const gse::vec3<gse::position>& center, const gse::length radius, const gse::length height, const gse::angle theta) -> gse::camera::target {
	const auto pitch = gse::atan2(-height, radius);

	return {
		.position = center + gse::vec3<gse::length>(radius * gse::sin(theta), height, radius * gse::cos(theta)),
		.orientation = gse::from_axis_angle(gse::axis_y, theta) * gse::from_axis_angle(gse::axis_x, pitch),
	};
}

auto sandbox::scenario_camera::orbit_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(3.f), gse::meters(0.f)),
		gse::meters(48.f),
		gse::meters(20.f),
		gse::degrees(35.f) + gse::degrees(70.f) * progress
	);
}

auto sandbox::scenario_camera::pyramid_target(const float progress) -> gse::camera::target {
	return orbit_at(
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(22.f), gse::meters(0.f)),
		gse::meters(115.f),
		gse::meters(30.f),
		gse::degrees(-28.f) + gse::degrees(56.f) * progress
	);
}

