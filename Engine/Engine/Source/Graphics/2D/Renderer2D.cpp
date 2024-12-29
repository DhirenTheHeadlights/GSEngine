#include "Graphics/2D/Renderer2D.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/ResourcePaths.h"
#include "Graphics/Shader.h"
#include "Graphics/3D/Renderer3D.h"
#include "Platform/GLFW/Window.h"
#include "tests/caveview/glext.h"

namespace {
	GLuint g_vao, g_vbo, g_ebo;
    gse::shader g_shader;
	gse::shader g_msdf_shader;
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

	g_msdf_shader.create_shader_program(
		ENGINE_RESOURCES_PATH "Shaders/2D/msdf_shader.vert",
		ENGINE_RESOURCES_PATH "Shaders/2D/msdf_shader.frag"
	);

	g_msdf_shader.use();
	g_msdf_shader.set_mat4("projection", g_projection);
}

void gse::renderer2d::shutdown() {
    glDeleteVertexArrays(1, &g_vao);
    glDeleteBuffers(1, &g_vbo);
    glDeleteBuffers(1, &g_ebo);
}

namespace {
    void render_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4* color, const gse::texture* texture, const glm::vec4& uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)) {
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

        struct vertex {
            glm::vec2 position;
            glm::vec2 texture_coordinate;
        };

        const glm::vec2 uv0 = { uv_rect.x, uv_rect.y + uv_rect.w };             // Top-left
        const glm::vec2 uv1 = { uv_rect.x + uv_rect.z, uv_rect.y + uv_rect.w }; // Top-right
        const glm::vec2 uv2 = { uv_rect.x + uv_rect.z, uv_rect.y };             // Bottom-right
        const glm::vec2 uv3 = { uv_rect.x, uv_rect.y };                         // Bottom-left

        const vertex vertices[4] = {
            { .position= {0.0f, 1.0f}, .texture_coordinate= uv0},
	        { .position= {1.0f, 1.0f}, .texture_coordinate= uv1},
	        { .position= {1.0f, 0.0f}, .texture_coordinate= uv2},
	        { .position= {0.0f, 0.0f}, .texture_coordinate= uv3}
        };

        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

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

void gse::renderer2d::draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture, const glm::vec4& uv) {
	render_quad(position, size, nullptr, &texture, uv);
}

void gse::renderer2d::draw_text(const font& font, const std::string& text, const glm::vec2& position, const float scale, const glm::vec4& color) {
    if (text.empty()) return;

    g_msdf_shader.use();
    g_msdf_shader.set_int("uMSDF", 0);
    g_msdf_shader.set_vec4("uColor", color);
    g_msdf_shader.set_float("uRange", 4.0f);

    const texture& font_texture = font.get_texture();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_texture.get_texture_id());

    float start_x = position.x;
    const float start_y = position.y;

    for (const char c : text) {
        const auto& [u0, v0, u1, v1, width, height, x_offset, y_offset, x_advance] = font.get_character(c);

        if (width == 0.0f || height == 0.0f)
            continue;

        const float x_pos = start_x + x_offset * scale;
        const float y_pos = start_y - (height - y_offset) * scale;

        const float w = width * scale;
        const float h = height * scale;

        glm::vec4 uv_rect(u0, v0, (u1 - u0), (v1 - v0));

        draw_quad(glm::vec2(x_pos, y_pos), glm::vec2(w, h), font_texture, uv_rect);

        start_x += x_advance * scale;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}
