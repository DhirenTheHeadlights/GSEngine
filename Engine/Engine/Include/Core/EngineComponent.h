#pragma once

#include <typeindex>

#include "ID.h"

namespace gse {
	class component {
	public:
		component(id* id) : m_id(id) {}
		virtual ~component() = default;

		id* get_id() const { return m_id; }
	private:
		id* m_id;
	};

	class composable {
	public:
		template <typename component_type>
		void add_component(std::shared_ptr<component_type> component) { m_components.insert({ typeid(component_type), component }); }

		template <typename component_type>
		std::shared_ptr<component_type> get_component() const {
			if (const auto it = m_components.find(typeid(component_type)); it != m_components.end()) {
				return std::static_pointer_cast<component_type>(it->second);
			}
			return nullptr;
		}
	private:
		std::unordered_map<std::type_index, std::shared_ptr<void>> m_components;
	};
}