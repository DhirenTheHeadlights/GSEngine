module gse.graphics:mountain_ring_impl;

import std;

import :material;
import :mesh;
import :mountain_ring;
import :primitive_specs;

import gse.math;

namespace gse {
	auto mountain_ring_hash(
		std::uint32_t x,
		std::uint32_t y,
		std::uint32_t seed
	) -> float;

	auto mountain_ring_smooth(
		float t
	) -> float;

	auto mountain_ring_wrap(
		std::int32_t i,
		std::uint32_t cells
	) -> std::uint32_t;

	auto mountain_ring_noise(
		float u,
		float v,
		std::uint32_t cells_u,
		std::uint32_t seed
	) -> float;

	auto mountain_ring_ridge(
		float u,
		float v,
		std::uint32_t cells_u,
		std::uint32_t seed,
		std::uint32_t octaves
	) -> float;

	auto mountain_ring_submesh(
		const std::vector<vertex>& vertices,
		const std::vector<std::uint32_t>& indices,
		const material& surface
	) -> mesh_data;
}

auto gse::mountain_ring_hash(const std::uint32_t x, const std::uint32_t y, const std::uint32_t seed) -> float {
	std::uint32_t h = (x * 374761393u) ^ (y * 668265263u) ^ (seed * 2246822519u);
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return static_cast<float>(h) * 0x1p-32f;
}

auto gse::mountain_ring_smooth(const float t) -> float {
	return t * t * (3.0f - 2.0f * t);
}

auto gse::mountain_ring_wrap(const std::int32_t i, const std::uint32_t cells) -> std::uint32_t {
	const auto n = static_cast<std::int32_t>(cells);
	return static_cast<std::uint32_t>(((i % n) + n) % n);
}

auto gse::mountain_ring_noise(const float u, const float v, const std::uint32_t cells_u, const std::uint32_t seed) -> float {
	const float floor_u = std::floor(u);
	const float floor_v = std::floor(v);
	const auto iu = static_cast<std::int32_t>(floor_u);
	const auto iv = static_cast<std::int32_t>(floor_v);
	const float su = mountain_ring_smooth(u - floor_u);
	const float sv = mountain_ring_smooth(v - floor_v);

	const auto u0 = mountain_ring_wrap(iu, cells_u);
	const auto u1 = mountain_ring_wrap(iu + 1, cells_u);
	const auto v0 = static_cast<std::uint32_t>(iv);
	const auto v1 = static_cast<std::uint32_t>(iv + 1);

	const float a = mountain_ring_hash(u0, v0, seed);
	const float b = mountain_ring_hash(u1, v0, seed);
	const float c = mountain_ring_hash(u0, v1, seed);
	const float d = mountain_ring_hash(u1, v1, seed);

	const float lower = a + (b - a) * su;
	const float upper = c + (d - c) * su;
	return lower + (upper - lower) * sv;
}

auto gse::mountain_ring_ridge(const float u, const float v, const std::uint32_t cells_u, const std::uint32_t seed, const std::uint32_t octaves) -> float {
	float sum = 0.0f;
	float norm = 0.0f;
	float amplitude = 0.5f;
	std::uint32_t frequency = 1;

	for (const auto octave : std::views::iota(0u, octaves)) {
		const float sample = mountain_ring_noise(
			u * static_cast<float>(frequency),
			v * static_cast<float>(frequency),
			cells_u * frequency,
			seed + octave * 97u
		);
		sum += amplitude * (1.0f - std::abs(2.0f * sample - 1.0f));
		norm += amplitude;
		amplitude *= 0.5f;
		frequency *= 2;
	}

	return norm > 0.0f ? sum / norm : 0.0f;
}

auto gse::mountain_ring_submesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const material& surface) -> mesh_data {
	constexpr auto unused = std::numeric_limits<std::uint32_t>::max();

	std::vector<std::uint32_t> remap(vertices.size(), unused);
	std::vector<vertex> packed_vertices;
	std::vector<std::uint32_t> packed_indices;
	packed_indices.reserve(indices.size());

	for (const auto index : indices) {
		if (remap[index] == unused) {
			remap[index] = static_cast<std::uint32_t>(packed_vertices.size());
			packed_vertices.push_back(vertices[index]);
		}
		packed_indices.push_back(remap[index]);
	}

	return {
		.vertices = std::move(packed_vertices),
		.indices = std::move(packed_indices),
		.material = surface,
	};
}

