export module gse.physics:grass_field;

import std;

import gse.core;
import gse.math;

export namespace gse::physics {
	struct grass_field {
		int blades_per_side = 180;
		length spacing = meters(0.15f);
		length blade_height = meters(0.36f);
		length root_width = meters(0.03f);
		length tip_width = meters(0.002f);
		mass blade_mass = kilograms(0.006f);
		stiffness stretch_stiffness = newtons_per_meter(200.f);
		angular_stiffness bend_stiffness = newton_meters_per_radian(0.004f);
		stiffness contact_stiffness = newtons_per_meter(2000.f);
		inverse_time damping = per_second(25.f);
		length contact_margin = meters(0.01f);
		angle yield_angle = degrees(20.f);
		inverse_time yield_rate = per_second(6.f);
		angular_velocity recovery_rate = degrees_per_second(30.f);
		length sleep_tolerance = millimeters(1.f);
		velocity sleep_speed = meters_per_second(0.005f);
		time sleep_delay = seconds(0.5f);
	};

	struct grass_passthrough {};

	struct grass_reaction_target {
		id body;
	};
}

export namespace gse::physics::grass {
	constexpr std::uint32_t tile_blades = 50;

	auto blade_count(
		const grass_field& field
	) -> std::uint32_t;

	auto field_extent(
		const grass_field& field
	) -> length;

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

auto gse::physics::grass::blade_count(const grass_field& field) -> std::uint32_t {
	const auto side = static_cast<std::uint32_t>(std::max(field.blades_per_side, 0));
	return side * side;
}

auto gse::physics::grass::field_extent(const grass_field& field) -> length {
	return field.spacing * static_cast<float>(field.blades_per_side);
}

auto gse::physics::grass::tiles_per_side(const grass_field& field) -> std::uint32_t {
	const auto side = static_cast<std::uint32_t>(std::max(field.blades_per_side, 0));
	return (side + tile_blades - 1) / tile_blades;
}

auto gse::physics::grass::tile_count(const grass_field& field) -> std::uint32_t {
	const auto per_side = tiles_per_side(field);
	return per_side * per_side;
}

auto gse::physics::grass::rest_shape_changed(const grass_field& a, const grass_field& b) -> bool {
	return a.blades_per_side != b.blades_per_side
		|| a.spacing != b.spacing
		|| a.blade_height != b.blade_height
		|| a.root_width != b.root_width
		|| a.tip_width != b.tip_width;
}
