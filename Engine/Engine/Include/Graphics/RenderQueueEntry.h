#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Engine {
    struct RenderQueueEntry {
		RenderQueueEntry(const GLuint shaderProgram, const GLuint VAO, const GLenum drawMode, const GLsizei vertexCount, const glm::mat4& modelMatrix, const GLuint textureID)
			: shaderProgram(shaderProgram), VAO(VAO), drawMode(drawMode), vertexCount(vertexCount), modelMatrix(modelMatrix), textureID(textureID) {}
        GLuint shaderProgram;
        GLuint VAO;
        GLenum drawMode;
        GLsizei vertexCount;
        glm::mat4 modelMatrix;
        GLuint textureID;
    };
}