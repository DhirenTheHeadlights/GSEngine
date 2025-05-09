module;

#include <glad/glad.h>
#include <stb_image.h>

export module gse.graphics.cube_map;

import std;

import gse.physics.math;

export namespace gse {
    class cube_map {
    public:
        cube_map() = default;
        ~cube_map();

        auto create(const std::vector<std::string>& faces) -> void;
        auto create(int resolution, bool depth_only = false) -> void;
        auto bind(std::uint32_t unit) const -> void;
        auto update(const vec3<length>& position, const mat4& projection_matrix, const std::function<void(const mat4&, const mat4&)>& render_function) const -> void;

        auto get_texture_id() const -> std::uint32_t { return m_texture_id; }
        auto get_frame_buffer_id() const -> std::uint32_t { return m_frame_buffer_id; }
    private:
        std::uint32_t m_texture_id;
        std::uint32_t m_frame_buffer_id;
        std::uint32_t m_depth_render_buffer_id;
        int m_resolution;
        bool m_depth_only;

        static auto get_view_matrices(const vec3<length>& position) -> std::vector<mat4>;
    };
}

import gse.platform.glfw.window;

gse::cube_map::~cube_map() {
    glDeleteTextures(1, &m_texture_id);
    glDeleteFramebuffers(1, &m_frame_buffer_id);
    glDeleteRenderbuffers(1, &m_depth_render_buffer_id);
}

auto gse::cube_map::create(const std::vector<std::string>& faces) -> void {
    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_texture_id);

    int width, height, nr_channels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nr_channels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cerr << "Failed to load cube map texture: " << faces[i] << '\n';
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

auto gse::cube_map::create(const int resolution, const bool depth_only) -> void {
    this->m_resolution = resolution;
    this->m_depth_only = depth_only;

    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_texture_id);

    const GLenum internal_format = depth_only ? GL_DEPTH_COMPONENT : GL_RGB16F;
    const GLenum format = depth_only ? GL_DEPTH_COMPONENT : GL_RGB;

    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, static_cast<GLint>(internal_format),
            resolution, resolution, 0, format, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_frame_buffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer_id);

    if (depth_only) {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texture_id, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else {
        glGenRenderbuffers(1, &m_depth_render_buffer_id);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depth_render_buffer_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, resolution, resolution);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth_render_buffer_id);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

auto gse::cube_map::bind(const std::uint32_t unit) const -> void {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_texture_id);
}

auto gse::cube_map::update(const vec3<length>& position, const mat4& projection_matrix, const std::function<void(const mat4&, const mat4&)>& render_function) const -> void {
    glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer_id);
    glViewport(0, 0, m_resolution, m_resolution);

    const std::vector<mat4> view_matrices = get_view_matrices(position);

    for (unsigned int i = 0; i < 6; i++) {
        if (m_depth_only) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_texture_id, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        else {
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_texture_id, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        render_function(view_matrices[i], projection_matrix);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window::get_frame_buffer_size().x, window::get_frame_buffer_size().y);
}

auto gse::cube_map::get_view_matrices(const vec3<length>& position) -> std::vector<mat4> {
    return {
        look_at(position, position + vec3<length>(1.0f, 0.0f,  0.0f), { 0.0f, -1.0f, 0.0f }),  // +X
        look_at(position, position + vec3<length>(-1.0f, 0.0f, 0.0f), { 0.0f, -1.0f, 0.0f }),  // -X
        look_at(position, position + vec3<length>(0.0f, 1.0f, 0.0f),  { 0.0f, 0.0f,  1.0f }),  // +Y
        look_at(position, position + vec3<length>(0.0f, -1.0f, 0.0f), { 0.0f, 0.0f, -1.0f }),  // -Y
        look_at(position, position + vec3<length>(0.0f, 0.0f, 1.0f),  { 0.0f, -1.0f, 0.0f }),  // +Z
        look_at(position, position + vec3<length>(0.0f, 0.0f, -1.0f), { 0.0f, -1.0f, 0.0f })   // -Z
    };
}