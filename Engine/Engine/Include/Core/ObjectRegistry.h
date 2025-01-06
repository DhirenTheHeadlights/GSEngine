#pragma once

#include <Core/ID.h>
#include "Object/Object.h"

namespace gse::registry {
	bool is_object_in_list(const std::weak_ptr<id>& list_id, const object* object);
	bool is_id_in_list(const std::weak_ptr<id>& list_id, const id* id);

	void register_object(object* new_object);
	void unregister_object(const object* object_to_remove);

	struct component_container_base {
		virtual ~component_container_base() = default;
	};

	template <typename T>
		requires std::derived_from<T, gse::component>
	struct component_container final : component_container_base {
		std::vector<T> components;
	};

	namespace internal {
		inline std::unordered_map<std::type_index, std::unique_ptr<component_container_base>> g_component_containers;
	}

	template <typename T>
		requires std::derived_from<T, component>
	void add_component(T&& component) {  // NOLINT(cppcoreguidelines-missing-std-forward)
		if (const auto it = internal::g_component_containers.find(typeid(T)); it != internal::g_component_containers.end()) {
			auto& container = static_cast<component_container<T>&>(*it->second);
			container.components.push_back(std::move(component));  // NOLINT(bugprone-move-forwarding-reference)
		}
		else {
			auto container = std::make_unique<component_container<T>>();
			container->components.push_back(std::move(component));
			internal::g_component_containers.insert({ typeid(T), std::move(container) });
		}
	}

	template <typename T>
		requires std::derived_from<T, component>
	std::vector<T>& get_components() {
		if (const auto it = internal::g_component_containers.find(typeid(T)); it != internal::g_component_containers.end()) {
			auto& container = static_cast<component_container<T>&>(*it->second);
			return container.components;
		}
		static std::vector<T> empty_components;
		return empty_components;
	}

	template <typename T>
		requires std::derived_from<T, component>
	T& get_component(const id* desired_id) {
		if (const auto it = internal::g_component_containers.find(typeid(T)); it != internal::g_component_containers.end()) {
			for (auto& container = static_cast<component_container<T>&>(*it->second); auto& comp : container.components) {  // Removed 'const'
				if (comp.parent_id == desired_id) {
					return comp;
				}
			}
		}
		throw std::runtime_error("No component of this type found for the given id.");
	}

	template <typename T>
		requires std::derived_from<T, component>
	void remove_component(const T& component) {
		if (const auto it = internal::g_component_containers.find(typeid(T)); it != internal::g_component_containers.end()) {
			auto& container = static_cast<component_container<T>&>(*it->second);
			std::erase_if(container.components, [&component](const T& required_component) {
				return required_component == component;
				});
		}
	}

	void add_new_object_list(const std::weak_ptr<id>& list_id, const std::vector<object*>& objects = {});
	void add_new_id_list(const std::weak_ptr<id>& list_id, const std::vector<id*>& ids = {});

	void add_object_to_list(const std::weak_ptr<id>& list_id, object* object);
	void add_id_to_list(const std::weak_ptr<id>& list_id, id* id);

	void remove_object_from_list(const std::weak_ptr<id>& list_id, const object* object);
	void remove_id_from_list(const std::weak_ptr<id>& list_id, const id* id);

	void periodically_clean_up_stale_lists(const time& clean_up_interval = seconds(60.f));
}