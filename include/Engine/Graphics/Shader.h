#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace Engine {
    class Shader {
    public:
        // Constructor reads and builds the shader from given file paths
        Shader(const std::string& vertexPath, const std::string& fragmentPath);

        // Use the shader program
        void use();

        // Utility functions to set uniform values
        void setBool(const std::string& name, bool value) const;
        void setInt(const std::string& name, int value) const;
        void setFloat(const std::string& name, float value) const;
        void setMat4(const std::string& name, const GLfloat* value) const;

    private:
        // ID of the shader program
        unsigned int ID;

        // Utility function to load shader from file
        std::string loadShaderSource(const std::string& filePath);

        // Utility function to check for shader compile/link errors
        void checkCompileErrors(unsigned int shader, std::string type);
    };
}