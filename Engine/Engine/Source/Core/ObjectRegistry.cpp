#include "Core/ObjectRegistry.h"

#include <ranges>

#include "Core/Clock.h"
#include "Platform/PermaAssert.h"

namespace {
	std::unordered_map<gse::id*, std::vector<gse::object*>> g_object_lists;
	std::unordered_map<gse::id*, std::vector<std::uint32_t>> g_id_lists;

	std::unordered_map<std::string, std::uint32_t> g_string_to_index_map;
	std::vector<gse::object> g_objects;
	std::vector<std::uint32_t> g_free_indices;
	std::vector<gse::object> g_registered_inactive_objects;

	std::vector<gse::hook<gse::object>> g_hooks;
	std::vector<gse::hook<gse::object>> g_registered_inactive_hooks;

	gse::clock g_clean_up_clock;
}

auto gse::registry::create_object() -> object* {
	const object obj;
	g_registered_inactive_objects.push_back(obj);
	return &g_registered_inactive_objects.back();
}

auto gse::registry::add_object_hook(const std::uint32_t parent_id, hook<object>&& hook) -> void {
	const auto it = std::ranges::find_if(g_objects, [parent_id](const object& obj) {
		return obj.index == parent_id;
		});
	if (it != g_objects.end()) {
		hook.owner_id = parent_id;
		g_hooks.push_back(std::move(hook));
		return;
	}

	const auto it2 = std::ranges::find_if(g_registered_inactive_objects, [parent_id](const object& obj) {
		return obj.index == parent_id;
		});
	if (it2 != g_registered_inactive_objects.end()) {
		hook.owner_id = parent_id;
		g_registered_inactive_hooks.push_back(std::move(hook));
		return;
	}

	assert_comment(false, "Object not found in registry when trying to add a hook to it");
}

auto gse::registry::remove_object_hook(const hook<object>& hook) -> void {
	/// TODO: Implement
}

auto gse::registry::initialize_hooks() -> void {
	for (auto& hook : g_hooks) {
		hook.initialize();
	}
}

auto gse::registry::update_hooks() -> void {
	for (auto& hook : g_hooks) {
		hook.update();
	}
}

auto gse::registry::render_hooks() -> void {
	for (auto& hook : g_hooks) {
		hook.render();
	}
}

auto gse::registry::get_active_objects() -> std::vector<object>& {
	return g_objects;
}

auto gse::registry::get_object(const std::string& name) -> object* {
	const auto it = g_string_to_index_map.find(name);
	if (it == g_string_to_index_map.end()) {
		return nullptr;
	}
	return &g_objects[it->second];
}

auto gse::registry::get_object(const std::uint32_t index) -> object* {
	if (index < 0 || index >= static_cast<std::uint32_t>(g_objects.size())) {
		return nullptr;
	}
	return &g_objects[index];
}

auto gse::registry::get_object_name(const std::uint32_t index) -> std::string_view {
	if (index < 0 || index >= static_cast<std::uint32_t>(g_objects.size())) {
		return "Object not found in registry when trying to get its name";
	}
	for (const auto& [name, idx] : g_string_to_index_map) {
		if (idx == index) {
			return name;
		}
	}
	return "Object not found in registry when trying to get its name";
}

auto gse::registry::get_object_id(const std::string& name) -> std::uint32_t {
	const auto it = g_string_to_index_map.find(name);
	if (it == g_string_to_index_map.end()) {
		return -1;
	}
	return it->second;
}

auto gse::registry::get_number_of_objects() -> std::uint32_t {
	return static_cast<std::uint32_t>(g_objects.size());
}

