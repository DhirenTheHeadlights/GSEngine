export module gse.graphics:animation_components;

import std;

import :clip;
import :skinned_model;

import gse.core;
import gse.ecs;
import gse.math;
import gse.time;
import gse.assets;

export namespace gse {
	struct skeleton_instance_component {
		static constexpr std::size_t max_bones = 32;

		[[= networked]] resource::handle<skinned_model> model{};
		[[= networked]] id proxy;
		[[= networked]] std::array<id, max_bones> bones{};
		[[= networked]] std::uint32_t bone_count = 0;
	};

	struct clip_player_component {
		[[= networked]] resource::handle<clip_asset> clip{};
		[[= networked]] time elapsed;
		[[= networked]] float speed = 1.f;
		[[= networked]] bool playing = true;
	};
}
