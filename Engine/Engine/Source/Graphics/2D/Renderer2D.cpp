#include "Graphics/2D/Renderer2D.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/ResourcePaths.h"
#include "Graphics/Shader.h"
#include "Graphics/3D/Renderer3D.h"
#include "Platform/GLFW/Window.h"

namespace {
	GLuint g_vao, g_vbo, g_ebo;
    gse::shader g_shader;
	glm::mat4 g_projection;
}

void gse::renderer2d::initialize() {
    struct vertex {
        glm::vec2 position;
        glm::vec2 texture_coordinate;
    };

    // Define vertices for a unit quad (1x1), will be scaled via the model matrix
    constexpr vertex vertices[4] = {
        { .position = {0.0f, 1.0f}, .texture_coordinate = {0.0f, 1.0f}}, // Top-left
        { .position = {1.0f, 1.0f}, .texture_coordinate = {1.0f, 1.0f}}, // Top-right
        { .position = {1.0f, 0.0f}, .texture_coordinate = {1.0f, 0.0f}}, // Bottom-right
        { .position = {0.0f, 0.0f}, .texture_coordinate = {0.0f, 0.0f}}  // Bottom-left
    };

    const GLuint indices[6] = {
        0, 1, 2, // First Triangle
        2, 3, 0  // Second Triangle
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
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(vertex),
        reinterpret_cast<void*>(offsetof(vertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(vertex),
        reinterpret_cast<void*>(offsetof(vertex, texture_coordinate))
    );

    glBindVertexArray(0);

    g_shader.create_shader_program(
        ENGINE_RESOURCES_PATH "Shaders/2D/ui_2d_shader.vert",
        ENGINE_RESOURCES_PATH "Shaders/2D/ui_2d_shader.frag"
    );


    g_projection = glm::ortho(0.0f, static_cast<float>(window::get_window_size().x), 0.0f, static_cast<float>(window::get_window_size().y), -1.0f, 1.0f);
    g_shader.use();
    g_shader.set_mat4("projection", g_projection);
}

namespace {
    void render_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4* color, const gse::texture* texture) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        g_shader.use();
		g_shader.set_mat4("projection", g_projection);

        glm::mat4 model = translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
        model = scale(model, glm::vec3(size, 1.0f));
        g_shader.set_mat4("uModel", model);

        if (texture) {
            g_shader.set_int("uUseColor", 0);
            texture->bind();
        }
        else if (color) {
            g_shader.set_int("uUseColor", 1);
            g_shader.set_vec4("uColor", *color);
        }

        glBindVertexArray(g_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (texture) {
            texture->unbind();
        }

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
    }
}

void gse::renderer2d::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
	render_quad(position, size, &color, nullptr);
}

void gse::renderer2d::draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture) {
	render_quad(position, size, nullptr, &texture);
}

void gse::renderer2d::shutdown() {
    glDeleteVertexArrays(1, &g_vao);
    glDeleteBuffers(1, &g_vbo);
    glDeleteBuffers(1, &g_ebo);
}