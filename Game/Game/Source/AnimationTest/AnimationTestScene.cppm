export module gs:animation_test_scene;

import std;
import gse;

export namespace gs {
	class animation_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			build("Player")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(0.f, 0.f, 0.f)
				});

			build("Sun")
				.with<sun_light>({});

			// Create a grid of animated characters (10x10 = 100 characters)
			constexpr int grid_x = 10;
			constexpr int grid_z = 10;
			constexpr float spacing = 2.5f;

			const float start_x = -static_cast<float>(grid_x - 1) * spacing * 0.5f;
			const float start_z = -static_cast<float>(grid_z - 1) * spacing * 0.5f;

			for (int z = 0; z < grid_z; ++z) {
				for (int x = 0; x < grid_x; ++x) {
					const float px = start_x + static_cast<float>(x) * spacing;
					const float pz = start_z + static_cast<float>(z) * spacing;

					build(std::format("Character_{}_{}", x, z))
						.with<animated_character>({
							.position = gse::vec3<gse::length>(px, 0.f, pz),
							.skeleton_name = "character",
							.clip_name = "mixamo",
							.model_name = "character"
						});
				}
			}

			build("Floor")
				.with<static_floor>({
					.position = gse::vec3<gse::length>(0.f, -0.25f, 0.f),
					.size = gse::vec3<gse::length>(50.f, 0.5f, 50.f)
				});
		}

	private:
		struct sun_light final : hook<gse::entity> {
			struct params {};

			explicit sun_light(const params&) {}

			auto initialize() -> void override {
				add_component<gse::directional_light_component>({
					.color = gse::unitless::vec3(1.f, 0.95f, 0.9f),
					.intensity = 1.5f,
					.direction = gse::unitless::vec3(-0.5f, -1.f, -0.3f),
					.ambient_strength = 0.15f
				});
			}
		};

		struct static_floor final : hook<gse::entity> {
			struct params {
				gse::vec3<gse::length> position;
				gse::vec3<gse::length> size;
			};

			explicit static_floor(const params& p) : m_position(p.position), m_size(p.size) {}

			gse::vec3<gse::length> m_position;
			gse::vec3<gse::length> m_size;

			auto initialize() -> void override {
				add_component<gse::physics::motion_component>({
					.current_position = m_position,
					.affected_by_gravity = false,
					.position_locked = true
				});

				add_component<gse::physics::collision_component>({
					.bounding_box = { m_position, m_size }
				});

				const auto mat = gse::queue<gse::material>(
					"floor_material",
					gse::get<gse::texture>("concrete_bricks_1")
				);

				add_component<gse::render_component>({
					.models = { gse::procedural_model::box(mat) }
				});
			}
		};

		struct animated_character final : hook<gse::entity> {
			struct params {
				gse::vec3<gse::length> position;
				std::string skeleton_name;
				std::string clip_name;
				std::string model_name;
			};

			animated_character(const params& p)
				: position(p.position)
				, skeleton_name(p.skeleton_name)
				, clip_name(p.clip_name)
				, model_name(p.model_name) {}

			gse::vec3<gse::length> position;
			std::string skeleton_name;
			std::string clip_name;
			std::string model_name;

			auto initialize() -> void override {
				add_component<gse::physics::motion_component>({
					.current_position = position,
					.affected_by_gravity = false,
					.position_locked = true
				});

				add_component<gse::physics::collision_component>({
					.bounding_box = {
						gse::vec3<gse::length>(-0.5f, -0.5f, -0.5f),
						gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)
					}
				});

				add_component<gse::render_component>({
					.skinned_models = {
						gse::get<gse::skinned_model>(model_name)
					}
				});

				add_component<gse::animation_component>({
					.skeleton_id = gse::find(skeleton_name)
				});

				add_component<gse::clip_component>({
					.clip_id = gse::find(clip_name),
					.scale = 1.f,
					.loop = true
				});
			}

			auto update() -> void override {
				component_write<gse::physics::motion_component>().current_position = position;
			}
		};
	};
}
