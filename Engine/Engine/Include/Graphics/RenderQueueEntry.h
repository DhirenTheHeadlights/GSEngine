#pragma once

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gse {
    struct render_queue_entry {
        std::string material_key;
        GLuint vao;
        GLenum draw_mode;
        GLsizei vertex_count;
        glm::mat4 model_matrix;
        GLuint texture_id;
        glm::vec3 color;
    };
}