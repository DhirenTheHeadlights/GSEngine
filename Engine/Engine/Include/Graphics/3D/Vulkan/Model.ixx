export module gse.graphics.model;

import std;
import vulkan_hpp;

import gse.graphics.mesh;
import gse.platform;
import gse.physics.math;
import gse.core.id;

export namespace gse {
    class model_handle;

    class model : public identifiable {
    public:
	    explicit model(const std::string& tag) : identifiable(tag) {}

        auto initialize(const vulkan::config& config) -> void;

        std::vector<mesh> meshes;
        vec3<length> center_of_mass;
    };

    class model_handle {
    public:
	    explicit model_handle(const model& model);

        auto set_position(const vec3<length>& position) -> void;
        auto set_rotation(const vec3<angle>& rotation) -> void;
        auto get_render_queue_entries() const -> std::span<const render_queue_entry>;
        auto get_model_id() const -> id;

		auto operator==(const model_handle& other) const -> bool {
			return m_model_id == other.m_model_id;
		}
    private:
        std::vector<render_queue_entry> m_render_queue_entries;
        id m_model_id;
    };
}

auto gse::model::initialize(const vulkan::config& config) -> void {
    for (auto& mesh : meshes) {
        mesh.initialize(config);
        center_of_mass += mesh.center_of_mass;
    }
}

gse::model_handle::model_handle(const model& model) : m_model_id(model.get_id()) {
    for (const auto& mesh : model.meshes) {
        m_render_queue_entries.emplace_back(
			&mesh,
            mat4(1.0f),            // Identity matrix
            unitless::vec3(1.0f)   // Default color
        );
    }
}

auto gse::model_handle::set_position(const vec3<length>& position) -> void {
    for (auto& render_queue_entry : m_render_queue_entries) {
        render_queue_entry.model_matrix = translate(mat4(1.0f), position);
    }
}

auto gse::model_handle::set_rotation(const vec3<angle>& rotation) -> void {
    for (auto& render_queue_entry : m_render_queue_entries) {
        render_queue_entry.model_matrix = rotate(render_queue_entry.model_matrix, axis::x, rotation.x);
        render_queue_entry.model_matrix = rotate(render_queue_entry.model_matrix, axis::y, rotation.y);
        render_queue_entry.model_matrix = rotate(render_queue_entry.model_matrix, axis::z, rotation.z);
    }
}

auto gse::model_handle::get_render_queue_entries() const -> std::span<const render_queue_entry> {
    return m_render_queue_entries;
}

auto gse::model_handle::get_model_id() const -> id {
    return m_model_id;
}
