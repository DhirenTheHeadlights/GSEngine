export module gs:controlled_joint;

import std;
import gse;

export namespace gs {
	struct controlled_joint_component {
		gse::vec3f hinge_axis = { 0.f, 0.f, 1.f };
		gse::angle target_angle = gse::radians(0.f);
		gse::id flexor_muscle;
		gse::id extensor_muscle;
		float gain = 4.f;
		float damping = 1.0f;
		gse::quat rest_orientation = gse::quat(1.f, 0.f, 0.f, 0.f);
		gse::angle prev_angle = gse::radians(0.f);
		bool rest_initialized = false;
	};
}
