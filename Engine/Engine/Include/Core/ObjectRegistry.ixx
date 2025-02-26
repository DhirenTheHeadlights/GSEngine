export module gse.core.object_registry;

import std;

import gse.core.id;
import gse.core.object.hook;
import gse.core.component;
import gse.physics.math;
import gse.core.clock;
import gse.platform.assert;

export namespace gse::registry {
	// Creates an object with a random TEMPORARY uuid
	// The uuid will be replaced with its index in the object list when it is activated
	auto create_entity() -> std::uint32_t;

	auto add_entity_hook(std::uint32_t parent_id, std::unique_ptr<hook<entity>> hook) -> void;
	auto remove_object_hook(const hook<entity>& hook) -> void;

	auto initialize_hooks() -> void;
	auto update_hooks() -> void;
	auto render_hooks() -> void;

	auto is_entity_id_in_list(id* list_id, std::uint32_t id) -> bool;

	auto does_entity_exist(const std::string& name) -> bool;
	auto does_entity_exist(std::uint32_t index, std::uint32_t generation) -> bool;
	auto does_entity_exist(std::uint32_t index) -> bool;

	template <typename T> requires std::derived_from<T, gse::component> auto add_component(T&& component) -> void;
	template <typename T> requires std::derived_from<T, gse::component> auto get_components() -> std::vector<T>&;
	template <typename T> requires std::derived_from<T, gse::component> auto get_component(std::uint32_t desired_id) -> T&;
	template <typename T> requires std::derived_from<T, gse::component> auto get_component_ptr(std::uint32_t desired_id) -> T*; // May return nullptr
	template <typename T> requires std::derived_from<T, gse::component> auto remove_component(const T& component) -> void;

	auto get_active_objects() -> std::vector<std::uint32_t>;
	auto get_entity_name(std::uint32_t index) -> std::string_view;
	auto get_entity_id(const std::string& name) -> std::uint32_t;
	auto get_number_of_objects() -> std::uint32_t;

	auto activate_entity(std::uint32_t identifier, const std::string& name) -> std::uint32_t;
	auto remove_entity(const std::string& name) -> std::uint32_t;
	auto remove_entity(std::uint32_t index) -> void;

	auto add_new_entity_list(id* list_id, const std::vector<std::uint32_t>& ids = {}) -> void;
	auto add_id_to_list(id* list_id, std::uint32_t id) -> void;
	auto remove_id_from_list(id* list_id, std::uint32_t id) -> void;

	auto periodically_clean_up_registry(const time& clean_up_interval = seconds(60.f)) -> void;
}

std::unordered_map<gse::id*, std::vector<std::uint32_t>> g_entity_lists;

std::unordered_map<std::string, std::uint32_t> g_string_to_index_map;
std::vector<gse::entity> g_entities;
std::vector<std::uint32_t> g_free_indices;
std::vector<gse::entity> g_registered_inactive_entities;

std::vector<std::unique_ptr<gse::hook<gse::entity>>> g_hooks;
std::vector<std::unique_ptr<gse::hook<gse::entity>>> g_registered_inactive_hooks;

struct component_container_base {
	virtual ~component_container_base() = default;

	virtual auto flush_to_final(std::uint32_t parent_id) -> void = 0;
};

std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_component_containers;
std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_queued_components;

gse::clock g_clean_up_clock;

auto generate_random_entity_placeholder_id() -> std::uint32_t {
	return gse::random_value(std::numeric_limits<std::uint32_t>::max());
}

export namespace gse {
	struct entity {
		std::uint32_t index = generate_random_entity_placeholder_id();
		std::uint32_t generation = 0;
	};
}

auto gse::registry::create_entity() -> std::uint32_t {
	const entity obj;
	g_registered_inactive_entities.push_back(obj);
	return obj.index; // Return temp uuid
}

