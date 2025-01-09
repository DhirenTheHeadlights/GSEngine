#include "Core/Scene.h"

#include "Core/ObjectRegistry.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisions.h"

auto gse::scene::add_entity(std::uint32_t object_uuid, const std::string& name) -> void {
	if (m_is_active) {
		m_object_indexes.push_back(registry::activate_entity(object_uuid, name));
	}
	else {
		m_objects_to_add_upon_initialization.emplace_back(object_uuid, name);
	}
}

auto gse::scene::remove_entity(const std::string& name) -> void {
	std::erase(m_object_indexes, registry::remove_entity(name));
}

auto gse::scene::initialize() -> void {
	initialize_hooks();

	for (const auto& [object_uuid, name] : m_objects_to_add_upon_initialization) {
		m_object_indexes.push_back(registry::activate_entity(object_uuid, name));
	}

	m_objects_to_add_upon_initialization.clear();

	registry::initialize_hooks();

	renderer3d::initialize_objects();
}

auto gse::scene::update() const -> void {
	update_hooks();

	registry::update_hooks();
	physics::update();
	broad_phase_collision::update();
}

auto gse::scene::render() const -> void {
	render_hooks();

	registry::render_hooks();
	renderer3d::render();
}

auto gse::scene::exit() const -> void {
	for (const auto& index : m_object_indexes) {
		registry::remove_entity(index);
	}
}

auto gse::scene::get_entities() const -> std::vector<std::uint32_t> {
	return m_object_indexes;
}

