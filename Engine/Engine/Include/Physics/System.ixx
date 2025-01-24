export module gse.physics.system;

import std;

import gse.physics.math.vector;
import gse.physics.math.units;
import gse.physics.motion_component;

export namespace gse::physics {
	auto apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position = { 0.f, 0.f, 0.f }) -> void;
	auto apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void;
	auto update_object(motion_component& component) -> void;
	auto update() -> void;
}