auto gse::registry::add_entity_hook(const std::uint32_t parent_id, std::unique_ptr<hook<entity>> hook) -> void {
	const auto it = std::ranges::find_if(g_entities, [parent_id](const entity& obj) {
		return obj.index == parent_id;
		});
	if (it != g_entities.end()) {
		hook->owner_id = parent_id;
		g_hooks.push_back(std::move(hook));
		return;
	}

	const auto it2 = std::ranges::find_if(g_registered_inactive_entities, [parent_id](const entity& obj) {
		return obj.index == parent_id;
		});
	if (it2 != g_registered_inactive_entities.end()) {
		hook->owner_id = parent_id;
		g_registered_inactive_hooks.push_back(std::move(hook));
		return;
	}

	assert_comment(false, "Object not found in registry when trying to add a hook to it");
}

auto gse::registry::remove_object_hook(const hook<entity>& hook) -> void {
	/// TODO: Implement
}

auto gse::registry::initialize_hooks() -> void {
	for (const auto& hook : g_hooks) {
		hook->initialize();
	}
}

auto gse::registry::update_hooks() -> void {
	for (const auto& hook : g_hooks) {
		hook->update();
	}
}

auto gse::registry::render_hooks() -> void {
	for (const auto& hook : g_hooks) {
		hook->render();
	}
}

