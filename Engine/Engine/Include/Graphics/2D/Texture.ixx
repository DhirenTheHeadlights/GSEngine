module;

#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

export module gse.graphics.texture;

import std;
import glm;

export namespace gse {
    class texture {
    public:
        texture() = default;
        explicit texture(const std::string& filepath);
        ~texture();

        auto load_from_file(const std::string& filepath) -> void;
        auto bind(unsigned int unit = 0) const -> void;
        auto unbind() const -> void;
        auto set_wrapping(GLenum wrap_s, GLenum wrap_t) const -> void;
        auto set_filtering(GLenum min_filter, GLenum mag_filter) const -> void;

        auto get_dimensions() const -> glm::ivec2 { return m_size; }
        auto get_texture_id() const -> GLuint { return m_texture_id; }

    private:
        GLuint m_texture_id = 0;
        glm::ivec2 m_size = { 0, 0 };
        std::string m_filepath;
        int m_channels = 0;
    };
}

gse::texture::texture(const std::string& filepath) : m_filepath(filepath) {
	load_from_file(filepath);
}

gse::texture::~texture() {
	glDeleteTextures(1, &m_texture_id);
}

auto gse::texture::load_from_file(const std::string& filepath) -> void {
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(filepath.c_str(), &m_size.x, &m_size.y, &m_channels, 0);

	if (!data) {
		std::cerr << "Failed to load texture: " << filepath << '\n';
		return;
	}

	glGenTextures(1, &m_texture_id);
	glBindTexture(GL_TEXTURE_2D, m_texture_id);

	GLenum format = GL_RGB;
	if (m_channels == 4) {
		format = GL_RGBA;
	}
	else if (m_channels == 1) {
		format = GL_RED;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), m_size.x, m_size.y, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);
}

auto gse::texture::bind(const unsigned int unit) const -> void {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, m_texture_id);
}

auto gse::texture::unbind() const -> void {
	glBindTexture(GL_TEXTURE_2D, 0);
}

auto gse::texture::set_wrapping(const GLenum wrap_s, const GLenum wrap_t) const -> void {
	glBindTexture(GL_TEXTURE_2D, m_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap_s));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap_t));
	glBindTexture(GL_TEXTURE_2D, 0);
}

auto gse::texture::set_filtering(const GLenum min_filter, const GLenum mag_filter) const -> void {
	glBindTexture(GL_TEXTURE_2D, m_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(min_filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(mag_filter));
	glBindTexture(GL_TEXTURE_2D, 0);
}
