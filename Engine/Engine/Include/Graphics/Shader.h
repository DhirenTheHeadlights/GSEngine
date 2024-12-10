#pragma once

#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gse {
    class shader {
    public:
        shader() = default;
		shader(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometry_path = "");

		void create_shader_program(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometry_path = "");
        void use() const;

        // Utility functions to set uniform values
        void set_bool(const std::string& name, bool value) const;
        void set_int(const std::string& name, int value) const;
		void set_int_array(const std::string& name, const int* values, unsigned int count) const;
        void set_float(const std::string& name, float value) const;
        void set_mat4(const std::string& name, const glm::mat4& value) const;
		void set_mat4_array(const std::string& name, const glm::mat4* values, unsigned int count) const;
		void set_vec3(const std::string& name, const glm::vec3& value) const;

    	unsigned int get_id() const { return m_id; }
    private:
        unsigned int m_id = 0;

        static std::string load_shader_source(const std::string& file_path);

        void cache_uniform_locations();
        
        std::unordered_map<std::string, GLint> m_uniforms;

        static void check_compile_errors(unsigned int shader, const std::string& type);
    };
}