auto gse::build_mountain_ring_meshes(const primitive_mountain_ring_spec& spec) -> std::vector<mesh_data> {
	const auto angular = std::max(spec.angular_segments, 3u);
	const auto radial = std::max(spec.radial_segments, 2u);
	const auto cells = std::max(spec.ridge_cells, 1u);
	const auto radial_cells = std::max(spec.radial_cells, 1u);
	const auto massif_cells = std::max(spec.massif_cells, 1u);
	const float massif_depth = std::clamp(spec.massif_depth, 0.0f, 1.0f);
	const auto octaves = std::max(spec.octaves, 1u);
	const auto stride = radial + 1;
	const auto span = spec.outer_radius - spec.inner_radius;
	const float ridge_base = std::clamp(spec.ridge_base, 0.0f, 1.0f);

	std::vector<vertex> vertices;
	vertices.reserve(static_cast<std::size_t>(angular) * stride);

	std::vector<float> elevation;
	elevation.reserve(static_cast<std::size_t>(angular) * stride);

	for (const auto i : std::views::iota(0u, angular)) {
		const float around = static_cast<float>(i) / static_cast<float>(angular);
		const angle theta = degrees(360.0f) * around;
		const float cos_theta = cos(theta);
		const float sin_theta = sin(theta);
		const float massif = mountain_ring_noise(around * static_cast<float>(massif_cells), 0.5f, massif_cells, spec.seed + 613u);
		const float massif_scale = (1.0f - massif_depth) + massif_depth * massif;

		for (const auto j : std::views::iota(0u, stride)) {
			const float across = static_cast<float>(j) / static_cast<float>(radial);
			const length radius = spec.inner_radius + span * across;
			const angle sweep = degrees(180.0f) * across;
			const float falloff = std::pow(sin(sweep), 0.8f);
			const float ridge = mountain_ring_ridge(around * static_cast<float>(cells), across * static_cast<float>(radial_cells), cells, spec.seed, octaves);
			const float relief = ridge_base + (1.0f - ridge_base) * ridge;
			const float rise = falloff * relief * massif_scale;
			const length height = spec.peak_height * rise;

			vertices.push_back({
				.position = vec3<length>(radius * cos_theta, height, radius * sin_theta),
				.normal = vec3f(0.0f),
				.tex_coords = vec2f(around, across),
			});
			elevation.push_back(rise);
		}
	}

	std::vector<std::uint32_t> indices;
	indices.reserve(static_cast<std::size_t>(angular) * radial * 6);

	for (const auto i : std::views::iota(0u, angular)) {
		const auto next = (i + 1) % angular;
		for (const auto j : std::views::iota(0u, radial)) {
			const auto inner_here = i * stride + j;
			const auto inner_next = next * stride + j;
			const auto outer_here = inner_here + 1;
			const auto outer_next = inner_next + 1;

			indices.push_back(inner_here);
			indices.push_back(inner_next);
			indices.push_back(outer_here);
			indices.push_back(outer_here);
			indices.push_back(inner_next);
			indices.push_back(outer_next);
		}
	}

	std::vector<vec3f> accumulated(vertices.size(), vec3f(0.0f));

	for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
		const auto i0 = indices[t];
		const auto i1 = indices[t + 1];
		const auto i2 = indices[t + 2];
		const vec3<displacement> edge1 = vertices[i1].position - vertices[i0].position;
		const vec3<displacement> edge2 = vertices[i2].position - vertices[i0].position;
		const vec3f face = normalize(cross(edge1, edge2));
		accumulated[i0] += face;
		accumulated[i1] += face;
		accumulated[i2] += face;
	}

	for (std::size_t v = 0; v < vertices.size(); ++v) {
		const float length_of = magnitude(accumulated[v]);
		vertices[v].normal = length_of > 0.0f ? accumulated[v] / length_of : vec3f(0.0f, 1.0f, 0.0f);
	}

	const float snow_line = std::clamp(spec.snow_line, 0.0f, 1.0f);
	const float snow_slope = std::clamp(spec.snow_slope, 0.0f, 1.0f);

	std::vector<std::uint32_t> rock_indices;
	std::vector<std::uint32_t> snow_indices;
	rock_indices.reserve(indices.size());

	for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
		const auto i0 = indices[t];
		const auto i1 = indices[t + 1];
		const auto i2 = indices[t + 2];
		const float rise = (elevation[i0] + elevation[i1] + elevation[i2]) / 3.0f;
		const float facing = (vertices[i0].normal.y() + vertices[i1].normal.y() + vertices[i2].normal.y()) / 3.0f;

		auto& target = rise > snow_line && facing > snow_slope ? snow_indices : rock_indices;
		target.push_back(i0);
		target.push_back(i1);
		target.push_back(i2);
	}

	const material rock_surface{
		.base_color = spec.material.base_color,
		.roughness = spec.material.roughness,
		.metallic = spec.material.metallic,
	};

	const material snow_surface{
		.base_color = spec.snow_color,
		.roughness = spec.snow_roughness,
		.metallic = 0.0f,
	};

	std::vector<mesh_data> meshes;
	if (!rock_indices.empty()) {
		meshes.push_back(mountain_ring_submesh(vertices, rock_indices, rock_surface));
	}
	if (!snow_indices.empty()) {
		meshes.push_back(mountain_ring_submesh(vertices, snow_indices, snow_surface));
	}
	return meshes;
}
