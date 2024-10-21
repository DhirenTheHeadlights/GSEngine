#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace Engine {
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoords;
	};

	class MeshComponent {
	public:
		MeshComponent() = default;
		MeshComponent(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
			: vertices(vertices), indices(indices) {}

		const std::vector<Vertex>& getVertices() const { return vertices; }
		const std::vector<unsigned int>& getIndices() const { return indices; }

	private:
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
	};
}
