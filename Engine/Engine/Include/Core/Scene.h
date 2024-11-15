#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h"
#include "Graphics/Renderer.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisionHandler.h"

namespace Engine {
	class Scene {
	public:
		Scene() = default;
		Scene(const std::shared_ptr<ID>& id) : id(id) {}

		void addObject(const std::weak_ptr<Object>& object);
		void removeObject(const std::weak_ptr<Object>& object);

		void initialize(std::optional<GLuint> fbo);
		void update();
		void render();
		void exit();

		void setId(const std::shared_ptr<ID>& id) { this->id = id; }
		std::shared_ptr<ID> getId() const { return id; }

		void setActive(const bool isActive) { this->isActive = isActive; }
		bool getActive() const { return isActive; }
	private:
		std::vector<std::weak_ptr<Object>> objects;

		bool isActive = false;

		std::shared_ptr<ID> id;

		Physics::System physicsSystem;
		BroadPhaseCollisionHandler broadPhaseCollisionHandler;
		Renderer renderer;
	};
}