auto gse::registry::add_object(object* object_ptr, const std::string& name) -> std::uint32_t {
	auto& object = *object_ptr;

	// First, we flush all the queued components to the final components.
	// This must be done before we update the object's index and generation
	// as the index at this point functions as a parent id for the components.
	for (const auto& val : internal::g_queued_components | std::views::values) {
		val->flush_to_final(object.index);
	}

	std::uint32_t index;
	const std::uint32_t previous_id = object.index;
	if (!g_free_indices.empty()) {
		index = g_free_indices.back();
		g_free_indices.pop_back();

		g_objects[index].generation++;

		object.generation = g_objects[index].generation;
		object.index = index;

		g_objects[index] = object;
	}
	else {
		index = static_cast<std::uint32_t>(g_objects.size());
		object.index = index;
		object.generation = 0;
		g_objects.push_back(object);
	}

	if (const auto it = g_string_to_index_map.find(name); it != g_string_to_index_map.end()) {
		const std::string new_name = name + std::to_string(index);
		g_string_to_index_map[new_name] = index;
	}
	else {
		g_string_to_index_map[name] = index;
	}

	for (auto& hook : g_hooks) {
		if (hook.owner_id == previous_id) {
			hook.owner_id = index;
		}
	}

	for (auto& hook : g_registered_inactive_hooks) {
		if (hook.owner_id == previous_id) {
			hook.owner_id = index;
		}
	}

	std::erase_if(g_registered_inactive_hooks, [index](const hook<struct object>& hook) {
		if (hook.owner_id == index) {
			g_hooks.push_back(hook);
			return true;
		}
		return false;
		});

	std::erase_if(g_registered_inactive_objects, [index](const struct object& obj) {
		return obj.index == index;
		});

	return index;
}

auto gse::registry::remove_object(const std::string& name) -> std::uint32_t {
	const auto it = g_string_to_index_map.find(name);
	if (it == g_string_to_index_map.end()) {
		assert_comment(false, "Object not found in registry when trying to remove it");
	}
	const auto index = it->second;

	g_string_to_index_map.erase(it);
	g_objects[index].generation++;
	g_free_indices.push_back(index);

	return index;
}

auto gse::registry::remove_object(const std::uint32_t index) -> void {
	if (index < 0 || index >= static_cast<std::uint32_t>(g_objects.size())) {
		return;
	}

	std::erase_if(g_string_to_index_map, [index](const std::pair<std::string, std::uint32_t>& pair) {
		return pair.second == index;
		});

	g_objects[index].generation++;
	g_free_indices.push_back(index);
}

auto gse::registry::is_object_in_list(id* list_id, object* object) -> bool {
	return std::ranges::find(g_object_lists[list_id], object) != g_object_lists[list_id].end();
}

auto gse::registry::is_object_id_in_list(id* list_id, const std::uint32_t id) -> bool {
	return std::ranges::find(g_id_lists[list_id], id) != g_id_lists[list_id].end();
}

auto gse::registry::does_object_exist(const std::string& name) -> bool {
	return g_string_to_index_map.contains(name);
}

auto gse::registry::does_object_exist(const std::uint32_t index, const std::uint32_t generation) -> bool {
	if (index >= static_cast<std::uint32_t>(g_objects.size())) {
		return false;
	}

	return g_objects[index].generation == generation;
}

auto gse::registry::does_object_exist(const std::uint32_t index) -> bool {
	if (index >= static_cast<std::uint32_t>(g_objects.size())) {
		return false;
	}

	return true;
}

auto gse::registry::add_new_object_list(id* list_id, const std::vector<object*>& objects) -> void {
	g_object_lists[list_id] = objects;
}

auto gse::registry::add_new_id_list(id* list_id, const std::vector<std::uint32_t>& ids) -> void {
	g_id_lists[list_id] = ids;
}

auto gse::registry::add_object_to_list(id* list_id, object* object) -> void {
	g_object_lists[list_id].push_back(object);
}

auto gse::registry::add_id_to_list(id* list_id, const std::uint32_t id) -> void {
	g_id_lists[list_id].push_back(id);
}

auto gse::registry::remove_object_from_list(id* list_id, const object* object) -> void {
	std::erase_if(g_object_lists[list_id], [&](const gse::object* obj) {
		return obj == object;
		});
}

auto gse::registry::remove_id_from_list(id* list_id, const std::uint32_t id) -> void {
	std::erase_if(g_id_lists[list_id], [&](const std::uint32_t obj) {
		return obj == id;
		});
}

auto gse::registry::periodically_clean_up_stale_lists(const time& clean_up_interval) -> void {
	if (g_clean_up_clock.get_elapsed_time() < clean_up_interval) {
		return;
	}

	for (auto it = g_object_lists.begin(); it != g_object_lists.end();) {
		if (does_id_exist(it->first->number())) {
			it = g_object_lists.erase(it);
		}
		else {
			++it;
		}
	}

	for (auto it = g_id_lists.begin(); it != g_id_lists.end();) {
		if (does_id_exist(it->first->number())) {
			it = g_id_lists.erase(it);
		}
		else {
			++it;
		}
	}

	g_clean_up_clock.reset();
}