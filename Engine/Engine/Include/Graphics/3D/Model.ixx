module;

#include "glm/ext/matrix_transform.hpp"
#include "glad/glad.h"

export module gse.graphics.model;

import std;

import gse.graphics.mesh;
import gse.physics.math.units;
import gse.physics.math.vector;
import gse.core.id;

export namespace gse {
	class model_handle;

	class model : public identifiable {
	public:
		model(const std::string& tag) : identifiable(tag) {}

		auto initialize() -> void;

		std::vector<mesh> meshes;
		vec3<length> center_of_mass;
	};

	class model_handle {
	public:
		model_handle(id* model_id, const model& model);

		auto set_position(const vec3<length>& position) -> void;
		auto set_rotation(const vec3<angle>& rotation) -> void;
		auto set_material(const std::string& material_name) -> void;

		auto get_render_queue_entries() const -> const std::vector<render_queue_entry>&;
		auto get_model_id() const->id*;
	private:
		std::vector<render_queue_entry> m_render_queue_entries;
		id* m_model_id;
	};
}

import glm;

import gse.graphics.mesh;

auto gse::model::initialize() -> void {
	for (auto& mesh : meshes) {
		mesh.initialize();
		center_of_mass += mesh.center_of_mass;
	}
}

gse::model_handle::model_handle(id* model_id, const model& model) : m_model_id(model_id) {
	for (const auto& mesh : model.meshes) {
		m_render_queue_entries.emplace_back(
			"Concrete",
			mesh.vao,
			GL_TRIANGLES,
			static_cast<GLsizei>(mesh.indices.size()),
			glm::mat4(1.0f),
			glm::vec3(1.0f)
		);
	}
}

auto gse::model_handle::set_position(const vec3<length>& position) -> void {
	for (auto& render_queue_entry : m_render_queue_entries) {
		render_queue_entry.model_matrix = translate(glm::mat4(1.0f), position.as_default_units());
	}
}

auto gse::model_handle::set_rotation(const vec3<angle>& rotation) -> void {
	for (auto& render_queue_entry : m_render_queue_entries) {
		render_queue_entry.model_matrix = rotate(render_queue_entry.model_matrix, glm::radians(rotation.as<units::degrees>().x), glm::vec3(1.0f, 0.0f, 0.0f));
		render_queue_entry.model_matrix = rotate(render_queue_entry.model_matrix, glm::radians(rotation.as<units::degrees>().y), glm::vec3(0.0f, 1.0f, 0.0f));
		render_queue_entry.model_matrix = rotate(render_queue_entry.model_matrix, glm::radians(rotation.as<units::degrees>().z), glm::vec3(0.0f, 0.0f, 1.0f));
	}
}

auto gse::model_handle::set_material(const std::string& material_name) -> void {
	for (auto& render_queue_entry : m_render_queue_entries) {
		render_queue_entry.material_key = material_name;
	}
}

auto gse::model_handle::get_render_queue_entries() const -> const std::vector<render_queue_entry>& {
	return m_render_queue_entries;
}

auto gse::model_handle::get_model_id() const -> id* {
	return m_model_id;
}