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
		explicit object(const std::string& name = "Unnamed Entity") : id(generateID(name)) {}
		virtual ~object() = default;

		void set_scene_id(const std::shared_ptr<ID>& sceneId) { this->scene_id = sceneId; }

		std::weak_ptr<ID> getId() const { return id; }
		std::weak_ptr<ID> getSceneId() const { return scene_id; }

		bool operator==(const object& other) const { return id == other.id; }

		template <typename T>
		void addComponent(std::shared_ptr<T> component) { components.insert({typeid(T), component}); }

		template <typename T>
		std::shared_ptr<T> getComponent() const {
			if (const auto it = components.find(typeid(T)); it != components.end()) {
				return std::static_pointer_cast<T>(it->second);
			}
			return nullptr;
		}

		void add_hook(std::unique_ptr<base_hook> hook) { hooks.push_back(std::move(hook)); }

	private:
		std::shared_ptr<ID> scene_id;

		std::unordered_map<std::type_index, std::shared_ptr<void>> components;
		std::vector<std::unique_ptr<base_hook>> hooks;

		void initialize_hooks() const { for (const auto& hook : hooks) { hook->initialize(); } }

		void update_hooks() const { for (const auto& hook : hooks) { hook->update(); } }

		void render_hooks() const { for (const auto& hook : hooks) { hook->render(); } }

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
		std::shared_ptr<ID> id;

		virtual void initialize() {}
		virtual void update() {}
		virtual void render() {}
	};
}
