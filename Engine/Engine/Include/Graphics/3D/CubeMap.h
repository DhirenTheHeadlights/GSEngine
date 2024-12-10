#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace gse {
	class cube_map {
	public:
		cube_map() = default;
		~cube_map();

		void create(const std::vector<std::string>& faces);
		void create(int resolution, bool depth_only = false);
		void bind(GLuint unit) const;
		void update(const glm::vec3& position, const glm::mat4& projection_matrix, const std::function<void(const glm::mat4&, const glm::mat4&)>& render_function) const;

		GLuint get_texture_id() const { return m_texture_id; }
		GLuint get_frame_buffer_id() const { return m_frame_buffer_id; }
	private:
		GLuint m_texture_id;
		GLuint m_frame_buffer_id;
		GLuint m_depth_render_buffer_id;
		int m_resolution;

		static std::vector<glm::mat4> get_view_matrices(const glm::vec3& position);
	};
}
