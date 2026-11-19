export module gse.audio:modal;

import std;

import gse.math;

export namespace gse::audio {
	constexpr std::uint32_t ritz_degree = 12;

	struct box_mode {
		double stiffness = 0.0;
		vec3f axis_share;
		std::vector<float> shape;
	};

	auto ritz_term_count() -> std::size_t;

	auto solve_box_modes(
		const vec3f& proportions,
		float poisson_ratio,
		std::size_t count
	) -> std::vector<box_mode>;

	auto mode_displacement(
		std::span<const float> shape,
		const vec3f& at
	) -> vec3f;
}
