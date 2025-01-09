#pragma once

#include "Core/Scene.h"

namespace gse::scene_loader {
	void add_scene(std::unique_ptr<scene>& scene);
	void remove_scene(id* scene_id);

	void activate_scene(id* scene_id);
	void deactivate_scene(id* scene_id);
	void update();
	void render();
	void exit();

	void set_engine_initialized(bool initialized);
	void set_allow_multiple_active_scenes(bool allow);
	void queue_scene_trigger(id* id, const std::function<bool()>& trigger);

	std::vector<scene*> get_active_scenes();
	std::vector<id*> get_all_scenes();
	std::vector<id*> get_active_scene_ids();
	scene* get_scene(id* scene_id);
}