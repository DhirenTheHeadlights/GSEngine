#include "Graphics/Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "glm/gtc/type_ptr.hpp"

gse::shader::shader(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometry_path) {
	if (!geometry_path.empty()) {
		create_shader_program(vertex_path, fragment_path, geometry_path);
	}
    else {
        create_shader_program(vertex_path, fragment_path);
    }
}

auto gse::shader::create_shader_program(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometry_path) -> void {
    // 1. Retrieve the vertex and fragment shader source code from file paths
    const std::string vertex_code = load_shader_source(vertex_path);
    const std::string fragment_code = load_shader_source(fragment_path);
	const std::string geometry_code = geometry_path.empty() ? "" : load_shader_source(geometry_path);

    const char* v_shader_code = vertex_code.c_str();
    const char* f_shader_code = fragment_code.c_str();

    // Vertex Shader
    const unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &v_shader_code, nullptr);
    glCompileShader(vertex_shader);
    check_compile_errors(vertex_shader, "VERTEX");

    // Fragment Shader
    const unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &f_shader_code, nullptr);
    glCompileShader(fragment_shader);
    check_compile_errors(fragment_shader, "FRAGMENT");

	// Geometry Shader
    const unsigned int geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
    if (!geometry_path.empty()) {
        const char* g_shader_code = geometry_code.c_str();
        glShaderSource(geometry_shader, 1, &g_shader_code, nullptr);
        glCompileShader(geometry_shader);
        check_compile_errors(geometry_shader, "GEOMETRY");
    }

    // 3. Link shaders to create the shader program
    m_id = glCreateProgram();
    glAttachShader(m_id, vertex_shader);
    glAttachShader(m_id, fragment_shader);
	if (!geometry_path.empty()) {
		glAttachShader(m_id, geometry_shader);
	}
    glLinkProgram(m_id);
    check_compile_errors(m_id, "PROGRAM");

    // 4. Clean up the shaders as they are no longer needed once linked
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
	if (!geometry_path.empty()) {
		glDeleteShader(geometry_shader);
	}

    // Cache uniform locations
    cache_uniform_locations();
}

auto gse::shader::use() const -> void {
    glUseProgram(m_id);
}

auto gse::shader::load_shader_source(const std::string& file_path) -> std::string {
    std::ifstream shader_file;
    std::stringstream shader_stream;

    // Ensure ifstream objects can throw exceptions
    shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        // Open the file
        shader_file.open(file_path);
        shader_stream << shader_file.rdbuf();
        shader_file.close();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << '\n';
    }

    return shader_stream.str();
}

auto gse::shader::cache_uniform_locations() -> void {
    int uniform_count;
    glGetProgramiv(m_id, GL_ACTIVE_UNIFORMS, &uniform_count);
    for (int i = 0; i < uniform_count; ++i) {
        char uniform_name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        glGetActiveUniform(m_id, i, sizeof(uniform_name), &length, &size, &type, uniform_name);
        const GLint location = glGetUniformLocation(m_id, uniform_name);
        m_uniforms[std::string(uniform_name)] = location;
    }
}

// Utility to check and report shader compile and linking errors
auto gse::shader::check_compile_errors(const unsigned int shader, const std::string& type) -> void {
    int success;
    char info_log[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, info_log);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << info_log << '\n';
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, info_log);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << info_log << '\n';
        }
    }
}

// Utility functions for setting shader uniforms
auto gse::shader::set_bool(const std::string& name, const bool value) const -> void {
    glUniform1i(glGetUniformLocation(m_id, name.c_str()), static_cast<int>(value));
}

auto gse::shader::set_int(const std::string& name, const int value) const -> void {
    glUniform1i(glGetUniformLocation(m_id, name.c_str()), value);
}

auto gse::shader::set_int_array(const std::string& name, const int* values, const unsigned int count) const -> void {
	glUniform1iv(glGetUniformLocation(m_id, name.c_str()), count, values);
}

auto gse::shader::set_float(const std::string& name, const float value) const -> void {
    glUniform1f(glGetUniformLocation(m_id, name.c_str()), value);
}

auto gse::shader::set_mat4(const ::std::string& name, const glm::mat4& value) const -> void {
    glUniformMatrix4fv(glGetUniformLocation(m_id, name.c_str()), 1, GL_FALSE, value_ptr(value));
}

auto gse::shader::set_mat4_array(const ::std::string& name, const glm::mat4* values, const unsigned int count) const -> void {
    glUniformMatrix4fv(glGetUniformLocation(m_id, name.c_str()), count, GL_FALSE, value_ptr(*values));
}

auto gse::shader::set_vec3(const ::std::string& name, const glm::vec3& value) const -> void {
	glUniform3fv(glGetUniformLocation(m_id, name.c_str()), 1, value_ptr(value));
}

auto gse::shader::set_vec4(const ::std::string& name, const glm::vec4& value) const -> void {
	glUniform4fv(glGetUniformLocation(m_id, name.c_str()), 1, value_ptr(value));
}