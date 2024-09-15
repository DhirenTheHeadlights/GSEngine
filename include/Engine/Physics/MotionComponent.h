#pragma once

#include <glm/glm.hpp>

namespace Engine::Physics {
	struct MotionComponent {
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };		// Initial position (can be customized)
		glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };		// Initial velocity (can be customized)
		glm::vec3 acceleration = { 0.0f, 0.0f, 0.0f };	// Stores acceleration (gravity will affect this)
		float mass = 1.0f;										// Mass of the object (for future forces)
		float speed = 1.0f;										// Speed of the object (for future forces)
		bool affectedByGravity = true;							// Toggle for whether gravity affects this entity
		bool airborne = false; 									// Toggle for whether the entity is in the air
		bool grounded = false;									// Toggle for whether the entity is grounded
	};
}
