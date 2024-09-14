#pragma once

#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Engine {
    class Shader {
    public:
        // Constructor reads and builds the shader from given file paths
        Shader() = default;

        void createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);

        // Use the shader program
        void use() const;

        // Utility functions to set uniform values
        void setBool(const std::string& name, bool value) const;
        void setInt(const std::string& name, int value) const;
        void setFloat(const std::string& name, float value) const;
        void setMat4(const std::string& name, const GLfloat* value) const;

        int getID() const { return ID; }
        GLint getUniformLocation(const std::string& name) const { return uniforms.at(name); }

        static glm::mat4 getProjectionMatrix() { return glm::mat4(1.f); }

    private:
        // ID of the shader program
        unsigned int ID = 0;

        // Utility function to load shader from file
        static std::string loadShaderSource(const std::string& filePath);

        void cacheUniformLocations();
        
        std::unordered_map<std::string, GLint> uniforms;

        // Utility function to check for shader compile/link errors
        static void checkCompileErrors(unsigned int shader, const std::string& type);
    };
}