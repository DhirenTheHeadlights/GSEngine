export module gse.graphics:animation_graph;

import std;

import gse.utility;
import gse.math;

export namespace gse {
	enum class transition_condition_type {
		bool_equals,
		float_greater,
		float_less,
		trigger
	};

	struct transition_condition {
		std::string parameter_name;
		transition_condition_type type = transition_condition_type::bool_equals;
		float threshold = 0.f;
		bool bool_value = true;
	};

	struct animation_transition {
		std::string from_state;
		std::string to_state;
		time blend_duration;
		std::vector<transition_condition> conditions;
		bool has_exit_time = false;
		float exit_time_normalized = 1.f;
	};

	struct animation_state {
		std::string name;
		id clip_id;
		float speed = 1.f;
		bool loop = true;
	};

	struct animation_graph {
		std::string name;
		std::string default_state;
		std::vector<animation_state> states;
		std::vector<animation_transition> transitions;

		auto find_state(
			std::string_view state_name
		) const -> const animation_state*;

		auto find_transitions_from(
			std::string_view state_name
		) const -> std::vector<const animation_transition*>;
	};
}

auto gse::animation_graph::find_state(const std::string_view state_name) const -> const animation_state* {
	for (const auto& state : states) {
		if (state.name == state_name) {
			return &state;
		}
	}
	return nullptr;
}

auto gse::animation_graph::find_transitions_from(const std::string_view state_name) const -> std::vector<const animation_transition*> {
	std::vector<const animation_transition*> result;
	for (const auto& transition : transitions) {
		if (transition.from_state == state_name) {
			result.push_back(&transition);
		}
	}
	return result;
}
