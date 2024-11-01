#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisionHandler.h"
#include "Graphics/Renderer.h"

namespace Engine {
	class Scene {
	public:
		Scene(const std::shared_ptr<ID>& id) : id(id) {}

		void addObject(const std::weak_ptr<Object>& object);
		void removeObject(const std::weak_ptr<Object>& object);

		void initialize();
		void run();
		void exit();
	private:
		void update();
		void render();

		std::vector<std::weak_ptr<Object>> objects;

		bool isActive = false;

		std::shared_ptr<ID> id;

		std::unique_ptr<Physics::System> physicsSystem = std::make_unique<Physics::System>();
		std::unique_ptr<BroadPhaseCollisionHandler> broadPhaseCollisionHandler = std::make_unique<BroadPhaseCollisionHandler>();
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
	};
}