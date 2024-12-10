#include "Core/Scene.h"

template <typename... ComponentTypes, typename Action>
void handleComponent(const std::shared_ptr<gse::object>& object, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Namespace
		action(object->getComponent<ComponentTypes>()...);
	}
}

template <typename... ComponentTypes, typename SystemType, typename Action>
void handleComponent(const std::shared_ptr<gse::object>& object, SystemType& system, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Class
		(system.*action)(object->getComponent<ComponentTypes>()...);
	}
}

void gse::scene::add_object(const std::weak_ptr<object>& object) {
	m_objects.push_back(object);

	/// Components are not added here; it is assumed that the object will only be ready
	///	to be initialized after all components have been added (when the scene is activated)
}

void gse::scene::remove_object(const std::weak_ptr<object>& object) {
	if (const auto objectPtr = object.lock()) {
		if (objectPtr->get_scene_id().lock() == m_id) {
			handleComponent<physics::motion_component>(objectPtr, m_physics_system, &physics::group::remove_motion_component);
			handleComponent<physics::collision_component>(objectPtr, m_collision_group, &broad_phase_collision::group::remove_object);
			handleComponent<render_component>(objectPtr, m_render_group, &renderer::group::remove_render_component);
			handleComponent<light_source_component>(objectPtr, m_render_group, &renderer::group::remove_light_source_component);
		}
	}

	std::erase_if(m_objects, [&object](const std::weak_ptr<object>& requiredObject) {
		return requiredObject.lock() == object.lock();
		});
}

void gse::scene::initialize() {
	for (auto& object : m_objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->process_initialize();

			handleComponent<physics::motion_component>(objectPtr, m_physics_system, &physics::group::add_motion_component);
			handleComponent<physics::collision_component, physics::motion_component>(objectPtr, m_collision_group, &broad_phase_collision::group::add_dynamic_object);
			handleComponent<physics::collision_component>(objectPtr, m_collision_group, &broad_phase_collision::group::add_object);
			handleComponent<render_component>(objectPtr, m_render_group, &renderer::group::add_render_component);
			handleComponent<light_source_component>(objectPtr, m_render_group, &renderer::group::add_light_source_component);
		}
	}
}

void gse::scene::update() {
	m_physics_system.update();
	broad_phase_collision::update(m_collision_group);

	for (auto& object : m_objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->process_update();
		}
	}
}

void gse::scene::render() {
	render_objects(m_render_group);

	for (auto& object : m_objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->process_render();
		}
	}
}

void gse::scene::exit() {
	// TODO
}

