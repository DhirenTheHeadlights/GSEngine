export module gse.audio:spatial;

import std;

import gse.core;
import gse.math;
import gse.physics;

import :mixer;

export namespace gse::audio {
	struct listener_component {
		id body;
	};

	enum class listening_mode : std::uint8_t {
		speakers,
		headphones,
	};

	auto speed_of_sound() -> velocity;

	auto head_radius() -> length;

	struct emission {
		vec3<current_position> position;
		vec3<velocity> velocity;
		float occluded = 0.f;
	};

	auto place(
		const physics::transform_component& listener,
		const emission& source,
		listening_mode listening
	) -> placement;
}
