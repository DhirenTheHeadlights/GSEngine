#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h"
#include "Graphics/3D/Renderer3D.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisions.h"

namespace gse {
	class scene {
	public:
		scene() = default;
		scene(const std::shared_ptr<ID>& id) : id(id) {}

		void addObject(const std::weak_ptr<object>& object);
		void removeObject(const std::weak_ptr<object>& object);

		void initialize();
		void update();
		void render();
		void exit();

		void setId(const std::shared_ptr<ID>& id) { this->id = id; }
		std::shared_ptr<ID> getId() const { return id; }

		void setActive(const bool isActive) { this->isActive = isActive; }
		bool getActive() const { return isActive; }
	private:
		std::vector<std::weak_ptr<object>> objects;

		bool isActive = false;

		std::shared_ptr<ID> id;

		Physics::Group physicsSystem;
		BroadPhaseCollision::Group collisionGroup;
		renderer::Group renderGroup;
	};
}