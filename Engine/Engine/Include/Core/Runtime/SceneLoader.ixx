export module gse.runtime:scene_loader;

import std;

import :scene;

import gse.assert;
import gse.utility;

export namespace gse {
	class scene_loader final : public non_copyable {
	public:
		auto add(std::unique_ptr<scene>& scene) -> void;
		auto remove(const id& scene_id) -> void;

		auto activate(const id& scene_id) -> void;
		auto deactivate(const id& scene_id) -> void;

		auto update() -> void;
		auto render() -> void;

		auto queue_trigger(const id& id, const std::function<bool()>& trigger) -> void;

		auto active_scenes() -> std::vector<scene*>;
		auto scene(const id& scene_id) -> scene*;
	private:
		std::unordered_map<id, std::unique_ptr<gse::scene>> m_scenes;
		std::unordered_map<id, std::function<bool()>> m_scene_triggers;
	};
}

auto gse::scene_loader::add(std::unique_ptr<gse::scene>& scene) -> void {
	assert(scene.get(), "Cannot add a null scene to the scene loader");
	m_scenes.insert({ scene->id(), std::move(scene) });
}

auto gse::scene_loader::remove(const id& scene_id) -> void {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (scene->second->active()) {
			scene->second->exit();
		}
		m_scenes.erase(scene);
	}
}

auto gse::scene_loader::activate(const id& scene_id) -> void {
	assert(!m_scenes.empty(), "No scenes available to activate");
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (!scene->second->active()) {
			scene->second->initialize();
			scene->second->set_active(true);
		}
	}
}

auto gse::scene_loader::deactivate(const id& scene_id) -> void {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		if (scene->second->active()) {
			scene->second->exit();
			scene->second->set_active(false);
		}
	}
}

auto gse::scene_loader::update() -> void {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->active()) {
			scene->update();
		}
	}
	for (const auto& [id, trigger] : m_scene_triggers) {
		if (trigger()) {
			activate(id);
		}
	}
}

auto gse::scene_loader::render() -> void {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->active()) {
			scene->render();
		}
	}
}

auto gse::scene_loader::queue_trigger(const id& id, const std::function<bool()>& trigger) -> void {
	m_scene_triggers.insert({ id, trigger });
}

auto gse::scene_loader::active_scenes() -> std::vector<gse::scene*> {
	std::vector<gse::scene*> active_scenes;
	active_scenes.reserve(m_scenes.size());
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->active()) {
			active_scenes.push_back(scene.get());
		}
	}
	return active_scenes;
}

auto gse::scene_loader::scene(const id& scene_id) -> gse::scene* {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}
