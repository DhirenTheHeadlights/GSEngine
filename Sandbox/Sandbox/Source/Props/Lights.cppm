export module sandbox:lights;

import std;
import gse;

import :archetypes;
import :piston;
import :spawn_params;

export namespace sandbox {
	auto spawn_light_field(
		gse::scene& s,
		const light_field_params& params = {}
	) -> void;
}

auto sandbox::spawn_light_field(gse::scene& s, const light_field_params& params) -> void {
	constexpr float golden_angle = 137.50776f;
	constexpr float golden_ratio_frac = 0.6180339f;
	const auto inverse_reach = 1.f / params.falloff_radius;
	const float reach_ratio = params.intensity / gse::renderer::light_culling::limits.cull_threshold;
	const auto falloff_linear = inverse_reach * 2.f;
	const auto falloff_quadratic = inverse_reach * inverse_reach * std::max(reach_ratio - 3.f, 1.f);

	const bool drifting = params.drift > gse::meters(0.f);
	int spawned = 0;

	for (int c = 0; c < params.clusters; ++c) {
		const auto cluster_index = static_cast<float>(c);
		const auto theta = gse::degrees(golden_angle * cluster_index);
		const auto radial = params.field_radius * std::sqrt((cluster_index + 0.5f) / static_cast<float>(params.clusters));
		const auto height = params.field_height * (golden_ratio_frac * cluster_index - std::floor(golden_ratio_frac * cluster_index));
		const auto center = gse::vec3<gse::position>(radial * gse::sin(theta), height, radial * gse::cos(theta));

		const auto hue = gse::degrees(360.f * (golden_ratio_frac * cluster_index * 3.f - std::floor(golden_ratio_frac * cluster_index * 3.f)));
		const auto color = gse::vec3f(
			0.55f + 0.45f * gse::sin(hue),
			0.55f + 0.45f * gse::sin(hue + gse::degrees(120.f)),
			0.55f + 0.45f * gse::sin(hue + gse::degrees(240.f))
		);

		for (int i = 0; i < params.lights_per_cluster; ++i) {
			const auto local_index = static_cast<float>(i);
			const auto local_theta = gse::degrees(golden_angle * (local_index + cluster_index));
			const auto local_radial = params.cluster_radius * std::sqrt((local_index + 0.5f) / static_cast<float>(params.lights_per_cluster));
			const auto local_height = params.cluster_radius * (2.f * (golden_ratio_frac * local_index - std::floor(golden_ratio_frac * local_index)) - 1.f);
			const auto origin = center + gse::vec3<gse::length>(
				local_radial * gse::sin(local_theta),
				local_height,
				local_radial * gse::cos(local_theta)
			);

			auto light = sphere_light(origin, params.source_radius);
			light.light.color = color;
			light.light.intensity = params.intensity;
			light.light.linear = falloff_linear;
			light.light.quadratic = falloff_quadratic;
			light.spec.material.base_color = color;
			if (drifting) {
				light.motion.body = gse::physics::kinematic_body{};
			}

			const auto light_id = s.spawn(std::format("FieldLight_{}", spawned), std::move(light));
			++spawned;

			if (drifting) {
				s.registry().add_component<piston::component>(
					light_id,
					{
						.center = origin,
						.amplitude = gse::vec3<gse::length>(
							params.drift * gse::sin(local_theta),
							params.drift * 0.35f,
							params.drift * gse::cos(local_theta)
						),
						.omega = params.drift_rate,
						.phase = gse::degrees(golden_angle * static_cast<float>(spawned)),
					}
				);
				s.registry().add_component<gse::physics::kinematic_target_component>(
					light_id,
					{
						.position = origin,
					}
				);
			}
		}
	}

	gse::log::println("light field: {} lights in {} clusters across {:.1f:m} radius", spawned, params.clusters, params.field_radius);
}
