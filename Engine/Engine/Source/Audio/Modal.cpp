module gse.audio:modal_impl;

import std;

import gse.math;

import :modal;

namespace gse::audio {
	constexpr std::size_t ritz_orders = ritz_degree + 1;
	constexpr std::size_t symmetry_classes = 8;
	constexpr std::size_t rigid_modes = 6;

	using ritz_term = std::array<std::uint32_t, 3>;
	using order_table = std::array<std::array<double, ritz_orders>, ritz_orders>;

	struct ritz_unknown {
		std::uint32_t component = 0;
		std::uint32_t term = 0;
	};

	struct symmetry_block {
		std::vector<ritz_unknown> unknowns;
		std::vector<double> stiffness;
		std::vector<double> vectors;
	};

	struct solved_mode {
		double stiffness = 0.0;
		std::size_t block = 0;
		std::size_t column = 0;
	};

	auto ritz_terms() -> std::vector<ritz_term>;

	auto slope_table() -> order_table;

	auto slope_overlap_table(
		const order_table& slope
	) -> order_table;

	auto symmetry_class_of(
		const ritz_unknown& unknown,
		const ritz_term& term
	) -> std::size_t;

	auto gradient_overlap(
		const order_table& slope,
		const order_table& slope_overlap,
		std::size_t along_first,
		std::size_t along_second,
		const ritz_term& first,
		const ritz_term& second
	) -> double;

	auto diagonalize(
		std::vector<double>& matrix,
		std::vector<double>& vectors,
		std::size_t size
	) -> void;

	auto normalized_legendre(
		double at
	) -> std::array<double, ritz_orders>;
}

auto gse::audio::ritz_term_count() -> std::size_t {
	return (ritz_degree + 1) * (ritz_degree + 2) * (ritz_degree + 3) / 6;
}

auto gse::audio::ritz_terms() -> std::vector<ritz_term> {
	std::vector<ritz_term> terms;
	terms.reserve(ritz_term_count());
	for (std::uint32_t first = 0; first <= ritz_degree; ++first) {
		for (std::uint32_t second = 0; first + second <= ritz_degree; ++second) {
			for (std::uint32_t third = 0; first + second + third <= ritz_degree; ++third) {
				terms.push_back({ first, second, third });
			}
		}
	}
	return terms;
}

auto gse::audio::slope_table() -> order_table {
	order_table slope{};
	for (std::size_t of = 0; of < ritz_orders; ++of) {
		for (std::size_t onto = 0; onto < of; ++onto) {
			if ((of - onto) % 2 == 1) {
				slope[of][onto] = std::sqrt(static_cast<double>((2 * of + 1) * (2 * onto + 1)));
			}
		}
	}
	return slope;
}

auto gse::audio::slope_overlap_table(const order_table& slope) -> order_table {
	order_table overlap{};
	for (std::size_t first = 0; first < ritz_orders; ++first) {
		for (std::size_t second = 0; second < ritz_orders; ++second) {
			for (std::size_t onto = 0; onto < ritz_orders; ++onto) {
				overlap[first][second] += slope[first][onto] * slope[second][onto];
			}
		}
	}
	return overlap;
}

auto gse::audio::symmetry_class_of(const ritz_unknown& unknown, const ritz_term& term) -> std::size_t {
	std::size_t symmetry = 0;
	for (std::size_t axis = 0; axis < 3; ++axis) {
		const std::size_t odd = (term[axis] + (axis == unknown.component ? 1 : 0)) % 2;
		symmetry |= odd << axis;
	}
	return symmetry;
}

