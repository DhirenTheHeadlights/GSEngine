export module gse.graphics:render_component;

import std;

import :mesh;
import :model;
import :model_loader;

import gse.utility;
import gse.physics.math;

export namespace gse {
	struct render_component final : component {
		render_component(const std::uint32_t id) : component(id) {}
		render_component(std::uint32_t id, const model_handle& handle);

		render_component(render_component&&) noexcept = default;
		auto operator=(render_component&&) noexcept -> render_component & = default;

		render_component(const render_component&) = delete;
		auto operator=(const render_component&) -> render_component & = delete;

		auto update_bounding_box_meshes() -> void;
		auto set_model_positions(const vec3<length>& position) -> void;

		std::vector<model_handle> models;
		std::vector<mesh> bounding_box_meshes;

		auto calculate_center_of_mass() -> void;
		vec3<length> center_of_mass;

		bool render = true;
		bool render_bounding_boxes = true;
	};
}

gse::render_component::render_component(const std::uint32_t id, const model_handle& handle) : component(id) {
	models.emplace_back(model_loader::model(handle));
	calculate_center_of_mass();
}

auto gse::render_component::update_bounding_box_meshes() -> void {
	for (auto& bounding_box_mesh : bounding_box_meshes) {
		// TODO: Implement this
	}
}

auto gse::render_component::set_model_positions(const vec3<length>& position) -> void {
	for (auto& model_handle : models) {
		model_handle.set_position(position);
	}
}

auto gse::render_component::calculate_center_of_mass() -> void {
	vec3<length> sum;
	int number_of_meshes = 0;
	for (const auto& model_handle : models) {
		for (const auto& model = model_loader::model(model_handle); auto& model_mesh : model.m_meshes) {
			sum += model_mesh.center_of_mass;
			number_of_meshes += static_cast<int>(model.m_meshes.size());
		}
	}

	center_of_mass = sum / static_cast<float>(number_of_meshes);
}