module gse.graphics;

import std;

import :material;
import :mesh;
import :model;
import :primitives;

import gse.assets;
import gse.core;
import gse.math;

namespace gse::primitives {
	auto build_box_meshes() -> std::vector<mesh_data>;

	auto build_sphere_meshes(std::uint32_t sectors, std::uint32_t stacks) -> std::vector<mesh_data>;
}

auto gse::primitives::build_box_meshes() -> std::vector<mesh_data> {
	const auto h = meters(0.5f);
	const auto neg_h = -h;

	std::vector<vertex> v;
	v.reserve(24);

	auto push_face = [&](const vec3<length>& a,
						 const vec3<length>& b,
						 const vec3<length>& c,
						 const vec3<length>& d,
						 const vec3f& n) {
		v.push_back({ a, n, { 0.0f, 0.0f } });
		v.push_back({ b, n, { 1.0f, 0.0f } });
		v.push_back({ c, n, { 1.0f, 1.0f } });
		v.push_back({ d, n, { 0.0f, 1.0f } });
	};

	push_face({ h, neg_h, neg_h }, { neg_h, neg_h, neg_h }, { neg_h, h, neg_h }, { h, h, neg_h }, { 0, 0, -1 });
	push_face({ neg_h, neg_h, h }, { h, neg_h, h }, { h, h, h }, { neg_h, h, h }, { 0, 0, 1 });
	push_face({ neg_h, neg_h, neg_h }, { neg_h, neg_h, h }, { neg_h, h, h }, { neg_h, h, neg_h }, { -1, 0, 0 });
	push_face({ h, neg_h, h }, { h, neg_h, neg_h }, { h, h, neg_h }, { h, h, h }, { 1, 0, 0 });
	push_face({ neg_h, h, h }, { h, h, h }, { h, h, neg_h }, { neg_h, h, neg_h }, { 0, 1, 0 });
	push_face({ neg_h, neg_h, neg_h }, { h, neg_h, neg_h }, { h, neg_h, h }, { neg_h, neg_h, h }, { 0, -1, 0 });

	std::vector<std::uint32_t> idx;
	idx.reserve(36);
	for (const auto f : std::views::iota(0u, 6u)) {
		const std::uint32_t o = f * 4;
		idx.push_back(o + 0);
		idx.push_back(o + 1);
		idx.push_back(o + 2);
		idx.push_back(o + 2);
		idx.push_back(o + 3);
		idx.push_back(o + 0);
	}

	std::vector<mesh_data> meshes;
	meshes.push_back(
		{
			.vertices = std::move(v),
			.indices = std::move(idx),
			.material = {},
		}
	);
	return meshes;
}

auto gse::primitives::build_sphere_meshes(const std::uint32_t sectors, const std::uint32_t stacks)
	-> std::vector<mesh_data> {
	const auto radius = meters(1.0f);

	std::vector<vertex> vertices;
	vertices.reserve(static_cast<std::size_t>(stacks + 1) * (sectors + 1));

	for (const auto i : std::views::iota(0u, stacks + 1)) {
		const float v_param = static_cast<float>(i) / static_cast<float>(stacks);
		const float phi = std::numbers::pi_v<float> * v_param;
		const float sp = std::sin(phi);
		const float cp = std::cos(phi);

		for (const auto j : std::views::iota(0u, sectors + 1)) {
			const float u_param = static_cast<float>(j) / static_cast<float>(sectors);
			const float theta = 2.0f * std::numbers::pi_v<float> * u_param;
			const float st = std::sin(theta);
			const float ct = std::cos(theta);

			const vec3<length> p{
				radius * sp * ct,
				radius * cp,
				radius * sp * st,
			};

			const vec3f n{
				sp * ct,
				cp,
				sp * st,
			};

			const vec2f t{
				u_param,
				v_param,
			};

			vertices.push_back({ p, n, t });
		}
	}

	std::vector<std::uint32_t> indices;
	indices.reserve(static_cast<std::size_t>(stacks) * sectors * 6);

	for (const auto i : std::views::iota(0u, stacks)) {
		for (const auto j : std::views::iota(0u, sectors)) {
			const std::uint32_t cur = i * (sectors + 1) + j;
			const std::uint32_t nxt = cur + (sectors + 1);

			indices.push_back(cur);
			indices.push_back(nxt);
			indices.push_back(cur + 1);
			indices.push_back(cur + 1);
			indices.push_back(nxt);
			indices.push_back(nxt + 1);
		}
	}

	std::vector<mesh_data> meshes;
	meshes.push_back(
		{
			.vertices = std::move(vertices),
			.indices = std::move(indices),
			.material = {},
		}
	);
	return meshes;
}

auto gse::primitives::initialize(data& d, asset::data& assets) -> void {
	d.unit_box = asset::queue<model>(assets, "Primitives/unit_box", build_box_meshes());
	d.sphere_lo = asset::queue<model>(assets, "Primitives/sphere_lo", build_sphere_meshes(8, 6));
	d.sphere_mid = asset::queue<model>(assets, "Primitives/sphere_mid", build_sphere_meshes(24, 16));
	d.sphere_hi = asset::queue<model>(assets, "Primitives/sphere_hi", build_sphere_meshes(48, 32));
}
