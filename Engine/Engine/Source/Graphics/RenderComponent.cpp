#include "Graphics/RenderComponent.h"

void gse::render_component::add_mesh(std::unique_ptr<mesh> mesh) {
	mesh->set_up_mesh();
	m_meshes.push_back(std::move(mesh));
}

void gse::render_component::add_bounding_box_mesh(std::unique_ptr<bounding_box_mesh> bounding_box_mesh) {
	bounding_box_mesh->set_up_mesh();
	m_bounding_box_meshes.push_back(std::move(bounding_box_mesh));
}

void gse::render_component::update_bounding_box_meshes() const {
	for (const auto& bounding_box_mesh : m_bounding_box_meshes) {
		bounding_box_mesh->update();
	}
}

std::vector<gse::render_queue_entry> gse::render_component::get_queue_entries() const {
	if (!m_render) return {};

	std::vector<render_queue_entry> entries;
	entries.reserve(m_meshes.size() + m_bounding_box_meshes.size());
	for (const auto& mesh : m_meshes) {
		entries.push_back(mesh->get_queue_entry());
	}

	return entries;
}

std::vector<gse::render_queue_entry> gse::render_component::get_bounding_box_queue_entries() const {
	if (!m_render_bounding_boxes) return {};

	std::vector<render_queue_entry> entries;
	entries.reserve(m_bounding_box_meshes.size());
	for (const auto& bounding_box_mesh : m_bounding_box_meshes) {
		entries.push_back(bounding_box_mesh->get_queue_entry());
	}

	return entries;
}

std::vector<gse::mesh*> gse::render_component::get_meshes() const {
	return get_pointers(m_meshes);
}

std::vector<gse::bounding_box_mesh*> gse::render_component::get_bounding_box_meshes() const {
	return get_pointers(m_bounding_box_meshes);
}

void gse::render_component::set_render(const bool render, const bool render_bounding_boxes) {
	this->m_render = render;
	this->m_render_bounding_boxes = render_bounding_boxes;
}

void gse::render_component::set_mesh_positions(const vec3<length>& position) const {
	for (const auto& mesh : m_meshes) {
		mesh->set_position(position);
	}
}
