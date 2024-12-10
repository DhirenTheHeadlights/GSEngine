#include "Core/SceneHandler.h"

#include <ranges>

void gse::scene_handler::add_scene(const std::shared_ptr<scene>& scene, const std::string& tag) {
	if (!scene->get_id()) {
		scene->set_id(generate_id(tag));
	}

	m_scenes.insert({ scene->get_id(), scene });
}

void gse::scene_handler::remove_scene(const std::shared_ptr<id>& scene_id) {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (scene->second->get_active()) {
			scene->second->exit();
		}
		m_scenes.erase(scene);
	}
}

void gse::scene_handler::activate_scene(const std::shared_ptr<id>& scene_id) {
	permaAssertComment(m_engine_initialized, "You are trying to activate a scene before the engine is initialized");
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (!scene->second->get_active()) {
			scene->second->initialize();
			scene->second->set_active(true);
		}
	}
}

void gse::scene_handler::deactivate_scene(const std::shared_ptr<id>& scene_id) {
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

void gse::scene_handler::queue_scene_trigger(const std::shared_ptr<id>& id, const std::function<bool()>& trigger) {
	m_scene_triggers.insert({ id, trigger });
}

std::vector<std::shared_ptr<gse::scene>> gse::scene_handler::get_active_scenes() const {
	std::vector<std::shared_ptr<scene>> activeScenes;
	activeScenes.reserve(m_scenes.size());
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->get_active()) {
			activeScenes.push_back(scene);
		}
	}
	return activeScenes;
}

std::vector<std::shared_ptr<gse::id>> gse::scene_handler::get_all_scenes() const {
	std::vector<std::shared_ptr<id>> allScenes;
	allScenes.reserve(m_scenes.size());
	for (const auto& id : m_scenes | std::views::keys) {
		allScenes.push_back(id);
	}
	return allScenes;
}

std::vector<std::shared_ptr<gse::id>> gse::scene_handler::get_active_scene_ids() const {
	std::vector<std::shared_ptr<id>> activeSceneIds;
	activeSceneIds.reserve(m_scenes.size());
	for (const auto& id : m_scenes | std::views::keys) {
		if (m_scenes.at(id)->get_active()) {
			activeSceneIds.push_back(id);
		}
	}
	return activeSceneIds;
}

std::shared_ptr<gse::scene> gse::scene_handler::get_scene(const std::shared_ptr<id>& scene_id) const {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		return scene->second;
	}
	return nullptr;
}

