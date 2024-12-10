#include "Graphics/Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "glm/gtc/type_ptr.hpp"

gse::Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath) {
	if (!geometryPath.empty()) {
		createShaderProgram(vertexPath, fragmentPath, geometryPath);
	}
    else {
        createShaderProgram(vertexPath, fragmentPath);
    }
}

void gse::Shader::createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath) {
    // 1. Retrieve the vertex and fragment shader source code from file paths
    const std::string vertexCode = loadShaderSource(vertexPath);
    const std::string fragmentCode = loadShaderSource(fragmentPath);
	const std::string geometryCode = geometryPath.empty() ? "" : loadShaderSource(geometryPath);

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // Vertex Shader
    const unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vShaderCode, nullptr);
    glCompileShader(vertexShader);
    checkCompileErrors(vertexShader, "VERTEX");

    // Fragment Shader
    const unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fShaderCode, nullptr);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT");

	// Geometry Shader
    const unsigned int geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
    if (!geometryPath.empty()) {
        const char* gShaderCode = geometryCode.c_str();
        glShaderSource(geometryShader, 1, &gShaderCode, nullptr);
        glCompileShader(geometryShader);
        checkCompileErrors(geometryShader, "GEOMETRY");
    }

    // 3. Link shaders to create the shader program
    ID = glCreateProgram();
    glAttachShader(ID, vertexShader);
    glAttachShader(ID, fragmentShader);
	if (!geometryPath.empty()) {
		glAttachShader(ID, geometryShader);
	}
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    // 4. Clean up the shaders as they are no longer needed once linked
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
	if (!geometryPath.empty()) {
		glDeleteShader(geometryShader);
	}

    // Cache uniform locations
    cacheUniformLocations();
}

void gse::Shader::use() const {
    glUseProgram(ID);
}

std::string gse::Shader::loadShaderSource(const std::string& filePath) {
    std::ifstream shaderFile;
    std::stringstream shaderStream;

    // Ensure ifstream objects can throw exceptions
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        // Open the file
        shaderFile.open(filePath);
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << '\n';
    }

    return shaderStream.str();
}

void gse::Shader::cacheUniformLocations() {
    int uniformCount;
    glGetProgramiv(ID, GL_ACTIVE_UNIFORMS, &uniformCount);
    for (int i = 0; i < uniformCount; ++i) {
        char uniformName[256];
        GLsizei length;
        GLint size;
        GLenum type;
        glGetActiveUniform(ID, i, sizeof(uniformName), &length, &size, &type, uniformName);
        const GLint location = glGetUniformLocation(ID, uniformName);
        uniforms[std::string(uniformName)] = location;
    }
}

// Utility to check and report shader compile and linking errors
void gse::Shader::checkCompileErrors(const unsigned int shader, const std::string& type) {
    int success;
    char infoLog[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << '\n';
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << '\n';
        }
    }
}

// Utility functions for setting shader uniforms
void gse::Shader::setBool(const std::string& name, const bool value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), static_cast<int>(value));
}

void gse::Shader::setInt(const std::string& name, const int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void gse::Shader::setIntArray(const std::string& name, const int* values, const unsigned int count) const {
	glUniform1iv(glGetUniformLocation(ID, name.c_str()), count, values);
}

void gse::Shader::setFloat(const std::string& name, const float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void gse::Shader::setMat4(const ::std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value_ptr(value));
}

void gse::Shader::setMat4Array(const ::std::string& name, const glm::mat4* values, const unsigned int count) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), count, GL_FALSE, value_ptr(*values));
}

void gse::Shader::setVec3(const ::std::string& name, const glm::vec3& value) const {
	glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, value_ptr(value));
}
