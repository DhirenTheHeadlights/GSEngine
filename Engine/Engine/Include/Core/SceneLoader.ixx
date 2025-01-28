export module gse.core.scene_loader;

import std;

import gse.core.id;
import gse.core.scene;

export namespace gse::scene_loader {
	auto add_scene(std::unique_ptr<scene>& scene) -> void;
	auto remove_scene(id* scene_id) -> void;

	auto activate_scene(id* scene_id) -> void;
	auto deactivate_scene(id* scene_id) -> void;
	auto update() -> void;
	auto render() -> void;
	auto exit() -> void;

	auto set_engine_initialized(bool initialized) -> void;
	auto set_allow_multiple_active_scenes(bool allow) -> void;
	auto queue_scene_trigger(id* id, const std::function<bool()>& trigger) -> void;

	auto get_active_scenes() -> std::vector<scene*>;
	auto get_all_scenes() -> std::vector<id*>;
	auto get_active_scene_ids() -> std::vector<id*>;
	auto get_scene(id* scene_id) -> scene*;
}