auto gse::registry::get_active_objects() -> std::vector<std::uint32_t> {
	std::vector<std::uint32_t> active_objects;
	active_objects.reserve(g_entities.size());
	for (const auto& [index, generation] : g_entities) {
		active_objects.push_back(index);
	}
	return active_objects;
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

auto gse::registry::get_number_of_objects() -> std::uint32_t {
	return static_cast<std::uint32_t>(g_entities.size());
}

auto gse::registry::activate_entity(std::uint32_t identifier, const std::string& name) -> std::uint32_t {
	const auto it = std::ranges::find_if(g_registered_inactive_entities, [identifier](const entity& obj) {
		return obj.index == identifier;
		});

	assert_comment(it != g_registered_inactive_entities.end(), "Object not found in registry when trying to activate it");

	auto object = *it;

	// First, we flush all the queued components to the final components.
	// This must be done before we update the object's index and generation
	// as the index at this point functions as a parent id for the components.
	for (const auto& val : g_queued_components | std::views::values) {
		val->flush_to_final(object.index);
	}

	std::uint32_t index;
	const std::uint32_t previous_id = object.index;
	if (!g_free_indices.empty()) {
		index = g_free_indices.back();
		g_free_indices.pop_back();

		g_entities[index].generation++;

		object.generation = g_entities[index].generation;
		object.index = index;

		g_entities[index] = object;
	}
	else {
		index = static_cast<std::uint32_t>(g_entities.size());
		object.index = index;
		object.generation = 0;
		g_entities.push_back(object);
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

auto gse::registry::is_entity_id_in_list(id* list_id, const std::uint32_t id) -> bool {
	return std::ranges::find(g_entity_lists[list_id], id) != g_entity_lists[list_id].end();
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

template <typename T>
	requires std::derived_from<T, gse::component>
auto add_component_to_container(T&& component, std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& component_container_list) -> void;

template <typename T>
	requires std::derived_from<T, gse::component>
struct component_container final : component_container_base {
	std::vector<T> components;

	auto flush_to_final(const std::uint32_t parent_id) -> void override {
		auto& final_map = g_component_containers;

		std::vector<T> to_move;

		std::erase_if(components, [&](T& c) {
			if (c.parent_id == parent_id) {
				to_move.push_back(std::move(c));
				return true;
			}
			return false;
			});

		for (auto& c : to_move) {
			add_component_to_container(std::move(c), final_map);
		}
	}
};

template <typename T>
	requires std::derived_from<T, gse::component>
auto add_component_to_container(T&& component, std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& component_container_list) -> void {  // NOLINT(cppcoreguidelines-missing-std-forward)
	if (const auto it = component_container_list.find(typeid(T)); it != component_container_list.end()) {
		auto& container = static_cast<component_container<T>&>(*it->second);
		container.components.push_back(std::move(component));   // NOLINT(bugprone-move-forwarding-reference)
	}
	else {
		auto container = std::make_unique<component_container<T>>();
		container->components.push_back(std::move(component));  // NOLINT(bugprone-move-forwarding-reference)
		component_container_list.insert({ typeid(T), std::move(container) });
	}
}

template <typename T> requires std::derived_from<T, gse::component>
auto gse::registry::add_component(T&& component) -> void {
	if (registry::does_entity_exist(component.parent_id)) {
		add_component_to_container(std::forward<T>(component), g_component_containers);  // NOLINT(bugprone-move-forwarding-reference)
	}
	else {
		add_component_to_container(std::forward<T>(component), g_queued_components);  // NOLINT(bugprone-move-forwarding-reference)
	}
}

template <typename T> requires std::derived_from<T, gse::component>
auto gse::registry::get_components() -> std::vector<T>& {
	if (const auto it = g_component_containers.find(typeid(T)); it != g_component_containers.end()) {
		auto& container = static_cast<component_container<T>&>(*it->second);
		return container.components;
	}
	static std::vector<T> empty_components;
	return empty_components;
}

template <typename T>
	requires std::derived_from<T, gse::component>
// ReSharper disable once CppNotAllPathsReturnValue - The assert_comment will always be triggered if the component is not found
auto internal_get_component(const std::uint32_t desired_id) -> T* {
	if (const auto it = g_component_containers.find(typeid(T)); it != g_component_containers.end()) {
		for (auto& container = static_cast<component_container<T>&>(*it->second); auto & comp : container.components) {  // Removed 'const'
			if (comp.parent_id == desired_id) {
				return &comp;
			}
		}
	}

	if (const auto it = g_queued_components.find(typeid(T)); it != g_queued_components.end()) {
		for (auto& container = static_cast<component_container<T>&>(*it->second); auto & comp : container.components) {
			if (comp.parent_id == desired_id) {
				return &comp;
			}
		}
	}

	return nullptr;
}

template <typename T> requires std::derived_from<T, gse::component>
auto gse::registry::get_component(const std::uint32_t desired_id) -> T& {
	if (auto* component = internal_get_component<T>(desired_id); component) {
		return *component;
	}

	throw std::runtime_error("Component not found in registry");
}

template <typename T> requires std::derived_from<T, gse::component>
auto gse::registry::get_component_ptr(const std::uint32_t desired_id) -> T* {
	return internal_get_component<T>(desired_id);
}

template <typename T> requires std::derived_from<T, gse::component>
auto gse::registry::remove_component(const T& component) -> void {
	if (const auto it = g_component_containers.find(typeid(T)); it != g_component_containers.end()) {
		auto& container = static_cast<component_container<T>&>(*it->second);
		std::erase_if(container.components, [&component](const T& required_component) {
			return required_component == component;
			});
	}
}

auto gse::registry::add_new_entity_list(id* list_id, const std::vector<std::uint32_t>& ids) -> void {
	g_entity_lists[list_id] = ids;
}

auto gse::registry::add_id_to_list(id* list_id, const std::uint32_t id) -> void {
	g_entity_lists[list_id].push_back(id);
}

auto gse::registry::remove_id_from_list(id* list_id, const std::uint32_t id) -> void {
	std::erase_if(g_entity_lists[list_id], [&](const std::uint32_t obj) {
		return obj == id;
		});
}

auto gse::registry::periodically_clean_up_registry(const time& clean_up_interval) -> void {
	if (g_clean_up_clock.get_elapsed_time() < clean_up_interval) {
		return;
	}

	/// TODO: This causes a crash sometimes - not sure how to fix just yet

	/*for (auto it = g_entity_lists.begin(); it != g_entity_lists.end();) {
		if (does_id_exist(it->first->number())) {
			it = g_entity_lists.erase(it);
		}
		else {
			++it;
		}
	}*/

	g_clean_up_clock.reset();
}