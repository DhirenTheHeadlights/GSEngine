export module gse.physics:muscle_component;

import std;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	constexpr std::size_t max_muscle_path_points = 8;
	constexpr std::size_t max_muscle_wraps = 2;

	struct muscle_attachment {
		id entity;
		vec3<displacement> local_point;
		std::optional<std::uint32_t> frame;
	};

	struct muscle_wrap_cylinder {
		vec3<displacement> center;
		quat orientation = quat(1.f, 0.f, 0.f, 0.f);
		length radius;
		length half_length;
		vec3f side;
		std::uint32_t first_segment = 0;
		std::uint32_t segment_end = 0;
	};

	struct muscle_wrap {
		id entity;
		muscle_wrap_cylinder cylinder;
	};

	struct muscle_properties {
		force max_isometric_force = newtons(3200.f);
		length optimal_fiber_length;
		length tendon_slack_length;
		angle pennation_at_optimal = {};
		inverse_time max_contraction_rate = per_second(10.f);
		std::optional<float> tendon_strain_at_max_force;
	};

	struct muscle_spec {
		[[= networked]] std::array<muscle_attachment, max_muscle_path_points> path{};
		[[= networked]] std::uint32_t path_count = 0;
		[[= networked]] std::array<muscle_wrap, max_muscle_wraps> wraps{};
		[[= networked]] std::uint32_t wrap_count = 0;
		[[= networked]] muscle_properties properties;
		bool resolved = false;
	};

	struct muscle_component {
		[[= networked]] float excitation = 0.f;
	};
}