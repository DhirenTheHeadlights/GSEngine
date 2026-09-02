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
		gse::sphere_lod lod = gse::sphere_lod::mid
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

	struct cylinder_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_cylinder_spec spec;
	};

	auto static_disc_floor(
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& collider_size,
		gse::length visual_radius,
		const gse::vec3f& base_color = gse::vec3f(0.08f, 0.08f, 0.09f),
		float roughness = 0.85f,
		float metallic = 0.0f
	) -> cylinder_archetype;

	struct scatter_rock_archetype {
		gse::physics::transform_component transform;
		gse::primitive_box_spec spec;
	};

	auto scatter_rock(
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		const gse::quat& orientation,
		const gse::vec3f& base_color = gse::vec3f(0.13f, 0.13f, 0.15f),
		float roughness = 0.95f
	) -> scatter_rock_archetype;

	struct mountain_ring_archetype {
		gse::physics::transform_component transform;
		gse::primitive_mountain_ring_spec spec;
	};

	auto mountain_ring(
		gse::length inner_radius,
		gse::length outer_radius,
		gse::length peak_height,
		const gse::vec3f& base_color = gse::vec3f(0.21f, 0.23f, 0.27f),
		float roughness = 0.92f,
		std::uint32_t seed = 1337
	) -> mountain_ring_archetype;

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
			.constant = 1.0f,
			.linear = gse::per_meter(0.09f),
			.quadratic = gse::per_meter(0.032f) * gse::per_meter(1.f),
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

auto sandbox::scatter_rock(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::quat& orientation, const gse::vec3f& base_color, const float roughness) -> scatter_rock_archetype {
	return {
		.transform = {
			.position = position,
			.orientation = orientation,
		},
		.spec = {
			.material = {
				.base_color = base_color,
				.roughness = roughness,
				.metallic = 0.0f,
			},
			.size = size,
		},
	};
}

auto sandbox::static_disc_floor(const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& collider_size, const gse::length visual_radius, const gse::vec3f& base_color, const float roughness, const float metallic) -> cylinder_archetype {
	return {
		.transform = {
			.position = position,
		},
		.motion = {
			.body = gse::physics::static_body{},
		},
		.collision = {
			.shape = gse::physics::box_shape{
				.size = collider_size
			},
		},
		.spec = {
			.material = {
				.base_color = base_color,
				.roughness = roughness,
				.metallic = metallic,
			},
			.radius = visual_radius,
			.height = collider_size.y(),
		},
	};
}

auto sandbox::mountain_ring(const gse::length inner_radius, const gse::length outer_radius, const gse::length peak_height, const gse::vec3f& base_color, const float roughness, const std::uint32_t seed) -> mountain_ring_archetype {
	return {
		.transform = {
			.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(0.f), gse::meters(0.f)),
		},
		.spec = {
			.material = {
				.base_color = base_color,
				.roughness = roughness,
				.metallic = 0.0f,
			},
			.inner_radius = inner_radius,
			.outer_radius = outer_radius,
			.peak_height = peak_height,
			.seed = seed,
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
