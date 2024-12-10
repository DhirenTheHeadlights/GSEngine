#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>

namespace gse {
	class CubeMap {
	public:
		CubeMap() = default;
		~CubeMap();

		void create(const std::vector<std::string>& faces);
		void create(int resolution, bool depthOnly = false);
		void bind(GLuint unit) const;
		void update(const glm::vec3& position, const glm::mat4& projectionMatrix, const std::function<void(const glm::mat4&, const glm::mat4&)>& renderFunction) const;

		GLuint getTextureID() const { return textureID; }
		GLuint getFrameBufferID() const { return frameBufferID; }
	private:
		GLuint textureID;
		GLuint frameBufferID;
		GLuint depthRenderBufferID;
		int resolution;

		static std::vector<glm::mat4> getViewMatrices(const glm::vec3& position);
	};
}
