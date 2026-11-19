export module sandbox:stacks;

import std;
import gse;

import :archetypes;
import :spawn_params;

export namespace sandbox {
	auto spawn_pyramid(
		gse::scene& s,
		const pyramid_scene_params& params = {}
	) -> void;

	auto spawn_inverted_mass_pyramid(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_box_grid(
		gse::scene& s,
		const gse::vec3<gse::position>& origin,
		const stress_scene_params& params
	) -> void;

	auto spawn_domino_chain(
		gse::scene& s,
		int index,
		const gse::vec3<gse::position>& origin,
		gse::angle heading = gse::degrees(0.f)
	) -> void;

	auto spawn_sphere_stack(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_corner_drop(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_slope_friction_test(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_high_speed_impact_target(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;
}

auto sandbox::spawn_pyramid(gse::scene& s, const pyramid_scene_params& params) -> void {
	const int base_count = params.base_count;
	const gse::length extent = gse::meters(1.f);

	for (int row = 0; row < base_count; ++row) {
		const int count = base_count - row;
		const gse::length py = extent * (static_cast<float>(row) + 0.5f);

		for (int col = 0; col < count; ++col) {
			const gse::length px = extent * (static_cast<float>(col) - static_cast<float>(count - 1) * 0.5f);
			s.spawn(
				std::format("PyramidBlock_{}_{}", row, col),
				box(
					gse::vec3<gse::position>(px, py, gse::meters(0.f)),
					gse::vec3<gse::length>(extent),
					gse::kilograms(20.f)
				)
			);
		}
	}

	gse::log::println(
		"pyramid: base={} blocks={} height={:.1f:m}",
		base_count,
		base_count * (base_count + 1) / 2,
		extent * static_cast<float>(base_count)
	);
}

auto sandbox::spawn_inverted_mass_pyramid(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	s.spawn(
		"Pyramid Light Base",
		box(
			origin + gse::meters(0.f, 0.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(5.f)
		)
	);
	s.spawn(
		"Pyramid Mid",
		box(
			origin + gse::meters(0.f, 1.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(50.f)
		)
	);
	s.spawn(
		"Pyramid Heavy Top",
		box(
			origin + gse::meters(0.f, 2.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(500.f)
		)
	);
}

auto sandbox::spawn_box_grid(gse::scene& s, const gse::vec3<gse::position>& origin, const stress_scene_params& params) -> void {
	const int grid_x = params.box_grid_side;
	const int grid_z = params.box_grid_side;
	const int layers = params.box_grid_layers;
	constexpr float spacing = 1.1f;

	for (int layer = 0; layer < layers; ++layer) {
		for (int ix = 0; ix < grid_x; ++ix) {
			for (int iz = 0; iz < grid_z; ++iz) {
				const auto pos = origin +
					gse::meters(
						static_cast<float>(ix) * spacing,
						0.5f + static_cast<float>(layer) * 1.05f,
						static_cast<float>(iz) * spacing
					);
				s.spawn(
					std::format("Grid L{}R{}C{}", layer, ix, iz),
					box(
						pos,
						gse::vec3<gse::length>(gse::meters(1.f)),
						gse::kilograms(20.f)
					)
				);
			}
		}
	}
}

auto sandbox::spawn_domino_chain(gse::scene& s, const int index, const gse::vec3<gse::position>& origin, const gse::angle heading) -> void {
	const auto spacing = gse::meters(0.9f);
	const gse::quat facing(gse::axis_y, heading);

	for (int i = 0; i < 12; ++i) {
		const auto step = gse::rotate_vector(facing, gse::vec3<gse::length>(spacing * static_cast<float>(i), gse::meters(0.f), gse::meters(0.f)));
		const auto pos = origin + step + gse::meters(0.f, (i == 0) ? 1.2f : 1.f, 0.f);
		const auto tilt = (i == 0) ? facing * gse::quat(gse::axis_z, gse::radians(-0.8f)) : facing;
		s.spawn(
			std::format("Domino {} {}", index, i + 1),
			box(
				pos,
				gse::meters(0.3f, 2.f, 1.f),
				gse::kilograms(30.f),
				tilt
			)
		);
	}
}

auto sandbox::spawn_sphere_stack(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr int stack_count = 8;
	constexpr float radius = 0.5f;
	constexpr float spacing = radius * 2.f + 0.02f;

	for (int i = 0; i < stack_count; ++i) {
		const auto pos = origin + gse::meters(
			0.f,
			radius + spacing * static_cast<float>(i),
			0.f
		);
		s.spawn(
			std::format("Sphere Stack {}", i),
			sphere(pos, gse::meters(radius), gse::sphere_lod::mid)
		);
	}
}

auto sandbox::spawn_corner_drop(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	s.spawn(
		"Corner Drop Plate",
		static_box(
			origin + gse::meters(0.f, 0.25f, 0.f),
			gse::meters(6.f, 0.5f, 6.f)
		)
	);

	const auto tilt = gse::quat(gse::axis_x, gse::degrees(45.f)) * gse::quat(gse::axis_z, gse::degrees(35.264f));
	s.spawn(
		"Corner Drop Box",
		box(
			origin + gse::meters(0.f, 8.f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(20.f),
			tilt
		)
	);
}

auto sandbox::spawn_slope_friction_test(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto ramp_size = gse::meters(10.f, 0.5f, 4.f);
	const gse::vec3 box_size(gse::meters(1.f));

	const auto resting_offset_for = [&](const gse::quat& tilt) {
		return gse::rotate_vector(tilt, gse::vec3<gse::length>(gse::meters(0.f), ramp_size.y() * 0.5f + box_size.y() * 0.5f, gse::meters(0.f)));
	};

	const gse::quat ramp_tilt(gse::axis_z, gse::degrees(30.f));
	const auto ramp_position = origin + gse::meters(0.f, 2.f, 0.f);
	s.spawn("Ramp 30deg", static_box(ramp_position, ramp_size, ramp_tilt));
	s.spawn(
		"Ramp Box Should Hold",
		box(
			ramp_position + resting_offset_for(ramp_tilt),
			box_size,
			gse::kilograms(50.f),
			ramp_tilt
		)
	);

	const gse::quat steep_tilt(gse::axis_z, gse::degrees(45.f));
	const auto steep_position = origin + gse::meters(12.f, 2.f, 0.f);
	s.spawn("Steep Ramp 45deg", static_box(steep_position, ramp_size, steep_tilt));
	s.spawn(
		"Steep Box Should Slide",
		box(
			steep_position + resting_offset_for(steep_tilt),
			box_size,
			gse::kilograms(50.f),
			steep_tilt
		)
	);
}

auto sandbox::spawn_high_speed_impact_target(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const auto pos = origin +
				gse::meters(
					-1.1f + static_cast<float>(col) * 1.1f,
					0.5f + static_cast<float>(row) * 1.05f,
					0.f
				);
			s.spawn(
				std::format("Impact Wall r{}c{}", row, col),
				box(
					pos,
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(80.f)
				)
			);
		}
	}
}
