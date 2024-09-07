#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

namespace Game {
	class Arena {
	public:
		Arena(glm::vec3 size) : width(size.x), height(size.y), depth(size.z) {}

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