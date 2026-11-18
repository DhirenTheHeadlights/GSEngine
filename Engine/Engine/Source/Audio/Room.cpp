module gse.audio:room_impl;

import std;

import gse.assets;
import gse.core;
import gse.ecs;
import gse.math;
import gse.physics;

import :material;
import :reverb_network;
import :room;
import :spatial;

auto gse::audio::room_direction(const std::uint32_t ray) -> vec3f {
	const angle golden = radians(2.39996323f);

	const float height = 1.f - 2.f * (static_cast<float>(ray) + 0.5f) / static_cast<float>(room_ray_count);
	const float ring = std::sqrt(1.f - height * height);
	const angle around = golden * static_cast<float>(ray);
	return vec3f(ring * cos(around), height, ring * sin(around));
}

auto gse::audio::surface_absorption(const room_scene& scene, const id owner) -> float {
	constexpr float unlined = 0.1f;

	const auto* lining = scene.materials.find(owner);
	if (lining == nullptr) {
		return unlined;
	}
	const auto material = asset::try_get<acoustic_material>(scene.assets, lining->material);
	return material.valid() ? material.resolve()->properties().absorption : unlined;
}

auto gse::audio::room_horizon() -> length {
	return meters(100.f);
}

auto gse::audio::trace_ray(const room_scene& scene, const vec3f& direction) -> room_ray {
	const length horizon = room_horizon();

	room_ray ray;
	vec3<position> from = scene.ear;
	vec3f heading = direction;
	std::array ignored{ scene.unheard[0], scene.unheard[1], id{} };
	for (std::uint32_t bounce = 0; bounce < room_bounce_count; ++bounce) {
		const auto hit = physics::nearest_hit(scene.targets, from, from + heading * horizon, ignored);
		++ray.surfaces;
		if (!hit) {
			ray.absorbed += 1.f;
			return ray;
		}
		ray.absorbed += surface_absorption(scene, hit->owner);
		if (bounce == 0) {
			ray.first_reach = hit->distance;
		}
		else {
			ray.chord_length += hit->distance;
			++ray.chords;
		}
		const vec3f normal = normalize(hit->normal);
		from = from + heading * hit->distance;
		heading = normalize(reflect(heading, normal));
		ignored[2] = hit->owner;
	}
	ray.enclosed = true;
	return ray;
}

auto gse::audio::trace_room(room& traced, const room_scene& scene, const std::uint32_t ray_count) -> void {
	const length horizon = room_horizon();
	const std::array axes{
		vec3f(1.f, 0.f, 0.f),
		vec3f(-1.f, 0.f, 0.f),
		vec3f(0.f, 1.f, 0.f),
		vec3f(0.f, -1.f, 0.f),
		vec3f(0.f, 0.f, 1.f),
		vec3f(0.f, 0.f, -1.f),
	};

	for (std::uint32_t count = 0; count < ray_count; ++count) {
		traced.rays[traced.next_ray] = trace_ray(scene, room_direction(traced.next_ray));
		traced.next_ray = (traced.next_ray + 1) % room_ray_count;
	}

	for (const auto [wall, toward] : std::views::zip(traced.walls, axes)) {
		const auto hit = physics::nearest_hit(scene.targets, scene.ear, scene.ear + toward * horizon, scene.unheard);
		wall = {
			.toward = toward,
			.reach = hit ? hit->distance : length{},
			.absorption = hit ? surface_absorption(scene, hit->owner) : 1.f,
		};
	}

	traced.traced = true;
	traced.heard = estimate_room(traced);
	traced.ear = scene.ear;
	traced.listener = scene.unheard[0];
}

