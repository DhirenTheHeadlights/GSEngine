module;

#include <glad/glad.h>

export module gse.graphics.debug_rendering;

import std;

import gse.core.main_clock;
import gse.graphics.shader;
import gse.physics.math;

namespace std {
	template<>
	struct hash<gse::time> {
		size_t operator()(gse::time const& t) const noexcept {
			// pull out the raw float value and hash it
			return std::hash<float>()(t.as_default_unit());
		}
	};
}


export namespace gse::debug_renderer {

	auto initialize() -> void;
	auto update() -> void;
	auto render_debug_entities(const gse::mat4& view_matrix, const gse::mat4& projection_matrix, std::unordered_map<std::string, gse::shader> texture_shaders) -> void;
	auto add_debug_point(const gse::vec3<gse::length>& point) -> void;
	auto add_debug_vector(const gse::vec3<gse::length>& start, const gse::vec3<gse::length>& vector) -> void;
	
}


std::unordered_map<gse::time, gse::vec3<gse::length>> debug_points;

std::unordered_map<gse::time, std::pair<gse::vec3<gse::length>, gse::vec3<gse::length>>> debug_vectors;

gse::time g_debug_point_lifetime = gse::seconds(2.0f);
gse::time g_debug_vector_lifetime = gse::seconds(2.0f);


GLuint g_debug_point_vao = 0;
GLuint g_debug_point_vbo = 0;
GLuint g_debug_point_ebo = 0;
GLsizei g_debug_point_index_count = 0;


auto create_debug_sphere_vao() -> void {
	constexpr int sector_count = 18; // Longitude
	constexpr int stack_count = 12;  // Latitude
	constexpr float radius = 1.0f;

	std::vector<float> vertices;
	std::vector<std::uint32_t> indices;

	for (int i = 0; i <= stack_count; ++i) {
		float stack_angle = 3.1415926 / 2 - i * 3.1415926 / stack_count;
		float xy = radius * std::cosf(stack_angle);
		float z = radius * std::sinf(stack_angle);

		for (int j = 0; j <= sector_count; ++j) {
			float sector_angle = j * 2 * 3.1415926 / sector_count;
			float x = xy * std::cosf(sector_angle);
			float y = xy * std::sinf(sector_angle);
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(z);
		}
	}

	for (int i = 0; i < stack_count; ++i) {
		int k1 = i * (sector_count + 1);
		int k2 = k1 + sector_count + 1;

		for (int j = 0; j < sector_count; ++j, ++k1, ++k2) {
			if (i != 0) {
				indices.push_back(k1);
				indices.push_back(k2);
				indices.push_back(k1 + 1);
			}
			if (i != (stack_count - 1)) {
				indices.push_back(k1 + 1);
				indices.push_back(k2);
				indices.push_back(k2 + 1);
			}
		}
	}

	g_debug_point_index_count = static_cast<GLsizei>(indices.size());

	glGenVertexArrays(1, &g_debug_point_vao);
	glGenBuffers(1, &g_debug_point_vbo);
	glGenBuffers(1, &g_debug_point_ebo);

	glBindVertexArray(g_debug_point_vao);

	glBindBuffer(GL_ARRAY_BUFFER, g_debug_point_vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_debug_point_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0); // layout(location = 0)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

	glBindVertexArray(0);

}

static GLuint g_debug_vector_vao = 0;
static GLuint g_debug_vector_vbo = 0;
static GLsizei g_debug_vector_vertex_count = 0;


auto create_debug_vector_vao() -> void {
	constexpr GLfloat initial_verts[6] = {
		0.f, 0.f, 0.f,  // start
		0.f, 0.f, 0.f   // end (will be overwritten)
	};
	g_debug_vector_vertex_count = 2;

	glGenVertexArrays(1, &g_debug_vector_vao);
	glGenBuffers(1, &g_debug_vector_vbo);

	glBindVertexArray(g_debug_vector_vao);
	glBindBuffer(GL_ARRAY_BUFFER, g_debug_vector_vbo);
	// dynamic, since we’ll update it each frame
	glBufferData(GL_ARRAY_BUFFER, sizeof(initial_verts), initial_verts, GL_DYNAMIC_DRAW);

	// layout(location = 0) => vec3 position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
	glBindVertexArray(0);
}

