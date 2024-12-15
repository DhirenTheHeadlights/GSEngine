#include "Core/Scene.h"

#include "Core/ObjectRegistry.h"

namespace {
	template <typename... component_types, typename action_type>
	void handle_component(const gse::object* object, action_type action) {
		if ((object->get_component<component_types>() && ...)) { // Namespace
			action(object->get_component<component_types>()...);
		}
	}

	template <typename... component_types, typename system_type, typename action_type>
	void handle_component(const gse::object* object, system_type& system, action_type action) {
		if ((object->get_component<component_types>() && ...)) { // Class
			(system.*action)(object->get_component<component_types>()...);
		}
	}
}

void gse::scene::add_object(std::unique_ptr<object>&& object) {
	m_objects.push_back(std::move(object));
	registry::register_object(object.get());

	/// Components are not added here; it is assumed that the object will only be ready
	///	to be initialized after all components have been added (when the scene is activated)
}

void gse::scene::remove_object(const object* object_to_remove) {
	registry::unregister_object(object_to_remove);

	if (object_to_remove->get_scene_id().lock() == m_id) {
		handle_component<physics::motion_component>(object_to_remove, m_physics_system, &physics::group::remove_motion_component);
		handle_component<physics::collision_component>(object_to_remove, m_collision_group, &broad_phase_collision::group::remove_object);
		handle_component<render_component>(object_to_remove, m_render_group, &renderer::group::remove_render_component);
		handle_component<light_source_component>(object_to_remove, m_render_group, &renderer::group::remove_light_source_component);
	}

	std::erase_if(m_objects, [&object_to_remove](const std::unique_ptr<object>& required_object) {
		return required_object.get() == object_to_remove;
		});
}

void gse::scene::initialize() {
	for (auto& object : m_objects) {
		object->process_initialize();

		handle_component<physics::motion_component>(object.get(), m_physics_system, &physics::group::add_motion_component);
		handle_component<physics::collision_component, physics::motion_component>(object.get(), m_collision_group, &broad_phase_collision::group::add_dynamic_object);
		handle_component<physics::collision_component>(object.get(), m_collision_group, &broad_phase_collision::group::add_object);
		handle_component<render_component>(object.get(), m_render_group, &renderer::group::add_render_component);
		handle_component<light_source_component>(object.get(), m_render_group, &renderer::group::add_light_source_component);
	}
}

void gse::scene::update() {
	m_physics_system.update();
	broad_phase_collision::update(m_collision_group);

	for (const auto& object : m_objects) {
		object->process_update();
	}
}

void gse::scene::render() {
	render_objects(m_render_group);

	for (const auto& object : m_objects) {
		object->process_render();
	}
}

void gse::scene::exit() const {
	for (const auto& object : m_objects) {
		registry::unregister_object(object.get());
	}
}