auto gse::audio::gradient_overlap(const order_table& slope, const order_table& slope_overlap, const std::size_t along_first, const std::size_t along_second, const ritz_term& first, const ritz_term& second) -> double {
	double overlap = 1.0;
	for (std::size_t axis = 0; axis < 3; ++axis) {
		const bool first_sloped = axis == along_first;
		const bool second_sloped = axis == along_second;
		if (first_sloped && second_sloped) {
			overlap *= slope_overlap[first[axis]][second[axis]];
		}
		else if (first_sloped) {
			overlap *= slope[first[axis]][second[axis]];
		}
		else if (second_sloped) {
			overlap *= slope[second[axis]][first[axis]];
		}
		else if (first[axis] != second[axis]) {
			return 0.0;
		}
	}
	return overlap;
}

auto gse::audio::diagonalize(std::vector<double>& matrix, std::vector<double>& vectors, const std::size_t size) -> void {
	constexpr int sweep_limit = 64;
	constexpr double converged = 1e-26;

	const auto at = [size](std::vector<double>& values, const std::size_t row, const std::size_t column) -> double& {
		return values[row * size + column];
	};

	vectors.assign(size * size, 0.0);
	for (std::size_t index = 0; index < size; ++index) {
		at(vectors, index, index) = 1.0;
	}

	const double total = std::ranges::fold_left(matrix, 0.0, [](const double sum, const double value) {
		return sum + value * value;
	});

	for (int sweep = 0; sweep < sweep_limit; ++sweep) {
		double off_diagonal = 0.0;
		for (std::size_t p = 0; p < size; ++p) {
			for (std::size_t q = p + 1; q < size; ++q) {
				off_diagonal += at(matrix, p, q) * at(matrix, p, q);
			}
		}
		if (off_diagonal <= converged * total) {
			return;
		}

		for (std::size_t p = 0; p < size; ++p) {
			for (std::size_t q = p + 1; q < size; ++q) {
				const double coupling = at(matrix, p, q);
				if (coupling == 0.0) {
					continue;
				}
				const double theta = (at(matrix, q, q) - at(matrix, p, p)) / (2.0 * coupling);
				const double tangent = std::copysign(1.0, theta) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
				const double cosine = 1.0 / std::sqrt(tangent * tangent + 1.0);
				const double sine = tangent * cosine;

				for (std::size_t k = 0; k < size; ++k) {
					const double in_p = at(matrix, k, p);
					const double in_q = at(matrix, k, q);
					at(matrix, k, p) = cosine * in_p - sine * in_q;
					at(matrix, k, q) = sine * in_p + cosine * in_q;
				}
				for (std::size_t k = 0; k < size; ++k) {
					const double in_p = at(matrix, p, k);
					const double in_q = at(matrix, q, k);
					at(matrix, p, k) = cosine * in_p - sine * in_q;
					at(matrix, q, k) = sine * in_p + cosine * in_q;
				}
				for (std::size_t k = 0; k < size; ++k) {
					const double in_p = at(vectors, k, p);
					const double in_q = at(vectors, k, q);
					at(vectors, k, p) = cosine * in_p - sine * in_q;
					at(vectors, k, q) = sine * in_p + cosine * in_q;
				}
			}
		}
	}
}

