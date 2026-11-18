module gse.audio:material_impl;

import std;

import gse.assets;
import gse.concurrency;
import gse.config;
import gse.containers;
import gse.core;
import gse.fs;
import gse.log;
import gse.math;
import gse.meta;

import :material;
import :mixer;
import :modal;

namespace gse::audio {
	struct material_source {
		density mass_density;
		elastic_modulus young_modulus;
		float poisson_ratio = 0.f;
		inverse_time mass_damping;
		time stiffness_damping;
	};

	struct box_source {
		vec3<displacement> size;
	};

	auto bake_box(
		const material_source& material,
		const vec3<displacement>& size
	) -> acoustic_box;
}

gse::acoustic_material::acoustic_material(const std::filesystem::path& filepath) : identifiable(config::asset_tag(filepath)), m_path(filepath) {}

auto gse::acoustic_material::load(asset::load_ctx&) -> async::task<asset_result> {
	auto loaded = load_baked<baked>(m_path);
	if (!loaded) {
		co_return std::unexpected(std::move(loaded.error()));
	}
	m_baked = std::move(*loaded);
	co_return asset_result{};
}

auto gse::acoustic_material::properties() const -> const baked& {
	return m_baked;
}

auto gse::acoustic_material::box_of(const vec3<displacement>& size) const -> const acoustic_box* {
	const displacement same = millimeters(0.1f);
	const auto found = std::ranges::find_if(m_baked.boxes, [&](const acoustic_box& box) {
		return abs(box.size.x() - size.x()) <= same && abs(box.size.y() - size.y()) <= same && abs(box.size.z() - size.z()) <= same;
	});
	return found != m_baked.boxes.end() ? &*found : nullptr;
}

auto gse::audio::mode_shape(const acoustic_box& box, const std::size_t mode) -> std::span<const float> {
	const std::size_t weights = 3 * ritz_term_count();
	return std::span(box.shapes.storage).subspan(mode * weights, weights);
}

auto gse::audio::bake_box(const material_source& material, const vec3<displacement>& size) -> acoustic_box {
	const frequency highest_baked = kilohertz(19.2f);
	constexpr float turn = 2.f * std::numbers::pi_v<float>;

	const vec3<displacement> half = size * 0.5f;
	const displacement longest = std::max({ half.x(), half.y(), half.z() });
	const vec3f proportions(half.x() / longest, half.y() / longest, half.z() / longest);
	const velocity bar_speed(sqrt(material.young_modulus / material.mass_density));

	acoustic_box box{
		.size = size,
	};
	for (const auto& [stiffness, axis_share, shape] : solve_box_modes(proportions, material.poisson_ratio, resonance_capacity)) {
		const inverse_time turning = bar_speed / longest * static_cast<float>(std::sqrt(stiffness));
		const frequency pitch = turning / turn;
		if (pitch >= highest_baked) {
			break;
		}
		box.rings.storage.push_back({
			.pitch = pitch,
			.decay = (material.mass_damping + material.stiffness_damping * turning * turning) * 0.5f,
		});
		box.axis_shares.storage.push_back(axis_share);
		box.shapes.storage.append_range(shape);
	}
	return box;
}

auto gse::bake(const std::filesystem::path& src, acoustic_material::baked& out) -> bool {
	std::ifstream file(src, std::ios::binary);
	if (!file.is_open()) {
		log::println(log::level::warning, log::category::assets, "Failed to open acoustic material source: {}", src.generic_display_string());
		return false;
	}
	const std::string text(std::istreambuf_iterator<char>(file), {});

	audio::material_source material;
	std::vector<audio::box_source> boxes;
	for (const auto& section : layout_store::parse_sections(text)) {
		if (section.name == "Material") {
			meta::read_fields(section.values, material);
		}
		else if (section.name.starts_with("Box.")) {
			meta::read_fields(section.values, boxes.emplace_back());
		}
	}

	const bool elastic = material.mass_density > density{} && material.young_modulus > elastic_modulus{} && material.poisson_ratio > 0.f && material.poisson_ratio < 0.5f;
	const bool damped = material.mass_damping > inverse_time{} && material.stiffness_damping >= time{};
	const bool sized = std::ranges::all_of(boxes, [](const audio::box_source& box) {
		return box.size.x() > displacement{} && box.size.y() > displacement{} && box.size.z() > displacement{};
	});
	if (!elastic || !damped || !sized) {
		log::println(
			log::level::warning,
			log::category::assets,
			"Acoustic material {} needs a [Material] section with mass_density, young_modulus, poisson_ratio between 0 and 0.5, a positive mass_damping and stiffness_damping, each with its unit, and a positive size in every [Box.*] section",
			src.generic_display_string()
		);
		return false;
	}

	out.mass_density = material.mass_density;
	out.young_modulus = material.young_modulus;
	out.poisson_ratio = material.poisson_ratio;
	out.mass_damping = material.mass_damping;
	out.stiffness_damping = material.stiffness_damping;
	for (const auto& [size] : boxes) {
		out.boxes.push_back(audio::bake_box(material, size));
	}
	return true;
}
