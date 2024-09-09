#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Engine.h"
#include "GameObject.h"

namespace Game {
	class Arena : public GameObject {
	public:
		Arena(const glm::vec3 size) : GameObject(Engine::idManager.generateID()), width(size.x), height(size.y), depth(size.z) {}

		void initialize();
		void render(const glm::mat4& view, const glm::mat4& projection) const;
	private:
		float width, height, depth; 

		// Vertex data for gridlines and walls
		std::vector<float> gridVertices;
		unsigned int gridVAO = 0, gridVBO = 0;

		// Generates gridlines
		void generateGridLines();

		// OpenGL setup for rendering
		void setupBuffers();
	};
}