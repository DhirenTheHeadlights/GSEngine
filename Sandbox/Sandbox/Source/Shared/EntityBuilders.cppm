export module sandbox:entity_builders;

import std;
import gse;

export namespace sandbox {
	struct box_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_box_spec spec;
	};

	struct collider_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
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
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f),
		const gse::vec3f& base_color = gse::vec3f(0.5f, 0.5f, 0.5f),
		float roughness = 0.8f,
		float metallic = 0.0f
	) -> box_archetype;

	auto static_collider(
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> collider_archetype;

	auto capsule(
		const gse::vec3<gse::position>& position,
		gse::length radius,
		gse::length half_height,
		gse::mass m = gse::kilograms(1000.f),
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> collider_archetype;

	auto static_sphere(
		const gse::vec3<gse::position>& position,
		gse::length radius
	) -> collider_archetype;

	auto static_capsule(
		const gse::vec3<gse::position>& position,
		gse::length radius,
		gse::length half_height,
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> collider_archetype;

	struct hull_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::physics::hull_definition hull;
	};

	auto box_as_hull(
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		gse::mass m = gse::kilograms(1000.f),
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> hull_archetype;
}

auto sandbox::box(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::mass m, const gse::quat& orientation, const float roughness, const float metallic) -> box_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::dynamic_body{
				.mass = m,
			},
		},
		.collision = {
			.shape = gse::physics::box_shape{
				.size = size
			},
		},
		.spec = {
			.material = {
				.base_color = gse::vec3f(
					gse::random_value(0.3f, 1.0f),
					gse::random_value(0.3f, 1.0f),
					gse::random_value(0.3f, 1.0f)
				),
				.roughness = roughness,
				.metallic = metallic,
			},
			.size = size,
		},
	};
}

auto sandbox::sphere(const gse::vec3<gse::position>& position, const gse::length radius, const gse::sphere_lod lod) -> sphere_archetype {
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
			.shape = gse::physics::sphere_shape{
				.radius = radius
			},
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

auto sandbox::sphere_light(const gse::vec3<gse::position>& position, const gse::length radius, const gse::sphere_lod lod) -> sphere_light_archetype {
	return {
		.transform = {
			.position = position,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::sphere_shape{
				.radius = radius
			},
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
			.intensity = gse::watts_per_square_meter(78.5f),
			.position = position,
			.constant = 1.0f,
			.linear = gse::per_meter(0.09f),
			.quadratic = 0.032f,
			.ambient_strength = 0.025f,
		},
	};
}

auto sandbox::static_box(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::quat& orientation, const gse::vec3f& base_color, const float roughness, const float metallic) -> box_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::box_shape{
				.size = size
			},
		},
		.spec = {
			.material = {
				.base_color = base_color,
				.roughness = roughness,
				.metallic = metallic,
			},
			.size = size,
		},
	};
}

auto sandbox::static_collider(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::quat& orientation) -> collider_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::box_shape{
				.size = size
			},
		},
	};
}

auto sandbox::capsule(const gse::vec3<gse::position>& position, const gse::length radius, const gse::length half_height, const gse::mass m, const gse::quat& orientation) -> collider_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::dynamic_body{
				.mass = m,
			},
		},
		.collision = {
			.shape = gse::physics::capsule_shape{
				.radius = radius,
				.half_height = half_height,
			},
		},
	};
}

auto sandbox::static_sphere(const gse::vec3<gse::position>& position, const gse::length radius) -> collider_archetype {
	return {
		.transform = {
			.position = position,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::sphere_shape{
				.radius = radius
			},
		},
	};
}

auto sandbox::box_as_hull(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::mass m, const gse::quat& orientation) -> hull_archetype {
	const auto half = size * 0.5f;

	std::vector<gse::vec3<gse::length>> corners;
	corners.reserve(8);
	for (int i = 0; i < 8; ++i) {
		corners.emplace_back(
			(i & 1) ? half.x() : -half.x(),
			(i & 2) ? half.y() : -half.y(),
			(i & 4) ? half.z() : -half.z()
		);
	}

	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::dynamic_body{
				.mass = m,
			},
		},
		.collision = {
			.shape = gse::physics::box_shape{
				.size = size
			},
		},
		.hull = {
			.points = std::move(corners),
		},
	};
}

auto sandbox::static_capsule(const gse::vec3<gse::position>& position, const gse::length radius, const gse::length half_height, const gse::quat& orientation) -> collider_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::capsule_shape{
				.radius = radius,
				.half_height = half_height,
			},
		},
	};
}
