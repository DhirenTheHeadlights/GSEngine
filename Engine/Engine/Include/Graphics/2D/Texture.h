#pragma once

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Core/Object/Object.h"

namespace gse {
    class texture {
    public:
        texture() = default;
        explicit texture(const std::string& filepath);
        ~texture();

        void load_from_file(const std::string& filepath);
        void bind(unsigned int unit = 0) const;
        void unbind() const;
        void set_wrapping(GLenum wrap_s, GLenum wrap_t) const;
        void set_filtering(GLenum min_filter, GLenum mag_filter) const;

        glm::ivec2 get_dimensions() const { return m_size; }
        GLuint get_texture_id() const { return m_texture_id; }

    private:
        GLuint m_texture_id = 0;
        glm::ivec2 m_size = { 0, 0 };
        std::string m_filepath;
        int m_channels = 0;
    };
}