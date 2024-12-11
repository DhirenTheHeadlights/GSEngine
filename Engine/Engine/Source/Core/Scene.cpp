#include "Core/Scene.h"

template <typename... component_types, typename action_type>
void handle_component(const std::shared_ptr<gse::object>& object, action_type action) {
	if ((object->get_component<component_types>() && ...)) { // Namespace
		action(object->get_component<component_types>()...);
	}
}

template <typename... component_types, typename system_type, typename action_type>
void handle_component(const std::shared_ptr<gse::object>& object, system_type& system, action_type action) {
	if ((object->get_component<component_types>() && ...)) { // Class
		(system.*action)(object->get_component<component_types>()...);
	}
}

void gse::scene::add_object(const std::weak_ptr<object>& object) {
	m_objects.push_back(object);

	/// Components are not added here; it is assumed that the object will only be ready
	///	to be initialized after all components have been added (when the scene is activated)
}

void gse::scene::remove_object(const std::weak_ptr<object>& object_to_remove) {
	if (const auto object_ptr = object_to_remove.lock()) {
		if (object_ptr->get_scene_id().lock() == m_id) {
			handle_component<physics::motion_component>(object_ptr, m_physics_system, &physics::group::remove_motion_component);
			handle_component<physics::collision_component>(object_ptr, m_collision_group, &broad_phase_collision::group::remove_object);
			handle_component<render_component>(object_ptr, m_render_group, &renderer::group::remove_render_component);
			handle_component<light_source_component>(object_ptr, m_render_group, &renderer::group::remove_light_source_component);
		}
	}

	std::erase_if(m_objects, [&object_to_remove](const std::weak_ptr<object>& required_object) {
		return required_object.lock() == object_to_remove.lock();
		});
}

void gse::scene::initialize() {
	for (auto& object : m_objects) {
		if (const auto object_ptr = object.lock()) {
			object_ptr->process_initialize();

			handle_component<physics::motion_component>(object_ptr, m_physics_system, &physics::group::add_motion_component);
			handle_component<physics::collision_component, physics::motion_component>(object_ptr, m_collision_group, &broad_phase_collision::group::add_dynamic_object);
			handle_component<physics::collision_component>(object_ptr, m_collision_group, &broad_phase_collision::group::add_object);
			handle_component<render_component>(object_ptr, m_render_group, &renderer::group::add_render_component);
			handle_component<light_source_component>(object_ptr, m_render_group, &renderer::group::add_light_source_component);
		}
	}
}

void gse::scene::update() {
	m_physics_system.update();
	broad_phase_collision::update(m_collision_group);

	for (auto& object : m_objects) {
		if (const auto object_ptr = object.lock()) {
			object_ptr->process_update();
		}
	}
}

void gse::scene::render() {
	render_objects(m_render_group);

	for (auto& object : m_objects) {
		if (const auto object_ptr = object.lock()) {
			object_ptr->process_render();
		}
	}
}

void gse::scene::exit() {
	// TODO
}

