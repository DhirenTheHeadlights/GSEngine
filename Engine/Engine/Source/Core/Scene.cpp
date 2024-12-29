#include "Core/Scene.h"

#include "Core/ObjectRegistry.h"

void gse::scene::add_object(std::unique_ptr<object>&& object) {
	registry::register_object(object.get());
	m_objects.push_back(std::move(object));
}

void gse::scene::remove_object(const object* object_to_remove) {
	registry::unregister_object(object_to_remove);

	std::erase_if(m_objects, [&object_to_remove](const std::unique_ptr<object>& required_object) {
		return required_object.get() == object_to_remove;
		});
}

void gse::scene::initialize() const {
	initialize_hooks();

	for (const auto& object : m_objects) {
		object->initialize_hooks();
	}

	renderer3d::initialize_objects();
}

void gse::scene::update() const {
	update_hooks();

	physics::update();
	broad_phase_collision::update();

	for (const auto& object : m_objects) {
		object->update_hooks();
	}

	const auto pmc = registry::get_component<physics::motion_component>(grab_id("Player").lock().get());
	print("accel", pmc.current_acceleration);
}

void gse::scene::render() const {
	render_hooks();

	renderer3d::render();

	for (const auto& object : m_objects) {
		object->render_hooks();
	}
}

void gse::scene::exit() const {
	for (const auto& object : m_objects) {
		registry::unregister_object(object.get());
	}
}

std::vector<gse::object*> gse::scene::get_objects() const {
	std::vector<object*> objects;
	objects.reserve(m_objects.size());
	for (const auto& object : m_objects) {
		objects.push_back(object.get());
	}
	return objects;
}

