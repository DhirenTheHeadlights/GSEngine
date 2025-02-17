export module gse.core.regisry.component;

import std;

import gse.core.component;
import gse.core.registry.entity;

struct component_container_base;

export namespace gse::registry {
	template <typename T> concept is_component = std::derived_from<T, component>;

	template <is_component T> auto add_component(T&& component) -> void;
	template <is_component T> auto get_components() -> std::vector<T>&;
	template <is_component T> auto get_component(std::uint32_t desired_id) -> T&;
	template <is_component T> auto get_component_ptr(std::uint32_t desired_id) -> T*; // May return nullptr
	template <is_component T> auto remove_component(const T& component) -> void;

	auto get_queued_components() -> std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>&;
}

struct component_container_base {
	virtual ~component_container_base() = default;

	virtual auto flush_to_final(std::uint32_t parent_id) -> void = 0;
};

std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_component_containers;
std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_queued_components;

template <gse::registry::is_component T>
	requires std::derived_from<T, gse::component>
auto add_component_to_container(T&& component, std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& component_container_list) -> void;

template <gse::registry::is_component T>
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
			c.parent_name = gse::registry::get_entity_name(parent_id);
			add_component_to_container(std::move(c), final_map);
		}
	}
};

template <gse::registry::is_component T>
	requires std::derived_from<T, gse::component>
auto add_component_to_container(T&& component, std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& component_container_list) -> void {  // NOLINT(cppcoreguidelines-missing-std-forward)
	component.parent_name = gse::registry::get_entity_name(component.parent_id);

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

template <gse::registry::is_component T>
auto gse::registry::add_component(T&& component) -> void {
	if (registry::does_entity_exist(component.parent_id)) {
		add_component_to_container(std::forward<T>(component), g_component_containers);  // NOLINT(bugprone-move-forwarding-reference)
	}
	else {
		add_component_to_container(std::forward<T>(component), g_queued_components);  // NOLINT(bugprone-move-forwarding-reference)
	}
}

template <gse::registry::is_component T>
auto gse::registry::get_components() -> std::vector<T>& {
	if (const auto it = g_component_containers.find(typeid(T)); it != g_component_containers.end()) {
		auto& container = static_cast<component_container<T>&>(*it->second);
		return container.components;
	}
	static std::vector<T> empty_components;
	return empty_components;
}

template <gse::registry::is_component T>
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

template <gse::registry::is_component T>
auto gse::registry::get_component(const std::uint32_t desired_id) -> T& {
	if (auto* component = internal_get_component<T>(desired_id); component) {
		return *component;
	}

	throw std::runtime_error("Component not found in registry");
}

template <gse::registry::is_component T>
auto gse::registry::get_component_ptr(const std::uint32_t desired_id) -> T* {
	return internal_get_component<T>(desired_id);
}

template <gse::registry::is_component T>
auto gse::registry::remove_component(const T& component) -> void {
	if (const auto it = g_component_containers.find(typeid(T)); it != g_component_containers.end()) {
		auto& container = static_cast<component_container<T>&>(*it->second);
		std::erase_if(container.components, [&component](const T& required_component) {
			return required_component == component;
			});
	}
}

auto gse::registry::get_queued_components() -> std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& {
	return g_queued_components;
}
