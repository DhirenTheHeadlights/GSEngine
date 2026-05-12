export module gs:entity_builders;

import std;
import gse;

export namespace gs {
	auto build_box(
		gse::asset::state& assets,
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
		gse::asset::state& assets,
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		gse::length radius,
		int sectors = 24,
		int stacks = 16
	) -> gse::scene::builder;

	auto build_sphere_light(
		gse::asset::state& assets,
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		gse::length radius,
		int sectors = 18,
		int stacks = 10
	) -> void;

	auto build_static_box(
		gse::asset::state& assets,
		gse::scene* scene,
		const std::string& name,
		const gse::vec3<gse::position>& position,
		const gse::vec3<gse::length>& size,
		const gse::quat& orientation = gse::quat(1.f, 0.f, 0.f, 0.f)
	) -> gse::scene::builder;
}

auto gs::build_box(gse::asset::state& assets, gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::mass m, const gse::quat& orientation, const float roughness, const float metallic) -> gse::scene::builder {
	const gse::inertia box_inertia = m * gse::dot(size, size) / 18.f;
	const gse::material mat{
		.base_color = gse::vec3f(gse::random_value(0.3f, 1.0f), gse::random_value(0.3f, 1.0f), gse::random_value(0.3f, 1.0f)),
		.roughness = roughness,
		.metallic = metallic,
	};

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
		.with<gse::render_component>({
			.models = {
				gse::procedural_model::box(assets, mat, size),
			},
			.model_count = 1,
		});
}

auto gs::build_sphere(gse::asset::state& assets, gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::length radius, const int sectors, const int stacks) -> gse::scene::builder {
	const float diameter = radius.as<gse::meters>() * 2.f;

	return scene->build(name)
		.with<gse::physics::transform_component>({
			.position = position,
			.scale = { diameter, diameter, diameter },
		})
		.with<gse::physics::motion_component>({
			.body = gse::physics::dynamic_body{
				.mass = gse::kilograms(100.f),
			},
		})
		.with<gse::physics::collision_component>({
			.shape = gse::physics::sphere_shape{ .radius = radius },
		})
		.with<gse::render_component>({
			.models = {
				gse::procedural_model::sphere(
					assets,
					gse::material{
						.diffuse_texture = gse::asset::get<gse::texture>(assets, "Textures/Textures/sun"),
					},
					sectors,
					stacks
				),
			},
			.model_count = 1,
		});
}

auto gs::build_sphere_light(gse::asset::state& assets, gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::length radius, const int sectors, const int stacks) -> void {
	build_sphere(assets, scene, name, position, radius, sectors, stacks)
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

auto gs::build_static_box(gse::asset::state& assets, gse::scene* scene, const std::string& name, const gse::vec3<gse::position>& position, const gse::vec3<gse::length>& size, const gse::quat& orientation) -> gse::scene::builder {
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
		.with<gse::render_component>({
			.models = {
				gse::procedural_model::box(
					assets,
					gse::material{
						.base_color = gse::vec3f(0.5f, 0.5f, 0.5f),
						.roughness = 0.8f,
						.metallic = 0.0f,
					},
					size
				),
			},
			.model_count = 1,
		});
}
