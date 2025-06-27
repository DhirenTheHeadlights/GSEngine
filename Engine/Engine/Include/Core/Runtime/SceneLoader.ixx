export module gse.runtime:scene_loader;

import std;

import :scene;

import gse.assert;
import gse.utility;

export namespace gse::scene_loader {
	auto add_scene(std::unique_ptr<scene>& scene) -> void;
	auto remove_scene(const id& scene_id) -> void;

	auto activate_scene(const id& scene_id) -> void;
	auto deactivate_scene(const id& scene_id) -> void;
	auto update() -> void;
	auto render() -> void;
	auto exit() -> void;

	auto set_engine_initialized(bool initialized) -> void;
	auto set_allow_multiple_active_scenes(bool allow) -> void;
	auto queue_scene_trigger(gse::id id, const std::function<bool()>& trigger) -> void;

	auto active_scenes() -> std::vector<scene*>;
	auto all_scenes() -> std::vector<id>;
	auto active_scene_ids() -> std::vector<id>;
	auto scene(const id& scene_id) -> scene*;
}

std::optional<std::uint32_t> g_fbo = std::nullopt;
bool g_engine_initialized = false;
bool g_allow_multiple_active_scenes = false;

std::unordered_map<gse::id, std::unique_ptr<gse::scene>> g_scenes;
std::unordered_map<gse::id, std::function<bool()>> g_scene_triggers;

auto gse::scene_loader::add_scene(std::unique_ptr<class scene>& scene) -> void {
	g_scenes.insert({ scene->id(), std::move(scene) });
}

auto gse::scene_loader::remove_scene(const id& scene_id) -> void {
	if (const auto scene = g_scenes.find(scene_id); scene != g_scenes.end()) {
		if (scene->second->active()) {
			scene->second->exit();
		}
		g_scenes.erase(scene);
	}
}

auto gse::scene_loader::activate_scene(const id& scene_id) -> void {
	assert(g_engine_initialized, "You are trying to activate a scene before the engine is initialized");

	if (!g_allow_multiple_active_scenes) {
		for (const auto& scene : g_scenes | std::views::values) {
			if (scene->active()) {
				scene->exit();
				scene->set_active(false);
			}
		}
	}

	if (const auto scene = g_scenes.find(scene_id); scene != g_scenes.end()) {
		if (!scene->second->active()) {
			scene->second->initialize();
			scene->second->set_active(true);
		}
	}
}

auto gse::scene_loader::deactivate_scene(const id& scene_id) -> void {
	if (const auto scene = g_scenes.find(scene_id); scene != g_scenes.end()) {
		if (scene->second->active()) {
			scene->second->exit();
			scene->second->set_active(false);
		}
	}
}

auto gse::scene_loader::update() -> void {
	for (const auto& scene : g_scenes | std::views::values) {
		if (scene->active()) {
			scene->update();
		}
	}

	for (const auto& [id, trigger] : g_scene_triggers) {
		if (trigger()) {
			activate_scene(id);
		}
	}
}

auto gse::scene_loader::render() -> void {
	for (const auto& scene : g_scenes | std::views::values) {
		if (scene->active()) {
			scene->render();
		}
	}
}

auto gse::scene_loader::exit() -> void {
	for (const auto& scene : g_scenes | std::views::values) {
		if (scene->active()) {
			scene->exit();
		}
	}
}

auto gse::scene_loader::set_engine_initialized(const bool initialized) -> void {
	g_engine_initialized = initialized;
}

auto gse::scene_loader::set_allow_multiple_active_scenes(const bool allow) -> void {
	g_allow_multiple_active_scenes = allow;
}

auto gse::scene_loader::queue_scene_trigger(id id, const std::function<bool()>& trigger) -> void {
	g_scene_triggers.insert({ id, trigger });
}

auto gse::scene_loader::active_scenes() -> std::vector<class scene*> {
	std::vector<class scene*> active_scenes;
	active_scenes.reserve(g_scenes.size());
	for (const auto& scene : g_scenes | std::views::values) {
		if (scene->active()) {
			active_scenes.push_back(scene.get());
		}
	}
	return active_scenes;
}

auto gse::scene_loader::all_scenes() -> std::vector<id> {
	std::vector<id> all_scenes;
	all_scenes.reserve(g_scenes.size());
	for (const auto& id : g_scenes | std::views::keys) {
		all_scenes.push_back(id);
	}
	return all_scenes;
}

auto gse::scene_loader::active_scene_ids() -> std::vector<id> {
	std::vector<id> active_scene_ids;
	active_scene_ids.reserve(g_scenes.size());
	for (const auto& id : g_scenes | std::views::keys) {
		if (g_scenes.at(id)->active()) {
			active_scene_ids.push_back(id);
		}
	}
	return active_scene_ids;
}

auto gse::scene_loader::scene(const id& scene_id) -> class scene* {
	if (const auto scene = g_scenes.find(scene_id); scene != g_scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}

