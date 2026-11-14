export module sandbox:grass_field;

import std;

import gse;

export namespace sandbox {
	struct grass_field {
		int blades_per_side = 180;
		gse::length spacing = gse::meters(0.15f);
		gse::length blade_height = gse::meters(0.36f);
		gse::length root_width = gse::meters(0.03f);
		gse::length tip_width = gse::meters(0.002f);
		gse::mass blade_mass = gse::kilograms(0.006f);
		gse::stiffness stretch_stiffness = gse::newtons_per_meter(200.f);
		gse::angular_stiffness bend_stiffness = gse::newton_meters_per_radian(0.004f);
		gse::stiffness contact_stiffness = gse::newtons_per_meter(2000.f);
		gse::inverse_time damping = gse::per_second(25.f);
		gse::length contact_margin = gse::meters(0.01f);
		gse::angle yield_angle = gse::degrees(20.f);
		gse::inverse_time yield_rate = gse::per_second(6.f);
		gse::angular_velocity recovery_rate = gse::degrees_per_second(30.f);
		gse::length sleep_tolerance = gse::millimeters(1.f);
		gse::velocity sleep_speed = gse::meters_per_second(0.005f);
		gse::time sleep_delay = gse::seconds(0.5f);
	};

	struct grass_passthrough {};

	struct grass_reaction_target {
		gse::id body;
	};
}

export namespace sandbox::grass {
	constexpr std::uint32_t tile_blades = 50;

	auto blade_count(
		const grass_field& field
	) -> std::uint32_t;

	auto field_extent(
		const grass_field& field
	) -> gse::length;

	auto tiles_per_side(
		const grass_field& field
	) -> std::uint32_t;

	auto tile_count(
		const grass_field& field
	) -> std::uint32_t;

	auto rest_shape_changed(
		const grass_field& a,
		const grass_field& b
	) -> bool;
}

auto sandbox::grass::blade_count(const grass_field& field) -> std::uint32_t {
	const auto side = static_cast<std::uint32_t>(std::max(field.blades_per_side, 0));
	return side * side;
}

auto sandbox::grass::field_extent(const grass_field& field) -> gse::length {
	return field.spacing * static_cast<float>(field.blades_per_side);
}

auto sandbox::grass::tiles_per_side(const grass_field& field) -> std::uint32_t {
	const auto side = static_cast<std::uint32_t>(std::max(field.blades_per_side, 0));
	return (side + tile_blades - 1) / tile_blades;
}

auto sandbox::grass::tile_count(const grass_field& field) -> std::uint32_t {
	const auto per_side = tiles_per_side(field);
	return per_side * per_side;
}

auto sandbox::grass::rest_shape_changed(const grass_field& a, const grass_field& b) -> bool {
	return a.blades_per_side != b.blades_per_side
		|| a.spacing != b.spacing
		|| a.blade_height != b.blade_height
		|| a.root_width != b.root_width
		|| a.tip_width != b.tip_width;
}
