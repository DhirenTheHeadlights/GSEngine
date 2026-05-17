export module gs:entity_builders;

import std;
import gse;

export namespace gs {
	struct box_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_box_spec spec;
	};

	struct sphere_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_sphere_spec spec;
	};

	struct sphere_light_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_sphere_spec spec;
		gse::point_light_component light;
	};

	auto box(
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		gse::mass m = gse::kilograms(1000.f),
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f),
		float roughness = 0.5f,
		float metallic = 0.0f
	) -> box_archetype;

	auto sphere(
		const gse::vec3<gse::position>& position,
		gse::length radius,
		gse::sphere_lod lod = gse::sphere_lod::mid
	) -> sphere_archetype;

	auto sphere_light(
		const gse::vec3<gse::position>& position,
		gse::length radius,
		gse::sphere_lod lod = gse::sphere_lod::lo
	) -> sphere_light_archetype;

	auto static_box(
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> box_archetype;
}

auto gs::box(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::mass m, const gse::quat& orientation, const float roughness, const float metallic) -> box_archetype {
	const gse::inertia box_inertia = m * gse::dot(size, size) / 18.f;

	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::dynamic_body{
				.mass = m,
				.moment_of_inertia = box_inertia,
			},
		},
		.collision = {
			.shape = gse::physics::box_shape{ .size = size },
		},
		.spec = {
			.material = {
				.base_color = gse::vec3f(gse::random_value(0.3f, 1.0f), gse::random_value(0.3f, 1.0f), gse::random_value(0.3f, 1.0f)),
				.roughness = roughness,
				.metallic = metallic,
			},
			.size = size,
		},
	};
}

auto gs::sphere(const gse::vec3<gse::position>& position, const gse::length radius, const gse::sphere_lod lod) -> sphere_archetype {
	return {
		.transform = {
			.position = position,
		},
		.motion = {
			.body = gse::physics::dynamic_body{
				.mass = gse::kilograms(100.f),
			},
		},
		.collision = {
			.shape = gse::physics::sphere_shape{ .radius = radius },
		},
		.spec = {
			.material = {
				.base_color = gse::vec3f(1.0f),
			},
			.lod = lod,
			.radius = radius,
		},
	};
}

auto gs::sphere_light(const gse::vec3<gse::position>& position, const gse::length radius, const gse::sphere_lod lod) -> sphere_light_archetype {
	return {
		.transform = {
			.position = position,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::sphere_shape{ .radius = radius },
		},
		.spec = {
			.material = {
				.base_color = gse::vec3f(1.0f),
			},
			.lod = lod,
			.radius = radius,
		},
		.light = {
			.color = gse::vec3f(1.f),
			.intensity = 78.5f,
			.position = position,
			.constant = 1.0f,
			.linear = 0.09f,
			.quadratic = 0.032f,
			.ambient_strength = 0.025f,
		},
	};
}

auto gs::static_box(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::quat& orientation) -> box_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::box_shape{ .size = size },
		},
		.spec = {
			.material = {
				.base_color = gse::vec3f(0.5f, 0.5f, 0.5f),
				.roughness = 0.8f,
				.metallic = 0.0f,
			},
			.size = size,
		},
	};
}
