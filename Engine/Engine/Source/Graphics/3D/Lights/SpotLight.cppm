export module gse.graphics:spot_light;

import std;

import gse.physics.math;
import gse.utility;

import :gui;

export namespace gse {
	struct spot_light_data {
		unitless::vec3 color;
		float intensity = 1.0f;
		vec3<length> position;
		unitless::vec3 direction;
		float constant = 1.0f;
		float linear = 0.09f;
		float quadratic = 0.032f;
		angle cut_off;
		angle outer_cut_off;
		float ambient_strength = 0.025f;
		length near_plane = meters(0.1f);
		length far_plane = meters(10000.f);
		std::vector<id> ignore_list_ids;
	};

	struct spot_light_component : component<spot_light_data> {
		explicit spot_light_component(const id owner_id, const spot_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {
			gui::start(
				std::format("Spot light {}", name),
				[this] {
					gui::slider(
						"Intensity",
						intensity,
						0.f,
						1000.f
					);

					gui::slider(
						"Ambient Strength",
						ambient_strength,
						0.f,
						1.f
					);

					gui::slider(
						"Linear",
						linear,
						0.f,
						1.f
					);

					gui::slider(
						"Quadratic",
						quadratic,
						0.f,
						1.f
					);

					gui::slider(
						"Cut Off",
						cut_off,
						degrees(0.f),
						degrees(90.f)
					);

					gui::slider(
						"Outer Cut Off",
						outer_cut_off,
						degrees(0.f),
						degrees(90.f)
					);

					gui::slider(
						"Ambient",
						ambient_strength,
						0.f,
						1.f
					);

					gui::slider(
						"Direction",
						direction,
						{ -1.f, -1.f, -1.f },
						{ 1.f, 1.f, 1.f }
					);

					gui::slider(
						"Position",
						position,
						{ -500.f, -500.f, -500.f },
						{ 500.f, 500.f, 500.f }
					);

					gui::slider(
						"Color",
						color,
						{ 0.f, 0.f, 0.f },
						{ 1.f, 1.f, 1.f }
					);

					gui::slider(
						"Near Plane",
						near_plane,
						meters(0.1f),
						meters(100.f)
					);

					gui::slider(
						"Far Plane",
						far_plane,
						meters(100.f),
						meters(10000.f)
					);
				}
			);
		}
	};
}