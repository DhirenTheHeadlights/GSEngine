#include "Graphics/2D/Renderer2D.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/ResourcePaths.h"
#include "Graphics/Shader.h"
#include "Graphics/3D/Renderer3D.h"

namespace {
	GLuint g_vao, g_vbo, g_ebo;
	gse::shader g_shader(ENGINE_RESOURCES_PATH "Shaders/ui_2d_shader.vert", ENGINE_RESOURCES_PATH "Shaders/ui_2d_shader.frag");
}

void gse::renderer::initialize2d() {
    struct vertex {
        glm::vec2 position;
        glm::vec2 texture_coordinate;
    };

    constexpr vertex vertices[4] = {
        {{0.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left
        {{1.0f, 0.0f}, {1.0f, 0.0f}}, // Top-right
        {{1.0f, 1.0f}, {1.0f, 1.0f}}, // Bottom-right
        {{0.0f, 1.0f}, {0.0f, 1.0f}}  // Bottom-left
    };

    const GLuint indices[6] = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glGenBuffers(1, &g_ebo);

    glBindVertexArray(g_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, texture_coordinate));

    glBindVertexArray(0);
}

void gse::renderer::begin_frame() {
	g_shader.use();
    g_shader.set_mat4("projection", get_camera().get_projection_matrix());
}

void gse::renderer::end_frame() {

}

namespace {
    void render_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4* color, const gse::texture* texture) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
        model = glm::scale(model, glm::vec3(size, 1.0f));
        g_shader.set_mat4("uModel", model);

        if (texture) {
            g_shader.set_int("uUseColor", 0);
            texture->bind();
        }
        else if (color) {
            g_shader.set_int("uUseColor", 1);
            //shader.setVec4("uColor", *color);
        }

        // Render the quad
        glBindVertexArray(g_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (texture) {
            texture->unbind();
        }
    }
}

void gse::renderer::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
	render_quad(position, size, &color, nullptr);
}

void gse::renderer::draw_quad(const glm::vec2& position, const glm::vec2& size, const gse::texture& texture) {
	render_quad(position, size, nullptr, &texture);
}

void gse::renderer::shutdown() {
    glDeleteVertexArrays(1, &g_vao);
    glDeleteBuffers(1, &g_vbo);
    glDeleteBuffers(1, &g_ebo);
}