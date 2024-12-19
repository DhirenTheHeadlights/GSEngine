#include "Core/SceneHandler.h"

#include <ranges>

void gse::scene_handler::add_scene(std::unique_ptr<scene>& scene) {
	m_scenes.insert({ scene->get_id().lock().get(), std::move(scene)});
}

void gse::scene_handler::remove_scene(id* scene_id) {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (scene->second->get_active()) {
			scene->second->exit();
		}
		m_scenes.erase(scene);
	}
}

void gse::scene_handler::activate_scene(id* scene_id) {
	permaAssertComment(m_engine_initialized, "You are trying to activate a scene before the engine is initialized");
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (!scene->second->get_active()) {
			scene->second->initialize();
			scene->second->set_active(true);
		}
	}
}

void gse::scene_handler::deactivate_scene(id* scene_id) {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (scene->second->get_active()) {
			scene->second->exit();
			scene->second->set_active(false);
		}
	}
}

void gse::scene_handler::update() {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->get_active()) {
			scene->update();
		}
	}

	for (const auto& [id, trigger] : m_scene_triggers) {
		if (trigger()) {
			activate_scene(id);
		}
	}
}

void gse::scene_handler::render() {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->get_active()) {
			scene->render();
		}
	}
}

void gse::scene_handler::exit() {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->get_active()) {
			scene->exit();
		}
	}
}

void gse::scene_handler::queue_scene_trigger(id* id, const std::function<bool()>& trigger) {
	m_scene_triggers.insert({ id, trigger });
}

std::vector<gse::scene*> gse::scene_handler::get_active_scenes() const {
	std::vector<scene*> active_scenes;
	active_scenes.reserve(m_scenes.size());
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->get_active()) {
			active_scenes.push_back(scene.get());
		}
	}
	return active_scenes;
}

std::vector<gse::id*> gse::scene_handler::get_all_scenes() const {
	std::vector<id*> all_scenes;
	all_scenes.reserve(m_scenes.size());
	for (const auto& id : m_scenes | std::views::keys) {
		all_scenes.push_back(id);
	}
	return all_scenes;
}

std::vector<gse::id*> gse::scene_handler::get_active_scene_ids() const {
	std::vector<id*> active_scene_ids;
	active_scene_ids.reserve(m_scenes.size());
	for (const auto& id : m_scenes | std::views::keys) {
		if (m_scenes.at(id)->get_active()) {
			active_scene_ids.push_back(id);
		}
	}
	return active_scene_ids;
}

gse::scene* gse::scene_handler::get_scene(id* scene_id) const {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}

