#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Object/Object.h"

namespace gse {
    class Texture {
    public:
        Texture() = default;
        explicit Texture(const std::string& filepath);
        ~Texture();

        void loadFromFile(const std::string& filepath);
        void bind(unsigned int unit = 0) const;
        void unbind() const;
        void setWrapping(GLenum wrapS, GLenum wrapT);
        void setFiltering(GLenum minFilter, GLenum magFilter);

        glm::ivec2 getDimensions() const { return dimensions; }
        GLuint getTextureID() const { return textureID; }

    private:
        GLuint textureID = 0;
        glm::ivec2 dimensions = { 0, 0 };
        std::string filepath;
        int channels = 0;
    };
}