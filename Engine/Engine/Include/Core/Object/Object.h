#pragma once

#include <typeindex>
#include <vector>

#include "Core/ID.h"
#include "Core/Object/Hook.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse {
	class scene;

	class object {
	public:
		explicit object(const std::string& name = "Unnamed Entity") : m_id(generate_id(name)) {}
		virtual ~object() = default;

		void set_scene_id(const std::shared_ptr<id>& scene_id) { this->m_scene_id = scene_id; }

		std::weak_ptr<id> get_id() const { return m_id; }
		std::weak_ptr<id> get_scene_id() const { return m_scene_id; }

		bool operator==(const object& other) const { return m_id == other.m_id; }

		template <typename component_type>
		void add_component(std::shared_ptr<component_type> component) { m_components.insert({typeid(component_type), component}); }

		template <typename component_type>
		std::shared_ptr<component_type> get_component() const {
			if (const auto it = m_components.find(typeid(component_type)); it != m_components.end()) {
				return std::static_pointer_cast<component_type>(it->second);
			}
			return nullptr;
		}

		void add_hook(std::unique_ptr<base_hook> hook) { m_hooks.push_back(std::move(hook)); }

	private:
		std::shared_ptr<id> m_scene_id;

		std::unordered_map<std::type_index, std::shared_ptr<void>> m_components;
		std::vector<std::unique_ptr<base_hook>> m_hooks;

		void initialize_hooks() const { for (const auto& hook : m_hooks) { hook->initialize(); } }

		void update_hooks() const { for (const auto& hook : m_hooks) { hook->update(); } }

		void render_hooks() const { for (const auto& hook : m_hooks) { hook->render(); } }

		void process_initialize() {
			initialize();
			initialize_hooks();
		}

		void process_update() {
			update();
			update_hooks();
		}

		void process_render() {
			render();
			render_hooks();
		}

		friend scene;

	protected:
		std::shared_ptr<id> m_id;

		virtual void initialize() {}
		virtual void update() {}
		virtual void render() {}
	};
}
