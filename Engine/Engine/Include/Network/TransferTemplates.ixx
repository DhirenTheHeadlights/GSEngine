module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
export module gse.network.transfer_templates;

import std;
import gse.graphics.model;
import gse.physics.bounding_box;
import gse.physics.math;

export namespace gse::network {
	#pragma pack(push, 1) // Transfer Structs must have no padding
	struct motion_component_transfer {
		uint32_t parent_id;
		gse::vec3<gse::length> current_position;
		gse::vec3<gse::velocity> current_velocity;
		gse::vec3<gse::acceleration> current_acceleration;
		gse::vec3<gse::torque> current_torque;
		gse::quat orientation;
		gse::vec3<gse::angular_velocity> angular_velocity;
		gse::vec3<gse::angular_acceleration> angular_acceleration;
		gse::velocity max_speed;
		gse::mass mass;
		gse::length most_recent_y_collision;
		float moment_of_inertia;
		bool affected_by_gravity;
		bool moving;
		bool airborne;
		bool self_controlled;
	};

	struct collision_component_transfer {
		uint32_t parent_id;
		gse::axis_aligned_bounding_box bounding_box;
		gse::oriented_bounding_box oriented_bounding_box;
		gse::collision_information collision_information;
		bool resolve_collisions;
	};

	struct render_component_transfer {
		uint32_t parent_id;
		std::vector<gse::model_handle> models;
		bool render;
		bool render_bounding_boxes;
	};
	#pragma pack(pop) // Restore default alignment
} 