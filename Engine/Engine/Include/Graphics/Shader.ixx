export module gse.graphics.shader;

import std;
import glm;

export namespace gse {
    class shader {
    public:
        shader() = default;
		shader(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometry_path = "");

		auto create_shader_program(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometry_path = "") -> void;
        auto use() const -> void;

        // Utility functions to set uniform values
        auto set_bool(const std::string& name, bool value) const -> void;
        auto set_int(const std::string& name, int value) const -> void;
		auto set_int_array(const std::string& name, const int* values, unsigned int count) const -> void;
        auto set_float(const std::string& name, float value) const -> void;
        auto set_mat4(const std::string& name, const glm::mat4& value) const -> void;
		auto set_mat4_array(const std::string& name, const glm::mat4* values, unsigned int count) const -> void;
		auto set_vec3(const std::string& name, const glm::vec3& value) const -> void;
		auto set_vec4(const std::string& name, const glm::vec4& value) const -> void;

    	auto get_id() const -> unsigned int { return m_id; }
    private:
        unsigned int m_id = 0;

        static auto load_shader_source(const std::string& file_path) -> std::string;

        auto cache_uniform_locations() -> void;
        
        std::unordered_map<std::string, int> m_uniforms;

        static auto check_compile_errors(unsigned int shader, const std::string& type) -> void;
    };
}