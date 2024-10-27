#pragma once

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Engine {
    struct RenderQueueEntry {
		RenderQueueEntry(const std::string& shaderProgramType, const GLuint VAO, const GLenum drawMode, const GLsizei vertexCount, const glm::mat4& modelMatrix, const GLuint textureID, const glm::vec3& color)
			: shaderProgramKey(shaderProgramType), VAO(VAO), drawMode(drawMode), vertexCount(vertexCount), modelMatrix(modelMatrix), textureID(textureID), color(color) {}
        std::string shaderProgramKey;
        GLuint VAO;
        GLenum drawMode;
        GLsizei vertexCount;
        glm::mat4 modelMatrix;
        GLuint textureID;
        glm::vec3 color;
    };
}