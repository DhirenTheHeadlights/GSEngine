export module gse.audio:contact;

import std;

import gse.core;
import gse.math;
import gse.physics;

import :material;
import :mixer;

export namespace gse::audio {
	constexpr std::uint32_t contact_starts_per_frame = 8;
	constexpr std::uint32_t contact_voice_limit = 32;
	constexpr std::uint32_t modal_voice_limit = 64;

	struct contact_sound_component {
		id clip;
		percentage<float> gain = percentage<float>::one();
	};

	auto audible(
		velocity relative_speed,
		impulse delivered
	) -> bool;

	auto contact_loudness(
		velocity relative_speed
	) -> float;

	auto reduced_mass(
		mass first,
		mass second
	) -> mass;

	struct strike {
		const acoustic_material& material;
		const acoustic_box& box;
		const physics::transform_component& body;
		const physics::contact_event& contact;
		impulse delivered;
		percentage<float> gain = percentage<float>::one();
	};

	auto drive_of(
		const strike& blow
	) -> strike_drive;

	auto ring_time(
		const acoustic_box& box
	) -> time;

	constexpr std::uint32_t rub_voice_limit = 16;
	constexpr std::uint64_t rub_release_steps = 12;

	enum class rub_kind : std::uint8_t {
		slide,
		roll,
	};

	struct rub {
		rub_kind kind = rub_kind::slide;
		float level = 0.f;
		frequency cutoff;
		frequency wobble;
		float wobble_depth = 0.f;
	};

	auto rub_of(
		const physics::moving_contact& moving,
		length lever,
		percentage<float> gain
	) -> rub;

	auto rub_drive(
		const acoustic_box& box,
		const physics::transform_component& body,
		const vec3f& normal
	) -> strike_drive;

	auto contact_damping() -> inverse_time;
}
