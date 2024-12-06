#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>

namespace Engine {
	class CubeMap {
	public:
		CubeMap() = default;
		~CubeMap();

		void create(const std::vector<std::string>& faces);
		void create(int resolution);
		void bind(GLuint unit) const;
		void update(const glm::vec3& position, const glm::mat4& projectionMatrix, const std::function<void(const glm::mat4&, const glm::mat4&)>& renderFunction) const;
	private:
		GLuint textureID;
		GLuint framebufferID;
		GLuint depthRenderbufferID;
		int resolution;

		static std::vector<glm::mat4> getViewMatrices(const glm::vec3& position);
	};
}
