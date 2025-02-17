export module gse.core.registry;

import std;

export import gse.core.regisry.component;
export import gse.core.registry.entity;
export import gse.core.registry.entity_list;
export import gse.core.registry.hook;

import gse.platform.assert;

export namespace gse::registry {
	auto activate_entity(std::uint32_t identifier, const std::string& name) -> std::uint32_t;
	auto remove_entity(const std::string& name) -> std::uint32_t;
	auto remove_entity(std::uint32_t index) -> void;
}

auto gse::registry::activate_entity(std::uint32_t identifier, const std::string& name) -> std::uint32_t {
	const auto inactive_entities = get_inactive_entities();

	const auto it = std::ranges::find_if(inactive_entities, [identifier](const entity& entity) {
		return entity.index == identifier;
		});

	assert_comment(it != inactive_entities.end(), "Object not found in registry when trying to activate it");

	auto entity = *it;

	// First, we flush all the queued components to the final components.
	// This must be done before we update the object's index and generation
	// as the index at this point functions as a parent id for the components.
	for (const auto& val : get_queued_components() | std::views::values) {
		val->flush_to_final(entity.index);
	}

	std::uint32_t index;
	const std::uint32_t previous_id = entity.index;

	auto free_indices = get_free_indices();

	if (!free_indices.empty()) {
		index = free_indices.back();
		free_indices.pop_back();

		g_entities[index].generation++;

		entity.generation = g_entities[index].generation;
		entity.index = index;

		g_entities[index] = entity;
	}
	else {
		index = static_cast<std::uint32_t>(g_entities.size());
		entity.index = index;
		entity.generation = 0;
		g_entities.push_back(entity);
	}

	if (const auto it2 = g_string_to_index_map.find(name); it2 != g_string_to_index_map.end()) {
		const std::string new_name = name + std::to_string(index);
		g_string_to_index_map[new_name] = index;
	}
	else {
		g_string_to_index_map[name] = index;
	}

	for (const auto& hook : g_hooks) {
		if (hook->owner_id == previous_id) {
			hook->owner_id = index;
		}
	}

	for (const auto& hook : g_registered_inactive_hooks) {
		if (hook->owner_id == previous_id) {
			hook->owner_id = index;
		}
	}

	for (auto it3 = g_registered_inactive_hooks.begin(); it3 != g_registered_inactive_hooks.end(); ) {
		if ((*it3)->owner_id == index) {
			g_hooks.push_back(std::move(*it3));
			it3 = g_registered_inactive_hooks.erase(it3);
		}
		else {
			++it3;
		}
	}

	for (auto it4 = g_registered_inactive_entities.begin(); it4 != g_registered_inactive_entities.end(); ) {
		if (it4->index == index) {
			g_entities[index] = *it4;
			it4 = g_registered_inactive_entities.erase(it4);
		}
		else {
			++it4;
		}
	}

	return index;
}

auto gse::registry::remove_entity(const std::string& name) -> std::uint32_t {
	const auto it = g_string_to_index_map.find(name);
	if (it == g_string_to_index_map.end()) {
		assert_comment(false, "Object not found in registry when trying to remove it");
	}
	const auto index = it->second;

	g_string_to_index_map.erase(it);
	g_entities[index].generation++;
	g_free_indices.push_back(index);

	return index;
}

auto gse::registry::remove_entity(const std::uint32_t index) -> void {
	if (index < 0 || index >= static_cast<std::uint32_t>(g_entities.size())) {
		return;
	}

	std::erase_if(g_string_to_index_map, [index](const std::pair<std::string, std::uint32_t>& pair) {
		return pair.second == index;
		});

	std::erase_if(g_hooks, [index](const std::unique_ptr<hook<entity>>& hook) {
		return hook->owner_id == index;
		});

	g_entities[index].generation++;
	g_free_indices.push_back(index);
}