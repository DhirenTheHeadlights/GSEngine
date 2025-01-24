export module gse.physics.narrow_phase_collisions;

import gse.physics.motion_component;
import gse.physics.bounding_box;
import gse.physics.collision_component;

export namespace gse::narrow_phase_collision {
	auto resolve_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, const physics::collision_component& other_collision_component) -> void;
}