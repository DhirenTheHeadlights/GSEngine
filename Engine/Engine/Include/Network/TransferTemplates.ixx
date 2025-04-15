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
import gse.platform.glfw.input;

export namespace gse::network {
	#pragma pack(push, 1) // Transfer Structs must have no padding
	struct motion_component_transfer {
		uint32_t parent_id;
		vec3<length> current_position;
		vec3<velocity> current_velocity;
		vec3<acceleration> current_acceleration;
		vec3<torque> current_torque;
		quat orientation;
		vec3<angular_velocity> angular_velocity;
		vec3<angular_acceleration> angular_acceleration;
		velocity max_speed;
		mass mass;
		length most_recent_y_collision;
		float moment_of_inertia;
		bool affected_by_gravity;
		bool moving;
		bool airborne;
		bool self_controlled;
	};

	struct collision_component_transfer {
		uint32_t parent_id;
		axis_aligned_bounding_box bounding_box;
		oriented_bounding_box oriented_bounding_box;
		collision_information collision_information;
		bool resolve_collisions;
	};

	struct render_component_transfer {
		uint32_t parent_id;
		std::vector<model_handle> models;
		bool render;
		bool render_bounding_boxes;
	};
	
	struct input_transfer {
		std::vector<input::ca
	};
	#pragma pack(pop) // Restore default alignment
} 