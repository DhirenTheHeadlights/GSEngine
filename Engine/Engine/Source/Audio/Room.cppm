export module gse.audio:room;

import std;

import gse.assets;
import gse.core;
import gse.ecs;
import gse.math;
import gse.physics;

import :material;
import :reverb_network;
import :spatial;

export namespace gse::audio {
	constexpr std::uint32_t room_ray_count = 64;
	constexpr std::uint32_t room_bounce_count = 6;
	constexpr std::uint32_t room_rays_per_step = 8;

	struct room_ray {
		length chord_length;
		std::uint32_t chords = 0;
		std::uint32_t surfaces = 0;
		float absorbed = 0.f;
		length first_reach;
		bool enclosed = false;
	};

	struct room_wall {
		vec3f toward;
		length reach;
		float absorption = 1.f;
	};

	struct room_estimate {
		length mean_free_path;
		float absorption = 1.f;
		float enclosed = 0.f;
		volume size;
		length radius;
		time decay;
		float send = 0.f;
	};

	struct room_tap {
		time delay;
		std::uint32_t settling_steps = 0;
	};

	struct listener_motion {
		time slide;
		std::uint32_t steps = 0;
		bool jumped = false;
	};

	struct room {
		std::array<room_ray, room_ray_count> rays{};
		std::array<room_wall, reverb_reflection_count> walls{};
		std::uint32_t next_ray = 0;
		std::uint64_t traced_step = 0;
		bool traced = false;
		room_estimate heard;
		time held_decay;
		std::array<room_tap, reverb_reflection_count> taps{};
		vec3<position> ear;
		id listener;
	};

	struct room_scene {
		std::span<const physics::ray_target> targets;
		const write<acoustic_material_component>& materials;
		shared_view<asset::data> assets;
		vec3<position> ear;
		std::array<id, 2> unheard;
	};

	auto trace_room(
		room& traced,
		const room_scene& scene,
		std::uint32_t ray_count
	) -> void;

	auto estimate_room(
		const room& traced
	) -> room_estimate;

	auto reverb_of(
		const room& traced,
		const physics::transform_component& listener
	) -> reverb_setting;

	auto settle_reflections(
		room& traced,
		reverb_setting& setting,
		const listener_motion& motion
	) -> void;
}

namespace gse::audio {
	auto room_direction(
		std::uint32_t ray
	) -> vec3f;

	auto room_horizon() -> length;

	auto surface_absorption(
		const room_scene& scene,
		id owner
	) -> float;

	auto trace_ray(
		const room_scene& scene,
		const vec3f& direction
	) -> room_ray;
}
