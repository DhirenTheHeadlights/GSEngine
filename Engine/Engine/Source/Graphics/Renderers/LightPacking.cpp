module gse.graphics:light_packing_impl;

import std;

import :atmosphere_renderer;
import :directional_light;
import :light_culling_renderer;
import :light_packing;
import :point_light;
import :shared_shaders;
import :spot_light;

import gse.core;
import gse.ecs;
import gse.math;
import gse.physics;

namespace gse::renderer::light_packing {
	auto reach(
		const vec3f& color,
		irradiance intensity,
		float constant,
		inverse_length linear,
		inverse_area quadratic
	) -> length;
}

auto gse::renderer::light_packing::reach(const vec3f& color, const irradiance intensity, const float constant, const inverse_length linear, const inverse_area quadratic) -> length {
	const float peak_ratio = std::max({ color.x(), color.y(), color.z() }) * intensity / light_culling::limits.cull_threshold;
	const float offset = constant - peak_ratio;
	const length unbounded = meters(100.f);

	if (quadratic < per_meter(1e-6f) * per_meter(1.f)) {
		return linear > per_meter(1e-6f) ? -offset / linear : unbounded;
	}

	const auto discriminant = linear * linear - 4.f * quadratic * offset;
	if (discriminant < inverse_area{}) {
		return unbounded;
	}
	return (sqrt(discriminant) - linear) / (2.f * quadratic);
}

auto gse::renderer::light_packing::pack(const std::span<shaders::forward::light> out, const view_matrix& view, const shared_view<atmosphere::data> atmosphere, const read<directional_light_component>& dir_lights, const read<spot_light_component>& spot_lights, const read<point_light_component>& point_lights, const read<physics::transform_component>& transforms) -> std::size_t {
	std::size_t count = 0;

	if (count < out.size()) {
		const auto sun_to_surface = -atmosphere.sun_direction;
		out[count] = {
			.light_type = shaders::forward::light_type::directional,
			.direction = view.transform_direction(sun_to_surface),
			.world_direction = sun_to_surface,
			.color = atmosphere.sun_color * atmosphere.sun_transmittance,
			.intensity = atmosphere.sun_intensity,
			.ambient_strength = atmosphere.sun_ambient_strength,
			.source_radius = atmosphere.sun_source_radius,
		};
		++count;
	}

	for (const auto& comp : dir_lights) {
		if (count >= out.size()) {
			break;
		}
		out[count] = {
			.light_type = shaders::forward::light_type::directional,
			.direction = view.transform_direction(comp.direction),
			.world_direction = comp.direction,
			.color = comp.color,
			.intensity = comp.intensity,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++count;
	}

	const auto spot_ids = spot_lights.owner_ids();
	for (std::size_t i = 0; i < spot_lights.size(); ++i) {
		if (count >= out.size()) {
			break;
		}
		const auto* tc = transforms.find(spot_ids[i]);
		if (tc == nullptr) {
			continue;
		}
		const auto& comp = spot_lights[i];
		out[count] = {
			.light_type = shaders::forward::light_type::spot,
			.position = view.transform_point(tc->position),
			.direction = view.transform_direction(comp.direction),
			.world_position = tc->position,
			.world_direction = comp.direction,
			.color = comp.color,
			.intensity = comp.intensity,
			.constant = comp.constant,
			.linear = comp.linear,
			.quadratic = comp.quadratic,
			.cut_off = cos(comp.cut_off),
			.outer_cut_off = cos(comp.outer_cut_off),
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
			.radius = reach(comp.color, comp.intensity, comp.constant, comp.linear, comp.quadratic),
		};
		++count;
	}

	const auto point_ids = point_lights.owner_ids();
	for (std::size_t i = 0; i < point_lights.size(); ++i) {
		if (count >= out.size()) {
			break;
		}
		const auto* tc = transforms.find(point_ids[i]);
		if (tc == nullptr) {
			continue;
		}
		const auto& comp = point_lights[i];
		out[count] = {
			.light_type = shaders::forward::light_type::point,
			.position = view.transform_point(tc->position),
			.world_position = tc->position,
			.color = comp.color,
			.intensity = comp.intensity,
			.constant = comp.constant,
			.linear = comp.linear,
			.quadratic = comp.quadratic,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
			.radius = reach(comp.color, comp.intensity, comp.constant, comp.linear, comp.quadratic),
		};
		++count;
	}

	return count;
}