auto gse::audio::estimate_room(const room& traced) -> room_estimate {
	constexpr float least_absorption = 0.01f;
	constexpr float most_absorption = 0.99f;
	constexpr float sixty_decibels = 6.f * std::numbers::ln10_v<float>;
	constexpr float sphere = 4.f * std::numbers::pi_v<float> / 3.f;
	const length reference = meters(1.f);

	float hit_rate = 0.f;
	std::uint32_t rated = 0;
	float absorbed = 0.f;
	std::uint32_t surfaces = 0;
	std::uint32_t enclosed = 0;
	volume reached;
	std::uint32_t reaching = 0;
	for (const auto& ray : traced.rays) {
		if (ray.chords > 0) {
			hit_rate += static_cast<float>(ray.chords) * (reference / ray.chord_length);
			++rated;
		}
		absorbed += ray.absorbed;
		surfaces += ray.surfaces;
		enclosed += ray.enclosed ? 1 : 0;
		if (ray.first_reach > length{}) {
			reached += ray.first_reach * ray.first_reach * ray.first_reach;
			++reaching;
		}
	}
	if (enclosed == 0 || hit_rate <= 0.f) {
		return {};
	}

	const length mean_free_path = reference * (static_cast<float>(rated) / hit_rate);
	const float absorption = std::clamp(absorbed / static_cast<float>(surfaces), least_absorption, most_absorption);
	const float enclosed_share = static_cast<float>(enclosed) / static_cast<float>(room_ray_count);
	const volume size = reached * (sphere / static_cast<float>(reaching));
	const area surface = size / mean_free_path * 4.f;
	const area room_constant = surface * (absorption / (1.f - absorption));
	const float reverberant = 16.f * std::numbers::pi_v<float> * (reference * reference / room_constant);

	return {
		.mean_free_path = mean_free_path,
		.absorption = absorption,
		.enclosed = enclosed_share,
		.size = size,
		.radius = reference * std::cbrt(size / (reference * reference * reference) / sphere),
		.decay = mean_free_path / speed_of_sound() * (sixty_decibels / -std::log(1.f - absorption)),
		.send = std::sqrt(reverberant) * enclosed_share,
	};
}

auto gse::audio::reverb_of(const room& traced, const physics::transform_component& listener) -> reverb_setting {
	const length reference = meters(1.f);

	const auto& heard = traced.heard;
	if (heard.enclosed <= 0.f) {
		return {};
	}

	reverb_setting setting{
		.spacing = heard.mean_free_path / speed_of_sound(),
		.decay = traced.held_decay,
		.send = heard.send,
	};
	const vec3f rightward = rotate_vector(listener.orientation, vec3f(1.f, 0.f, 0.f));
	for (const auto [early, wall] : std::views::zip(setting.early, traced.walls)) {
		if (wall.reach <= length{}) {
			continue;
		}
		const length path = wall.reach * 2.f;
		const float gain = (1.f - wall.absorption) * heard.enclosed * (reference / std::max(path, reference));
		const angle pan = degrees(45.f) * (dot(wall.toward, rightward) + 1.f);
		early = {
			.delay = path / speed_of_sound(),
			.left = gain * cos(pan),
			.right = gain * sin(pan),
		};
	}
	return setting;
}

auto gse::audio::settle_reflections(room& traced, reverb_setting& setting, const listener_motion& motion) -> void {
	constexpr std::uint32_t fade_steps = 3 * (room_ray_count / room_rays_per_step);

	for (const auto [early, tap] : std::views::zip(setting.early, traced.taps)) {
		if (motion.jumped) {
			tap = {
				.delay = early.delay,
			};
			continue;
		}
		const bool continuous = std::max(early.delay, tap.delay) - std::min(early.delay, tap.delay) <= motion.slide;
		if (!continuous && tap.settling_steps == 0) {
			tap.settling_steps = 2 * fade_steps;
		}
		if (tap.settling_steps == 0) {
			tap.delay = early.delay;
			continue;
		}
		if (tap.settling_steps <= fade_steps) {
			tap.delay = early.delay;
		}
		tap.settling_steps -= std::min(tap.settling_steps, motion.steps);
		early = {
			.delay = tap.delay,
		};
	}
}
