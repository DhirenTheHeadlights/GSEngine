#pragma once

#include <typeindex>
#include <vector>

#include "Core/ID.h"
#include "Graphics/BoundingBox.h"
#include "Core/Object/Hook.h"

namespace Engine {
	class Scene;
	class Object {
	public:
		explicit Object(const std::string& name = "Unnamed Entity") : id(generateID(name)) {}
		virtual ~Object() = default;

		void setSceneId(const std::shared_ptr<ID>& sceneId) { this->sceneId = sceneId; }

		std::weak_ptr<ID> getId() const { return id; }
		std::weak_ptr<ID> getSceneId() const { return sceneId; }

		bool operator==(const Object& other) const {
			return id == other.id;
		}

		template <typename T>
		void addComponent(std::shared_ptr<T> component) {
			components.insert({ typeid(T), component });
		}

		template<typename T>
		std::shared_ptr<T> getComponent() const {
			if (const auto it = components.find(typeid(T)); it != components.end()) {
				return std::static_pointer_cast<T>(it->second);
			}
			return nullptr;
		}

		void addHook(std::unique_ptr<BaseHook> hook) {
			hooks.push_back(std::move(hook));
		}
	private:
		std::shared_ptr<ID> sceneId;

		std::unordered_map<std::type_index, std::shared_ptr<void>> components;
		std::vector<std::unique_ptr<BaseHook>> hooks;

		void initializeHooks() const {
			for (const auto& hook : hooks) {
				hook->initialize();
			}
		}

		void updateHooks() const {
			for (const auto& hook : hooks) {
				hook->update();
			}
		}

		void renderHooks() const {
			for (const auto& hook : hooks) {
				hook->render();
			}
		}

		void processInitialize() {
			initialize();
			initializeHooks();
		}

		void processUpdate() {
			update();
			updateHooks();
		}

		void processRender() {
			render();
			renderHooks();
		}

		friend Scene;
	protected:
		std::shared_ptr<ID> id;

		virtual void initialize() {}
		virtual void update() {}
		virtual void render() {}
	};
}