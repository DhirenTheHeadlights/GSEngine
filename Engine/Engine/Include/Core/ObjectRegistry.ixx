export module gse.core.object_registry;

import std;

import gse.core.id;
import gse.core.object.hook;
import gse.core.component;
import gse.physics.math.units;

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

	struct component_container_base {
		virtual ~component_container_base() = default;

		virtual auto flush_to_final(std::uint32_t parent_id) -> void = 0;
	};

	namespace internal {
		std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_component_containers;
		std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_queued_components;

		template <typename T>
			requires std::derived_from<T, component>
		auto add_component(T&& component, std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& component_container_list) -> void;
	}

	auto get_component_containers() -> std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& {
		return internal::g_component_containers;
	}

	auto get_queued_components() -> std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& {
		return internal::g_queued_components;
	}

	template <typename T>
		requires std::derived_from<T, component>
	struct component_container final : component_container_base {
		std::vector<T> components;

		auto flush_to_final(const std::uint32_t parent_id) -> void override {
			auto& final_map = internal::g_component_containers;

			std::vector<T> to_move;

			std::erase_if(components, [&](T& c) {
				if (c.parent_id == parent_id) {
					to_move.push_back(std::move(c));
					return true;
				}
				return false;
				});

			for (auto& c : to_move) {
				internal::add_component(std::move(c), final_map);
			}
		}
	};

	namespace internal {
		template <typename T>
			requires std::derived_from<T, component>
		auto add_component(T&& component, std::unordered_map<std::type_index, std::unique_ptr<component_container_base>>& component_container_list) -> void {  // NOLINT(cppcoreguidelines-missing-std-forward)
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
	}

	template <typename T>
		requires std::derived_from<T, component>
	auto add_component(T&& component) -> void {  // NOLINT(cppcoreguidelines-missing-std-forward)
		if (does_entity_exist(component.parent_id)) {
			internal::add_component(std::forward<T>(component), internal::g_component_containers);  // NOLINT(bugprone-move-forwarding-reference)
		}
		else {
			internal::add_component(std::forward<T>(component), internal::g_queued_components);  // NOLINT(bugprone-move-forwarding-reference)
		}
	}

	template <typename T>
		requires std::derived_from<T, component>
	auto get_components() -> std::vector<T>& {
		if (const auto it = internal::g_component_containers.find(typeid(T)); it != internal::g_component_containers.end()) {
			auto& container = static_cast<component_container<T>&>(*it->second);
			return container.components;
		}
		static std::vector<T> empty_components;
		return empty_components;
	}

	namespace internal {
		template <typename T>
			requires std::derived_from<T, component>
		// ReSharper disable once CppNotAllPathsReturnValue - The assert_comment will always be triggered if the component is not found
		auto get_component(const std::uint32_t desired_id) -> T* {
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
	}

	template <typename T>
		requires std::derived_from<T, component>
	// ReSharper disable once CppNotAllPathsReturnValue - The assert_comment will always be triggered if the component is not found
	auto get_component(const std::uint32_t desired_id) -> T& {
		if (auto* component = internal::get_component<T>(desired_id); component) {
			return *component;
		}

		throw std::runtime_error("Component not found in registry");
	}

	template <typename T>
		requires std::derived_from<T, component>
	// Use this if unsure that a component exists for an object
	auto get_component_ptr(const std::uint32_t desired_id) -> T* {
		return internal::get_component<T>(desired_id);
	}

	template <typename T>
		requires std::derived_from<T, component>
	auto remove_component(const T& component) -> void {
		if (const auto it = internal::g_component_containers.find(typeid(T)); it != internal::g_component_containers.end()) {
			auto& container = static_cast<component_container<T>&>(*it->second);
			std::erase_if(container.components, [&component](const T& required_component) {
				return required_component == component;
				});
		}
	}

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
