export module gs:entity_builders;

import std;
import gse;

export namespace gs {
	auto build_box(
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		gse::mass m = gse::kilograms(1000.f),
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f),
		float roughness = 0.5f,
		float metallic = 0.0f
	) -> gse::scene::builder;

	auto build_sphere(
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		gse::length radius,
		gse::sphere_lod lod = gse::sphere_lod::mid
	) -> gse::scene::builder;

	auto build_sphere_light(
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		gse::length radius,
		gse::sphere_lod lod = gse::sphere_lod::lo
	) -> void;

	auto build_static_box(
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> gse::scene::builder;
}

auto gs::build_box(gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::mass m, const gse::quat& orientation, const float roughness, const float metallic) -> gse::scene::builder {
	const gse::inertia box_inertia = m * gse::dot(size, size) / 18.f;

	return scene->build(name)
		.with<gse::physics::transform_component>({
			.position = position,
			.orientation = orientation,
		})
		.with<gse::physics::motion_component>({
			.body = gse::physics::dynamic_body{
				.mass = m,
				.moment_of_inertia = box_inertia,
			},
		})
		.with<gse::physics::collision_component>({
			.shape = gse::physics::box_shape{ .size = size },
		})
		.with<gse::primitive_box_spec>({
			.material = {
				.base_color = gse::vec3f(gse::random_value(0.3f, 1.0f), gse::random_value(0.3f, 1.0f), gse::random_value(0.3f, 1.0f)),
				.roughness = roughness,
				.metallic = metallic,
			},
			.size = size,
		});
}

auto gs::build_sphere(gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::length radius, const gse::sphere_lod lod) -> gse::scene::builder {
	return scene->build(name)
		.with<gse::physics::transform_component>({
			.position = position,
		})
		.with<gse::physics::motion_component>({
			.body = gse::physics::dynamic_body{
				.mass = gse::kilograms(100.f),
			},
		})
		.with<gse::physics::collision_component>({
			.shape = gse::physics::sphere_shape{ .radius = radius },
		})
		.with<gse::primitive_sphere_spec>({
			.material = {
				.base_color = gse::vec3f(1.0f),
			},
			.lod = lod,
			.radius = radius,
		});
}

auto gs::build_sphere_light(gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::length radius, const gse::sphere_lod lod) -> void {
	build_sphere(scene, name, position, radius, lod)
		.with<gse::point_light_component>({
			.color = gse::vec3f(1.f),
			.intensity = 78.5f,
			.position = position,
			.constant = 1.0f,
			.linear = 0.09f,
			.quadratic = 0.032f,
			.ambient_strength = 0.025f,
		})
		.configure([](gse::physics::motion_component& mc) {
			mc.body = gse::physics::static_body{};
		});
}

auto gs::build_static_box(gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::quat& orientation) -> gse::scene::builder {
	return scene->build(name)
		.with<gse::physics::transform_component>({
			.position = position,
			.orientation = orientation,
		})
		.with<gse::physics::motion_component>({
			.body = gse::physics::static_body{},
		})
		.with<gse::physics::collision_component>({
			.shape = gse::physics::box_shape{ .size = size },
		})
		.with<gse::primitive_box_spec>({
			.material = {
				.base_color = gse::vec3f(0.5f, 0.5f, 0.5f),
				.roughness = 0.8f,
				.metallic = 0.0f,
			},
			.size = size,
		});
}
