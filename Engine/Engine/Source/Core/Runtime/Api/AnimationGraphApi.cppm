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