auto gse::audio::solve_box_modes(const vec3f& proportions, const float poisson_ratio, const std::size_t count) -> std::vector<box_mode> {
	const double poisson = poisson_ratio;
	const double dilation = poisson / ((1.0 + poisson) * (1.0 - 2.0 * poisson));
	const double shear = 1.0 / (2.0 * (1.0 + poisson));

	const auto terms = ritz_terms();
	const auto slope = slope_table();
	const auto slope_overlap = slope_overlap_table(slope);

	std::array<symmetry_block, symmetry_classes> blocks;
	for (std::uint32_t component = 0; component < 3; ++component) {
		for (std::uint32_t term = 0; term < terms.size(); ++term) {
			const ritz_unknown unknown{ .component = component, .term = term };
			blocks[symmetry_class_of(unknown, terms[term])].unknowns.push_back(unknown);
		}
	}

	std::vector<solved_mode> solved;
	for (std::size_t block_index = 0; block_index < symmetry_classes; ++block_index) {
		auto& [unknowns, stiffness, vectors] = blocks[block_index];
		const std::size_t size = unknowns.size();
		stiffness.assign(size * size, 0.0);

		for (std::size_t row = 0; row < size; ++row) {
			const std::size_t i = unknowns[row].component;
			const ritz_term& first = terms[unknowns[row].term];
			for (std::size_t column = row; column < size; ++column) {
				const std::size_t k = unknowns[column].component;
				const ritz_term& second = terms[unknowns[column].term];

				const double cross_scale = 1.0 / (static_cast<double>(proportions[i]) * static_cast<double>(proportions[k]));
				double entry = dilation * cross_scale * gradient_overlap(slope, slope_overlap, i, k, first, second);
				entry += shear * cross_scale * gradient_overlap(slope, slope_overlap, k, i, first, second);
				if (i == k) {
					for (std::size_t axis = 0; axis < 3; ++axis) {
						const double along = proportions[axis];
						entry += shear / (along * along) * gradient_overlap(slope, slope_overlap, axis, axis, first, second);
					}
				}
				stiffness[row * size + column] = entry;
				stiffness[column * size + row] = entry;
			}
		}

		diagonalize(stiffness, vectors, size);
		for (std::size_t column = 0; column < size; ++column) {
			solved.push_back({
				.stiffness = stiffness[column * size + column],
				.block = block_index,
				.column = column,
			});
		}
	}

	std::ranges::sort(solved, {}, &solved_mode::stiffness);

	std::vector<box_mode> modes;
	for (const auto& [stiffness, block_index, column] : solved | std::views::drop(rigid_modes) | std::views::take(count)) {
		const auto& block = blocks[block_index];
		const std::size_t size = block.unknowns.size();

		box_mode mode{
			.stiffness = stiffness,
			.shape = std::vector<float>(3 * terms.size(), 0.f),
		};
		for (std::size_t row = 0; row < size; ++row) {
			const auto& [component, term] = block.unknowns[row];
			const auto weight = static_cast<float>(block.vectors[row * size + column]);
			mode.shape[component * terms.size() + term] = weight;
			mode.axis_share[component] += weight * weight;
		}
		modes.push_back(std::move(mode));
	}
	return modes;
}

auto gse::audio::normalized_legendre(const double at) -> std::array<double, ritz_orders> {
	std::array<double, ritz_orders> values{};
	values[0] = 1.0;
	values[1] = at;
	for (std::size_t order = 1; order + 1 < ritz_orders; ++order) {
		const auto n = static_cast<double>(order);
		values[order + 1] = ((2.0 * n + 1.0) * at * values[order] - n * values[order - 1]) / (n + 1.0);
	}
	for (std::size_t order = 0; order < ritz_orders; ++order) {
		values[order] *= std::sqrt((2.0 * static_cast<double>(order) + 1.0) / 2.0);
	}
	return values;
}

auto gse::audio::mode_displacement(const std::span<const float> shape, const vec3f& at) -> vec3f {
	const std::array along{
		normalized_legendre(at[0]),
		normalized_legendre(at[1]),
		normalized_legendre(at[2]),
	};

	const std::size_t term_count = ritz_term_count();
	std::array<double, 3> moved{};
	std::size_t term = 0;
	for (std::uint32_t first = 0; first <= ritz_degree; ++first) {
		for (std::uint32_t second = 0; first + second <= ritz_degree; ++second) {
			for (std::uint32_t third = 0; first + second + third <= ritz_degree; ++third) {
				const double value = along[0][first] * along[1][second] * along[2][third];
				for (std::size_t component = 0; component < 3; ++component) {
					moved[component] += shape[component * term_count + term] * value;
				}
				++term;
			}
		}
	}
	return vec3f(static_cast<float>(moved[0]), static_cast<float>(moved[1]), static_cast<float>(moved[2]));
}
