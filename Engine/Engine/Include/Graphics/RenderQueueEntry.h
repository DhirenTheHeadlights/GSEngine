#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Engine {
    struct RenderQueueEntry {
		RenderQueueEntry(GLuint shaderProgram, GLuint VAO, GLenum drawMode, GLsizei vertexCount, const glm::mat4& modelMatrix, GLuint textureID)
			: shaderProgram(shaderProgram), VAO(VAO), drawMode(drawMode), vertexCount(vertexCount), modelMatrix(modelMatrix), textureID(textureID) {}
        GLuint shaderProgram;
        GLuint VAO;
        GLenum drawMode;
        GLsizei vertexCount;
        glm::mat4 modelMatrix;
        GLuint textureID;
    };
}