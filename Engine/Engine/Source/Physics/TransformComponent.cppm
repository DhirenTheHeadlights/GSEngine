export module gse.physics:transform_component;

import std;

import gse.core;
import gse.containers;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct transform_component {
		[[= networked]] vec3<current_position> position;
		[[= networked]] quat orientation = quat(1.f, 0.f, 0.f, 0.f);
	};

	auto transformation_matrix(
		const transform_component& tc
	) -> mat4f;
}

auto gse::physics::transformation_matrix(const transform_component& tc) -> mat4f {
	const mat4f translation = translate(mat4f(1.0f), tc.position);
	const mat4f rotation = mat4f(mat3_cast(tc.orientation));
	return translation * rotation;
}
