#pragma once

#include <glm/glm.hpp>

namespace Engine {
	namespace Physics {
		struct Component {
			glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };		// Initial velocity (can be customized)
			glm::vec3 acceleration = { 0.0f, 0.0f, 0.0f };  // Stores acceleration (gravity will affect this)
			float mass = 1.0f;								// Mass of the object (for future forces)
			bool affectedByGravity = true;					// Toggle for whether gravity affects this entity
		};
	}
}