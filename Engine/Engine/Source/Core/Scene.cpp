#include "Core/Scene.h"

#include "Core/ObjectRegistry.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisions.h"

auto gse::scene::add_object(object* object, const std::string& name) -> void {
	if (m_is_active) {
		m_object_indexes.push_back(registry::add_object(object, name));
	}
	else {
		m_objects_to_add_upon_initialization.push_back(object);
	}
}

auto gse::scene::remove_object(const std::string& name) -> void {
	std::erase(m_object_indexes, registry::remove_object(name));
}

auto gse::scene::initialize() -> void {
	initialize_hooks();

	for (const auto& object : m_objects_to_add_upon_initialization) {
		m_object_indexes.push_back(registry::add_object(object));
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

	auto& test = gse::registry::get_component_containers();
	auto& test2 = gse::registry::get_queued_components();
}

auto gse::scene::exit() const -> void {
	for (const auto& index : m_object_indexes) {
		registry::remove_object(index);
	}
}

auto gse::scene::get_objects() const -> std::vector<object*> {
	std::vector<object*> objects;
	objects.reserve(m_object_indexes.size());
	for (const auto& index : m_object_indexes) {
		objects.push_back(registry::get_object(index));
	}
	return objects;
}

