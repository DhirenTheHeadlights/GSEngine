module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

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