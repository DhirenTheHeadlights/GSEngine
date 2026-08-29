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
		[[= networked]] bool ragdolling = false;
	};

	struct clip_layer {
		[[= networked]] resource::handle<clip_asset> clip{};
		float weight = 0.f;
	};

	struct clip_player_component {
		static constexpr std::size_t max_layers = 6;

		[[= networked]] std::array<clip_layer, max_layers> layers{};
		[[= networked]] std::uint32_t layer_count = 0;
		[[= networked]] float phase = 0.f;
		[[= networked]] velocity desired_speed;
		[[= networked]] bool playing = true;
		[[= networked]] bool apply_root_motion = false;
	};
}
