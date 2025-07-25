export module gse.runtime:scene_loader;

import std;

import :main_clock;

import gse.assert;
import gse.utility;
import gse.physics;
import gse.graphics;

export namespace gse::scene_loader {
	auto add(std::unique_ptr<scene>& new_scene) -> void;
	auto remove(const id& scene_id) -> void;

	auto activate(const id& scene_id) -> void;
	auto deactivate(const id& scene_id) -> void;

	auto initialize() -> void;
	auto update() -> void;
	auto render() -> void;

	auto queue(const id& id, const std::function<bool()>& trigger) -> void;

	auto active_scenes() -> std::vector<scene*>;
	auto scene(const id& scene_id) -> scene*;
}

namespace gse::scene_loader {
	std::unordered_map<id, std::unique_ptr<gse::scene>> scenes;
	std::unordered_map<id, std::function<bool()>> scene_triggers;

	auto registries() -> std::vector<std::reference_wrapper<registry>>;
}

auto gse::scene_loader::add(std::unique_ptr<gse::scene>& new_scene) -> void {
	scenes.insert({ new_scene->id(), std::move(new_scene) });
}

auto gse::scene_loader::remove(const id& scene_id) -> void {
	if (const auto scene = scenes.find(scene_id); scene != scenes.end()) {
		scenes.erase(scene);
	}
}

auto gse::scene_loader::activate(const id& scene_id) -> void {
	if (const auto scene = scenes.find(scene_id); scene != scenes.end()) {
		if (!scene->second->active()) {
			scene->second->add_hook(std::make_unique<default_scene>(scene->second.get()));
			scene->second->initialize();
			scene->second->set_active(true);
		}
	}
}

auto gse::scene_loader::deactivate(const id& scene_id) -> void {
	if (const auto scene = scenes.find(scene_id); scene != scenes.end()) {
		if (scene->second->active()) {
			scene->second->set_active(false);
		}
	}
}

auto gse::scene_loader::initialize() -> void {
	renderer::initialize();
}

auto gse::scene_loader::update() -> void {
	for (const auto& scene : scenes | std::views::values) {
		if (scene->active()) {
			scene->update();
		}
	}

	for (const auto& [id, trigger] : scene_triggers) {
		if (trigger()) {
			activate(id);
		}
	}

	physics::update(
		registries(),
		main_clock::const_update_time,
		main_clock::dt()
	);
}

auto gse::scene_loader::render() -> void {
	renderer::render(
		registries(),
		[] {
			for (const auto& scene : scenes | std::views::values) {
				if (scene->active()) {
					scene->render();
				}
			}
		}
	);
}

auto gse::scene_loader::queue(const id& id, const std::function<bool()>& trigger) -> void {
	scene_triggers.insert({ id, trigger });
}

auto gse::scene_loader::active_scenes() -> std::vector<gse::scene*> {
	std::vector<gse::scene*> active_scenes;
	std::ranges::transform(
		scenes, 
		std::back_inserter(active_scenes),
		[](const auto& pair) {
			return pair.second.get();
		}
	);
	return active_scenes;
}

auto gse::scene_loader::scene(const id& scene_id) -> gse::scene* {
	if (const auto scene = scenes.find(scene_id); scene != scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}

auto gse::scene_loader::registries() -> std::vector<std::reference_wrapper<registry>> {
	std::vector<std::reference_wrapper<registry>> active_registries;
	for (const auto& scene : scenes | std::views::values) {
		if (scene->active()) {
			active_registries.push_back(std::ref(scene->registry()));
		}
	}
	return active_registries;
}
