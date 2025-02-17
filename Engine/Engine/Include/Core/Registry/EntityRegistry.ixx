export module gse.core.registry.entity;

import std;

import gse.physics.math;

export namespace gse {
	struct entity;
}

export namespace gse::registry {
	// Creates an object with a random TEMPORARY uuid
	// The uuid will be replaced with its index in the object list when it is activated
	auto create_entity() -> std::uint32_t;

	auto does_entity_exist(const std::string& name) -> bool;
	auto does_entity_exist(std::uint32_t index, std::uint32_t generation) -> bool;
	auto does_entity_exist(std::uint32_t index) -> bool;

	auto get_active_entities() -> std::vector<std::uint32_t>;
	auto get_inactive_entities() -> std::vector<entity>&;
	auto get_free_indices() -> std::vector<std::uint32_t>&;
	auto get_entity_name(std::uint32_t index) -> std::string_view;
	auto get_entity_id(const std::string& name) -> std::uint32_t;
	auto get_number_of_entities() -> std::uint32_t;
}

export namespace gse {
	struct entity {
		std::uint32_t index = random_value(std::numeric_limits<std::uint32_t>::max());
		std::uint32_t generation = 0;
	};
}

std::unordered_map<std::string, std::uint32_t> g_string_to_index_map;
std::vector<gse::entity> g_entities;
std::vector<std::uint32_t> g_free_indices;
std::vector<gse::entity> g_registered_inactive_entities;

auto gse::registry::create_entity() -> std::uint32_t {
	const entity obj;
	g_registered_inactive_entities.push_back(obj);
	return obj.index; // Return temp uuid
}

auto gse::registry::does_entity_exist(const std::string& name) -> bool {
	return g_string_to_index_map.contains(name);
}

auto gse::registry::does_entity_exist(const std::uint32_t index, const std::uint32_t generation) -> bool {
	if (index >= static_cast<std::uint32_t>(g_entities.size())) {
		return false;
	}

	return g_entities[index].generation == generation;
}

auto gse::registry::does_entity_exist(const std::uint32_t index) -> bool {
	if (index >= static_cast<std::uint32_t>(g_entities.size())) {
		return false;
	}

	return true;
}

auto gse::registry::get_active_entities() -> std::vector<std::uint32_t> {
	std::vector<std::uint32_t> active_objects;
	active_objects.reserve(g_entities.size());
	for (const auto& [index, generation] : g_entities) {
		active_objects.push_back(index);
	}
	return active_objects;
}

auto gse::registry::get_inactive_entities() -> std::vector<entity>& {
	return g_registered_inactive_entities;
}

auto gse::registry::get_free_indices() -> std::vector<std::uint32_t>& {
	return g_free_indices;
}

auto gse::registry::get_entity_name(const std::uint32_t index) -> std::string_view {
	if (index < 0 || index >= static_cast<std::uint32_t>(g_entities.size())) {
		return "Object not found in registry when trying to get its name";
	}
	for (const auto& [name, idx] : g_string_to_index_map) {
		if (idx == index) {
			return name;
		}
	}
	return "Object not found in registry when trying to get its name";
}

auto gse::registry::get_entity_id(const std::string& name) -> std::uint32_t {
	const auto it = g_string_to_index_map.find(name);
	if (it == g_string_to_index_map.end()) {
		return -1;
	}
	return it->second;
}

auto gse::registry::get_number_of_entities() -> std::uint32_t {
	return static_cast<std::uint32_t>(g_entities.size());
}