auto render_debug_points(const gse::mat4& view_matrix, const gse::mat4& projection_matrix, std::unordered_map<std::string, gse::shader> texture_shaders) -> void {

	for (auto& [time, contact_world_pos] : debug_points) {

			// Disable depth test so point draws over everything
			glDisable(GL_DEPTH_TEST);

			// Use a simple shader for point rendering
			const auto& point_shader = texture_shaders["DebugPoint"];
			point_shader.use();
			point_shader.set_mat4("view", view_matrix);
			point_shader.set_mat4("projection", projection_matrix);
			point_shader.set_vec3("color", gse::unitless::vec3({ 1.0f, 0.0f, 0.0f })); // Bright red

			// Build a model matrix to place the point
			gse::mat4 point_model = gse::translate(gse::mat4(1.0f), contact_world_pos);
			point_model = gse::scale(point_model, gse::vec3<gse::length>(0.05f)); // Small point size

			point_shader.set_mat4("model", point_model);

			glBindVertexArray(g_debug_point_vao);
			glDrawElements(GL_TRIANGLES, g_debug_point_index_count, GL_UNSIGNED_INT, nullptr);
			glBindVertexArray(0);


			// Restore depth test
			glEnable(GL_DEPTH_TEST);
		}
}

auto render_debug_vectors(const gse::mat4& view_matrix, const gse::mat4& projection_matrix, std::unordered_map<std::string, gse::shader> texture_shaders) -> void {
	for (auto& [time, start_dir_pair] : debug_vectors) {
		const auto& [start, dir] = start_dir_pair;	
		auto s = start_dir_pair.first;
		auto d = start_dir_pair.second;

		GLfloat verts[6] = {
			s.x.as_default_unit(),       s.y.as_default_unit(),       s.z.as_default_unit(),
			s.x.as_default_unit() + d.x.as_default_unit(), s.y.as_default_unit() + d.y.as_default_unit(), s.z.as_default_unit() + d.z.as_default_unit()
		};

		glBindBuffer(GL_ARRAY_BUFFER, g_debug_vector_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		// Disable depth test so vector draws over everything
		glDisable(GL_DEPTH_TEST);
		// Use a simple shader for vector rendering
		const auto& vector_shader = texture_shaders["DebugVector"];
		vector_shader.use();
		vector_shader.set_mat4("view", view_matrix);
		vector_shader.set_mat4("projection", projection_matrix);
		vector_shader.set_vec3("color", gse::unitless::vec3({ 0.0f, 1.0f, 0.0f })); // Bright green
		glBindVertexArray(g_debug_vector_vao);
		glDrawArrays(GL_LINES, 0, g_debug_vector_vertex_count);
		glBindVertexArray(0);
		// Restore depth test
		glEnable(GL_DEPTH_TEST);
	}

}

auto gse::debug_renderer::initialize() -> void {
	create_debug_sphere_vao();
	create_debug_vector_vao();
}

auto gse::debug_renderer::render_debug_entities(const gse::mat4& view_matrix, const gse::mat4& projection_matrix, std::unordered_map<std::string, gse::shader> texture_shaders) -> void {
	render_debug_points(view_matrix, projection_matrix, texture_shaders);
}

auto gse::debug_renderer::update() -> void {
	const auto now = gse::main_clock::get_current_time();
	std::erase_if(debug_points, [&](auto const& kv) {
		return (now - kv.first) > g_debug_point_lifetime;
		});
	std::erase_if(debug_vectors, [&](auto const& kv) {
		return (now - kv.first) > g_debug_vector_lifetime;
		});
}



auto gse::debug_renderer::add_debug_point(const gse::vec3<gse::length>& point) -> void {
	debug_points[gse::main_clock::get_current_time()] = point;
}


auto gse::debug_renderer::add_debug_vector(const gse::vec3<gse::length>& start, const gse::vec3<gse::length>& vector) -> void {
	debug_vectors[gse::main_clock::get_current_time()] = std::make_pair(start, vector);
}