#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "GameObject.h"

#include "Engine/Physics/CollisionHandler.h"

namespace Game {
	class Arena : public GameObject {
	public:
		Arena(const glm::vec3 size) : width(size.x), height(size.y), depth(size.z) {}

		void initialize();
		void render(const glm::mat4& view, const glm::mat4& projection) const;

		bool isColliding() const override { return false; }
		void setIsColliding(bool isColliding) override {}
		std::vector<Engine::BoundingBox>& getBoundingBoxes() override { return walls; }
	private:
		float width, height, depth; 
		std::vector<Engine::BoundingBox> walls;

		// Vertex data for gridlines and walls
		std::vector<float> gridVertices;
		unsigned int gridVAO = 0, gridVBO = 0;

		// Generates gridlines
		void generateGridLines();

		// OpenGL setup for rendering
		void setupBuffers();
	};
}