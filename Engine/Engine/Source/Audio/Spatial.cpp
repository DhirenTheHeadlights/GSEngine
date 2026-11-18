module gse.audio:spatial_impl;

import std;

import gse.core;
import gse.math;
import gse.physics;

import :mixer;
import :spatial;

auto gse::audio::speed_of_sound() -> velocity {
	return meters_per_second(343.f);
}

auto gse::audio::head_radius() -> length {
	return centimeters(8.75f);
}

auto gse::audio::place(const physics::transform_component& listener, const emission& source, const listening_mode listening) -> placement {
	constexpr float fastest_approach = 0.5f;
	constexpr float blocked_gain = 0.355f;
	const length near_field = meters(1.f);
	const length coincident = millimeters(1.f);
	const length absorption_reference = meters(10.f);
	const frequency absorption_reference_cutoff = kilohertz(16.4f);
	const frequency blocked_cutoff = kilohertz(1.f);

	const auto offset = source.position - listener.position;
	const length distance = magnitude(offset);
	const length heard = std::max(distance, near_field);
	const bool apart = distance > coincident;

	const float side = apart ? dot(offset, rotate_vector(listener.orientation, vec3f(1.f, 0.f, 0.f))) / distance : 0.f;
	const velocity receding = apart ? dot(source.velocity, offset) / distance : velocity{};
	const velocity closing_limit = speed_of_sound() * -fastest_approach;

	const float attenuation = near_field / heard * std::pow(blocked_gain, source.occluded);
	const time travel = distance / (speed_of_sound() + std::max(receding, closing_limit));
	const frequency shadow_corner = speed_of_sound() / (head_radius() * std::numbers::pi_v<float>);
	const frequency air_cutoff = absorption_reference_cutoff * std::sqrt(absorption_reference / heard);
	const frequency absorption_cutoff = air_cutoff * std::pow(std::min(1.f, blocked_cutoff / air_cutoff), source.occluded);

	if (listening == listening_mode::speakers) {
		const angle pan = degrees(45.f) * (side + 1.f);
		return {
			.left = { .delay = travel, .gain = percentage<float>::fraction(cos(pan) * attenuation) },
			.right = { .delay = travel, .gain = percentage<float>::fraction(sin(pan) * attenuation) },
			.shadow_corner = shadow_corner,
			.absorption_cutoff = absorption_cutoff,
		};
	}

	constexpr float shadow_mean = 1.05f;
	constexpr float shadow_swing = 0.95f;
	constexpr float shadow_angle_scale = 1.2f;
	const angle beside = degrees(90.f);
	const time head_crossing = head_radius() / speed_of_sound();

	const auto ear_facing = [&](const float toward) -> ear {
		const angle incidence = acos(std::clamp(toward, -1.f, 1.f));
		const time around_head = incidence < beside ? head_crossing * -cos(incidence) : head_crossing * ((incidence - beside) / radians(1.f));
		return {
			.delay = travel + around_head,
			.gain = percentage<float>::fraction(attenuation),
			.shadow = shadow_mean + shadow_swing * cos(incidence * shadow_angle_scale),
		};
	};

	return {
		.left = ear_facing(-side),
		.right = ear_facing(side),
		.shadow_corner = shadow_corner,
		.absorption_cutoff = absorption_cutoff,
	};
}
