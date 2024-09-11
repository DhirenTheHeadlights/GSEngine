#include "Shader.h"

using namespace Engine;

// Constructor that builds the shader program from vertex and fragment shader file paths
void Shader::createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    // 1. Retrieve the vertex and fragment shader source code from file paths
    const std::string vertexCode = loadShaderSource(vertexPath);
    const std::string fragmentCode = loadShaderSource(fragmentPath);

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // Vertex Shader
    const unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vShaderCode, NULL);
    glCompileShader(vertexShader);
    checkCompileErrors(vertexShader, "VERTEX");

    // Fragment Shader
    const unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fShaderCode, NULL);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT");

    // 3. Link shaders to create the shader program
    ID = glCreateProgram();
    glAttachShader(ID, vertexShader);
    glAttachShader(ID, fragmentShader);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    // 4. Clean up the shaders as they are no longer needed once linked
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Cache uniform locations
    cacheUniformLocations();
}

// Use the shader program
void Shader::use() const {
    glUseProgram(ID);
}

// Load shader source code from a file
std::string Shader::loadShaderSource(const std::string& filePath) {
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

void Shader::cacheUniformLocations() {
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
void Shader::checkCompileErrors(unsigned int shader, const std::string& type) {
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
void Shader::setBool(const std::string& name, const bool value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), static_cast<int>(value));
}

void Shader::setInt(const std::string& name, const int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, const float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setMat4(const std::string& name, const GLfloat* value) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value);
}
