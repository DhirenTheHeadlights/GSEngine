export module gse.graphics:controller_component;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse {
	struct animation_parameter {
		std::variant<bool, float> value;
		bool is_trigger = false;
	};

	struct blend_state {
		std::string from_state;
		std::string to_state;
		time blend_duration{};
		time blend_elapsed{};
		time from_time{};
		time to_time{};
		bool active = false;
	};

	struct controller_component_data {
		id graph_id;
	};

	struct controller_component : component<controller_component_data, controller_component_data> {
		controller_component(const id owner_id, const params& p) : component(owner_id, p) {}

		std::string current_state;
		time state_time{};
		bool state_playing = true;
		blend_state blend;
		std::unordered_map<std::string, animation_parameter> parameters;
		std::vector<mat4> blend_from_pose;
		std::vector<mat4> blend_to_pose;
	};
}
