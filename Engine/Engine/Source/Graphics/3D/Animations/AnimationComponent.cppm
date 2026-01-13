export module gse.graphics:animation_component;

import std;

import gse.utility;

import :skeleton;
import :clip;
import :resource_loader;

export namespace gse {
	struct animation_component_data {
		id skeleton_id;
	};

	struct animation_component : component<animation_component_data, animation_component_data> {
		animation_component(const id owner_id, const params& p) : component(owner_id, p) {}

		resource::handle<skeleton> skeleton;
		std::vector<mat4> local_pose;
		std::vector<mat4> global_pose;
		std::vector<mat4> skins;
	};
}
