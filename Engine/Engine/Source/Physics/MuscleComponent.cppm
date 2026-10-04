export module gse.physics:muscle_component;

import std;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	constexpr std::size_t max_muscle_path_points = 8;

	struct muscle_attachment {
		id entity;
		vec3<displacement> local_point;
	};

	struct muscle_properties {
		force max_isometric_force = newtons(3200.f);
		length optimal_fiber_length;
		length tendon_slack_length;
		angle pennation_at_optimal = {};
		inverse_time max_contraction_rate = per_second(10.f);
	};

	struct muscle_spec {
		[[= networked]] std::array<muscle_attachment, max_muscle_path_points> path{};
		[[= networked]] std::uint32_t path_count = 0;
		[[= networked]] muscle_properties properties;
		bool resolved = false;
	};

	struct muscle_component {
		[[= networked]] float excitation = 0.f;
	};
}