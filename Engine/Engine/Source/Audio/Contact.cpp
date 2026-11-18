module gse.audio:contact_impl;

import std;

import gse.core;
import gse.math;
import gse.physics;

import :contact;
import :material;
import :mixer;
import :modal;

auto gse::audio::audible(const velocity relative_speed, const impulse delivered) -> bool {
	const velocity slowest_heard = meters_per_second(0.2f);
	const impulse lightest_heard = newton_seconds(0.05f);
	return relative_speed >= slowest_heard && delivered >= lightest_heard;
}

auto gse::audio::contact_loudness(const velocity relative_speed) -> float {
	const velocity full_scale = meters_per_second(5.f);
	return std::clamp(relative_speed / full_scale, 0.f, 1.f);
}

auto gse::audio::reduced_mass(const mass first, const mass second) -> mass {
	if (first <= mass{}) {
		return second;
	}
	if (second <= mass{}) {
		return first;
	}
	return first * (second / (first + second));
}

auto gse::audio::drive_of(const strike& blow) -> strike_drive {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;
	constexpr float unit_cube_volume = 8.f;
	const acceleration full_scale = meters_per_second_squared(4.8e5f);

	const auto& [material, box, body, contact, delivered, gain] = blow;
	const quat into_body = conjugate(body.orientation);
	const vec3<current_position> touched = contact.point;
	const vec3<displacement> offset = rotate_vector(into_body, touched - body.position);
	const vec3<displacement> half = box.size * 0.5f;
	const vec3f at(
		std::clamp(offset.x() / half.x(), -1.f, 1.f),
		std::clamp(offset.y() / half.y(), -1.f, 1.f),
		std::clamp(offset.z() / half.z(), -1.f, 1.f)
	);
	const vec3f along = rotate_vector(into_body, contact.normal);

	const mass ringing_mass = material.properties().mass_density * (box.size.x() * box.size.y() * box.size.z());

	strike_drive drive{};
	for (std::size_t mode = 0; mode < box.rings.storage.size(); ++mode) {
		const float moved = dot(mode_displacement(mode_shape(box, mode), at), along);
		const inverse_time turning = box.rings.storage[mode].pitch * turn;
		const acceleration push = delivered * turning / ringing_mass * (unit_cube_volume * moved * moved);
		drive[mode] = gain.value() * (push / full_scale);
	}
	return drive;
}

auto gse::audio::ring_time(const acoustic_box& box) -> time {
	const float faded = 7.f * std::numbers::ln10_v<float>;
	const inverse_time slowest = std::ranges::min(box.rings.storage, {}, &resonance::decay).decay;
	return faded / slowest;
}

auto gse::audio::rub_of(const physics::moving_contact& moving, const length lever, const percentage<float> gain) -> rub {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;
	constexpr float slide_gain = 0.01f;
	constexpr float roll_gain = 0.01f;
	constexpr float roll_wobble_depth = 0.25f;
	constexpr float loudest = 0.5f;
	const length slide_grain = millimeters(0.2f);
	const length roll_grain = millimeters(2.f);
	const velocity quietest = meters_per_second(0.03f);
	const velocity reference_speed = meters_per_second(1.f);
	const force reference_load = newtons(100.f);
	const frequency brightest = hertz(16000.f);

	const bool rolling = moving.roll_speed > moving.slip_speed;
	const velocity speed = std::max(moving.slip_speed, moving.roll_speed);
	const float pressed = std::sqrt(std::max(moving.load / reference_load, 0.f));
	const float level = speed < quietest ? 0.f : gain.value() * (speed / reference_speed) * pressed;
	if (!rolling) {
		return {
			.kind = rub_kind::slide,
			.level = std::min(level * slide_gain, loudest),
			.cutoff = std::min<frequency>(speed / slide_grain, brightest),
		};
	}
	return {
		.kind = rub_kind::roll,
		.level = std::min(level * roll_gain, loudest),
		.cutoff = std::min<frequency>(speed / roll_grain, brightest),
		.wobble = moving.roll_speed / (lever * turn),
		.wobble_depth = roll_wobble_depth,
	};
}

auto gse::audio::rub_drive(const acoustic_box& box, const physics::transform_component& body, const vec3f& normal) -> strike_drive {
	const vec3f along = rotate_vector(conjugate(body.orientation), normal);
	const vec3f pressed(along.x() * along.x(), along.y() * along.y(), along.z() * along.z());

	strike_drive drive{};
	float total = 0.f;
	for (const auto [driven, share] : std::views::zip(drive, box.axis_shares.storage)) {
		driven = dot(share, pressed);
		total += driven;
	}
	for (float& driven : drive) {
		driven = std::sqrt(driven / total);
	}
	return drive;
}

auto gse::audio::contact_damping() -> inverse_time {
	return per_second(200.f);
}
