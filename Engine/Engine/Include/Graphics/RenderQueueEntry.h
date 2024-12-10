#pragma once

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gse {
    struct RenderQueueEntry {
        std::string materialKey;
        GLuint VAO;
        GLenum drawMode;
        GLsizei vertexCount;
        glm::mat4 modelMatrix;
        GLuint textureID;
        glm::vec3 color;
    };
}