export module gse.physics:joint_spec;

import std;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct fixed_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
	};

	struct distance_joint {
		length target;
	};

	struct hinge_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3f axis = { 0.f, 1.f, 0.f };
		std::optional<std::pair<angle, angle>> limits;
	};

	struct slider_joint {
		vec3f axis = { 0.f, 1.f, 0.f };
	};

	struct spring_joint {
		length target;
		inverse_mass compliance = per_kilograms(0.01f);
		float damping = 0.5f;
	};

	struct ball_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3<angular_stiffness> rest_stiffness = {};
		std::array<std::optional<std::pair<angle, angle>>, 3> limits = {};
	};

	struct universal_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3f twist_axis = { 0.f, 1.f, 0.f };
	};

	struct curve_segment {
		angle start;
		vec4f coefficients;
	};

	struct coordinate_curve {
		vec3f axis = { 1.f, 0.f, 0.f };
		std::optional<std::uint32_t> coordinate;
		std::vector<curve_segment> segments;
	};

	struct spatial_curve {
		std::array<coordinate_curve, 3> rotations;
		std::array<coordinate_curve, 3> translations;
	};

	struct curve_frame {
		vec3<displacement> anchor;
		quat orientation = quat(1.f, 0.f, 0.f, 0.f);
		spatial_curve transform;
	};

	struct coupled_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		quat frame_a = quat(1.f, 0.f, 0.f, 0.f);
		quat frame_b = quat(1.f, 0.f, 0.f, 0.f);
		spatial_curve transform;
		std::vector<curve_frame> frames;
		std::array<std::optional<std::pair<angle, angle>>, 3> limits = {};
	};

	using joint_config = std::variant<fixed_joint, distance_joint, hinge_joint, slider_joint, spring_joint, ball_joint, universal_joint, coupled_joint>;

	struct joint_spec {
		[[= networked]] id entity_a;
		[[= networked]] id entity_b;
		[[= networked]] joint_config config;
		bool resolved = false;
	};
}
