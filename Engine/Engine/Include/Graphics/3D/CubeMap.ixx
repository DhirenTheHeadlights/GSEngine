module;

#include <glad/glad.h>

export module gse.graphics.cube_map;

import std;
import glm;

export namespace gse {
	class cube_map {
	public:
		cube_map() = default;
		~cube_map();

		auto create(const std::vector<std::string>& faces) -> void;
		auto create(int resolution, bool depth_only = false) -> void;
		auto bind(GLuint unit) const -> void;
		auto update(const glm::vec3& position, const glm::mat4& projection_matrix, const std::function<void(const glm::mat4&, const glm::mat4&)>& render_function) const -> void;

		auto get_texture_id() const -> GLuint { return m_texture_id; }
		auto get_frame_buffer_id() const -> GLuint { return m_frame_buffer_id; }
	private:
		GLuint m_texture_id;
		GLuint m_frame_buffer_id;
		GLuint m_depth_render_buffer_id;
		int m_resolution;
		bool m_depth_only;

		static auto get_view_matrices(const glm::vec3& position) -> std::vector<glm::mat4>;
	};
}
