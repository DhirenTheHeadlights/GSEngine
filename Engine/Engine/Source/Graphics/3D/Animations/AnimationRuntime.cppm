export module gse.graphics:animation_runtime;

import std;

import gse.utility;
import gse.physics.math;

import :animation_component;
import :clip_component;
import :skeleton;
import :clip;
import :renderer;

export namespace gse::animation {
    auto advance_clip(
        clip_component& c, float dt
    ) -> void;

    auto sample_clip_local(
        const clip_asset& clip,
        const skeleton& skel,
        time t,
        std::span<mat4> local_out
    ) -> void;

    auto build_global_pose(
        const skeleton& skel,
        std::span<const mat4> local_pose,
        std::span<mat4> global_out
    ) -> void;

    auto build_skin_matrices(
        const skeleton& skel,
        std::span<const mat4> global_pose,
        std::span<mat4> skins_out
    ) -> void;

    auto update_animations(
        registry& reg,
        float dt
    ) -> void;
}

namespace gse::animation {
    auto key_index_for_time(
        const std::vector<joint_keyframe>& keys,
        time t
    ) -> std::size_t;
}


