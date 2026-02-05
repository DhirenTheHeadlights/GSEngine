export module gse.runtime:animation_graph_api;

import std;

import gse.utility;
import gse.graphics;

import :core_api;

export namespace gse::animation {
	auto create_graph(const animation_graph& graph) -> id {
		const auto graph_id = generate_id(graph.name);
		state_of<animation::state>().graphs.emplace(graph_id, graph);
		return graph_id;
	}

	auto build(const graph_builder& builder) -> id {
		animation_graph graph;
		graph.name = builder.name();
		graph.default_state = builder.default_state().name;

		std::unordered_map<std::string, state_handle> collected_states;

		auto add_state = [&](const state_handle& s) {
			if (!s.name.empty() && !collected_states.contains(s.name)) {
				collected_states[s.name] = s;
			}
		};

		add_state(builder.default_state());

		for (const auto& t : builder.get_transitions()) {
			add_state(t.from_state);
			add_state(t.to_state);
		}

		for (const auto& [name, s] : collected_states) {
			graph.states.push_back({
				.name = s.name,
				.clip_id = s.clip,
				.speed = s.speed,
				.loop = s.loop
			});
		}

		auto build_transition = [](const transition_def& t) {
			animation_transition transition;
			transition.from_state = t.from_state.name;
			transition.to_state = t.to_state.name;
			transition.blend_duration = seconds(t.blend_duration);

			for (const auto& c : t.conditions) {
				transition_condition cond;
				cond.parameter_name = c.param_name;
				cond.threshold = c.float_value;
				cond.bool_value = c.bool_value;

				switch (c.type) {
				case condition_type::bool_equals:
					cond.type = transition_condition_type::bool_equals;
					break;
				case condition_type::float_greater:
					cond.type = transition_condition_type::float_greater;
					break;
				case condition_type::float_less:
					cond.type = transition_condition_type::float_less;
					break;
				case condition_type::trigger:
					cond.type = transition_condition_type::trigger;
					break;
				case condition_type::exit_time:
					transition.has_exit_time = true;
					transition.exit_time_normalized = c.float_value;
					continue;
				}

				transition.conditions.push_back(cond);
			}

			return transition;
		};

		for (const auto& t : builder.get_transitions()) {
			if (t.from_any) {
				for (const auto& [name, s] : collected_states) {
					if (name != t.to_state.name) {
						auto transition = build_transition(t);
						transition.from_state = name;
						graph.transitions.push_back(transition);
					}
				}
			}
			else {
				graph.transitions.push_back(build_transition(t));
			}
		}

		return create_graph(graph);
	}

	auto set_parameter(controller_component& ctrl, const std::string_view name, const bool value) -> void {
		ctrl.parameters[std::string(name)] = animation_parameter{
			.value = value,
			.is_trigger = false
		};
	}

	auto set_parameter(controller_component& ctrl, const std::string_view name, const float value) -> void {
		ctrl.parameters[std::string(name)] = animation_parameter{
			.value = value,
			.is_trigger = false
		};
	}

	auto set_trigger(controller_component& ctrl, const std::string_view name) -> void {
		ctrl.parameters[std::string(name)] = animation_parameter{
			.value = true,
			.is_trigger = true
		};
	}
}
