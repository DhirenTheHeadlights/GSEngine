export module gs:skybox_scene;

import :player;

import std;
import gse;

export namespace gs {
	class skybox_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override;

		auto render(
		) -> void override;
	};
}

auto gs::skybox_scene::initialize() -> void {
	const auto skybox_position = gse::vec3<gse::position>(0.f, 0.f, 0.f);
	const auto skybox_size = gse::vec3<gse::length>(20000.f, 20000.f, 20000.f);

	build("Skybox")
		.with<gse::physics::motion_component>({
			.current_position = skybox_position,
			.mass = gse::kilograms(1000.f),
			.affected_by_gravity = false,
			.position_locked = true,
		})
		.with<gse::physics::collision_component>({
			.bounding_box = { skybox_position, skybox_size },
		})
		.with<gse::render_component>({
			.models = {
				gse::procedural_model::box(
					gse::material{
						.base_color = gse::vec3f(0.5f, 0.5f, 0.5f),
						.roughness = 0.5f,
						.metallic = 0.0f,
					},
					skybox_size
				),
			},
		})
		.with<gse::directional_light_component>({
			.color = gse::vec3f(1.f),
			.intensity = 1.f,
			.direction = gse::vec3f(0.0f, -1.0f, 0.0f),
			.ambient_strength = 1.0f,
		})
		.configure([](gse::physics::collision_component& cc) {
			cc.resolve_collisions = false;
		});

	const auto floor_position = gse::vec3<gse::position>(0.f, -500.f, 0.f);
	const auto floor_size = gse::vec3<gse::length>(20000.f, 10.f, 20000.f);

	build("Skybox Floor")
		.with<gse::physics::motion_component>({
			.current_position = floor_position,
			.mass = gse::kilograms(1000.f),
			.affected_by_gravity = false,
			.position_locked = true,
		})
		.with<gse::physics::collision_component>({
			.bounding_box = { floor_position, floor_size },
		})
		.with<gse::render_component>({
			.models = {
				gse::procedural_model::box(
					gse::material{
						.base_color = gse::vec3f(0.5f, 0.5f, 0.5f),
						.roughness = 0.5f,
						.metallic = 0.0f,
					},
					floor_size
				),
			},
		})
		.configure([](gse::physics::collision_component& cc) {
			cc.resolve_collisions = false;
		});

	build("Player")
		.with<gs::player::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 0.f, 0.f),
		});
}

auto gs::skybox_scene::render() -> void {}
