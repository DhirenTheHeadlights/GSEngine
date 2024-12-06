#include "Graphics/3D/BoundingBoxMesh.h"

Engine::BoundingBoxMesh::BoundingBoxMesh(const Vec3<Length>& lower, const Vec3<Length>& upper)
	: lower(lower), upper(upper) {
	updateGrid();
	drawMode = GL_LINES;
	shaderName = "SolidColor";
}

Engine::Vertex Engine::BoundingBoxMesh::createVertex(const glm::vec3& position) {
	return {
		position,
		glm::vec3(0.0f),
		glm::vec2(0.0f)
	};
}

void Engine::BoundingBoxMesh::updateGrid() {
	constexpr float cellSize = 10.0f;

	vertices.clear();

	const glm::vec3 min = lower.as<Meters>();
	const glm::vec3 max = upper.as<Meters>();

	// Generate grid lines for each face
	for (float y = min.y; y <= max.y; y += cellSize) {
		for (float x = min.x; x <= max.x; x += cellSize) {
			// Vertical lines (front and back faces)
			vertices.push_back(createVertex({ x, y, min.z }));
			vertices.push_back(createVertex({ x, y, max.z }));

			// Horizontal lines along x-axis (front and back faces)
			if (x + cellSize <= max.x) {
				float xEnd = x + cellSize;
				vertices.push_back(createVertex({ x, y, min.z }));
				vertices.push_back(createVertex({ xEnd, y, min.z }));

				vertices.push_back(createVertex({ x, y, max.z }));
				vertices.push_back(createVertex({ xEnd, y, max.z }));
			}

			// Horizontal lines along z-axis on left and right faces
			if (std::abs(x - max.x) < 1e-5 || std::abs(x - min.x) < 1e-5) {
				for (float z = min.z; z + cellSize <= max.z; z += cellSize) {
					float zEnd = z + cellSize;
					vertices.push_back(createVertex({ x, y, z }));
					vertices.push_back(createVertex({ x, y, zEnd }));
				}
			}

			// Vertical lines along y-axis on left and right faces
			if (y + cellSize <= max.y) {
				float yEnd = y + cellSize;
				for (float z = min.z; z <= max.z; z += cellSize) {
					vertices.push_back(createVertex({ x, y, z }));
					vertices.push_back(createVertex({ x, yEnd, z }));
				}
			}
		}
	}

	// Top and bottom faces grid lines
	for (float z = min.z; z <= max.z; z += cellSize) {
		for (float x = min.x; x <= max.x; x += cellSize) {
			if (x + cellSize <= max.x) {
				float xEnd = x + cellSize;
				vertices.push_back(createVertex({ x, max.y, z }));
				vertices.push_back(createVertex({ xEnd, max.y, z }));

				vertices.push_back(createVertex({ x, min.y, z }));
				vertices.push_back(createVertex({ xEnd, min.y, z }));
			}

			if (z + cellSize <= max.z) {
				float zEnd = z + cellSize;
				vertices.push_back(createVertex({ x, max.y, z }));
				vertices.push_back(createVertex({ x, max.y, zEnd }));

				vertices.push_back(createVertex({ x, min.y, z }));
				vertices.push_back(createVertex({ x, min.y, zEnd }));
			}
		}
	}
}

void Engine::BoundingBoxMesh::update() {
	updateGrid();
	setUpMesh();
}
