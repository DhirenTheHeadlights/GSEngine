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