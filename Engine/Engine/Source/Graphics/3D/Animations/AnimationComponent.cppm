export module gse.graphics:animation_component;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;
import :skeleton;
import :clip;

export namespace gse {
	struct animation_component {
		[[= networked]] id skeleton_id;

		resource::handle<skeleton> skeleton;
		std::vector<mat4f> local_pose;
		std::vector<mat4f> global_pose;
		std::vector<mat4f> skins;
		std::uint32_t skin_buffer_offset = 0;
	};
}
