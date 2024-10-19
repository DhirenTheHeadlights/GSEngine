#include "Engine/Graphics/BBRenderComponent.h"

void Engine::BoundingBoxRenderComponent::initializeGrid(const Vec3<Length>& lower, const Vec3<Length>& upper) {
	constexpr float cellSize = 10.0f;

	vertices.clear();

	const glm::vec3 min = lower.as<Units::Meters>();
	const glm::vec3 max = upper.as<Units::Meters>();

	// Generate grid lines for each face
	for (float y = min.y; y <= max.y; y += cellSize) {
		for (float x = min.x; x <= max.x; x += cellSize) {
			// Vertical lines (front and back faces)
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(min.z);
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(max.z);

			// Horizontal lines along x-axis (front and back faces)
			if (x + cellSize <= max.x) {
				float xEnd = x + cellSize;
				// Front face
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(min.z);
				vertices.push_back(xEnd);
				vertices.push_back(y);
				vertices.push_back(min.z);

				// Back face
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(max.z);
				vertices.push_back(xEnd);
				vertices.push_back(y);
				vertices.push_back(max.z);
			}

			// Horizontal lines along z-axis on left and right faces
			if (std::abs(x - max.x) < 1e-5 || std::abs(x - min.x) < 1e-5) {
				for (float z = min.z; z + cellSize <= max.z; z += cellSize) {
					float zEnd = z + cellSize;
					// Left or Right face at x
					vertices.push_back(x);
					vertices.push_back(y);
					vertices.push_back(z);
					vertices.push_back(x);
					vertices.push_back(y);
					vertices.push_back(zEnd);
				}
			}

			// Vertical lines along y-axis on left and right faces
			if (y + cellSize <= max.y) {
				float yEnd = y + cellSize;
				for (float z = min.z; z <= max.z; z += cellSize) {
					// Left face at x = min.x
					vertices.push_back(x);
					vertices.push_back(y);
					vertices.push_back(z);
					vertices.push_back(x);
					vertices.push_back(yEnd);
					vertices.push_back(z);

					// Right face at x = max.x
					vertices.push_back(x);
					vertices.push_back(y);
					vertices.push_back(z);
					vertices.push_back(x);
					vertices.push_back(yEnd);
					vertices.push_back(z);
				}
			}
		}
	}

	// Top and bottom faces grid lines
	for (float z = min.z; z <= max.z; z += cellSize) {
		for (float x = min.x; x <= max.x; x += cellSize) {
			if (x + cellSize <= max.x) {
				float xEnd = x + cellSize;
				// Lines along x-axis on top and bottom faces
				// Top face at y = max.y
				vertices.push_back(x);
				vertices.push_back(max.y);
				vertices.push_back(z);
				vertices.push_back(xEnd);
				vertices.push_back(max.y);
				vertices.push_back(z);

				// Bottom face at y = min.y
				vertices.push_back(x);
				vertices.push_back(min.y);
				vertices.push_back(z);
				vertices.push_back(xEnd);
				vertices.push_back(min.y);
				vertices.push_back(z);
			}

			if (z + cellSize <= max.z) {
				float zEnd = z + cellSize;
				// Lines along z-axis on top and bottom faces
				// Top face at y = max.y
				vertices.push_back(x);
				vertices.push_back(max.y);
				vertices.push_back(z);
				vertices.push_back(x);
				vertices.push_back(max.y);
				vertices.push_back(zEnd);

				// Bottom face at y = min.y
				vertices.push_back(x);
				vertices.push_back(min.y);
				vertices.push_back(z);
				vertices.push_back(x);
				vertices.push_back(min.y);
				vertices.push_back(zEnd);
			}
		}
	}
}

void Engine::BoundingBoxRenderComponent::updateGrid(const Vec3<Length>& lower, const Vec3<Length>& upper, const bool isMoving) {
	if (isMoving || !isInitialized) {
		initializeGrid(lower, upper);

		if (VAO == 0) {
			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);
		}

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), static_cast<void*>(nullptr));
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		isInitialized = true;
	}
}

void Engine::BoundingBoxRenderComponent::render() const {
	if (!isInitialized) {
		return;
	}

	glBindVertexArray(VAO);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 3));
	glBindVertexArray(0);
}
