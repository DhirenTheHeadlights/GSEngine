module gse.graphics:font_impl;

import std;

import :font;
import :texture;


import gse.math;
import gse.gpu;
import gse.core;
import gse.assets;
import gse.assert;
import gse.concurrency;
import gse.config;
import gse.freetype;

namespace gse {
	auto decode_utf8(
		std::string_view text,
		std::size_t& pos
	) -> char32_t;
}

auto gse::decode_utf8(std::string_view text, std::size_t& pos) -> char32_t {
	if (pos >= text.size()) {
		return 0;
	}
	const auto b0 = static_cast<unsigned char>(text[pos]);
	if (b0 < 0x80) {
		++pos;
		return b0;
	}
	if ((b0 & 0xE0) == 0xC0 && pos + 1 < text.size()) {
		const auto b1 = static_cast<unsigned char>(text[pos + 1]);
		pos += 2;
		return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
	}
	if ((b0 & 0xF0) == 0xE0 && pos + 2 < text.size()) {
		const auto b1 = static_cast<unsigned char>(text[pos + 1]);
		const auto b2 = static_cast<unsigned char>(text[pos + 2]);
		pos += 3;
		return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
	}
	if ((b0 & 0xF8) == 0xF0 && pos + 3 < text.size()) {
		const auto b1 = static_cast<unsigned char>(text[pos + 1]);
		const auto b2 = static_cast<unsigned char>(text[pos + 2]);
		const auto b3 = static_cast<unsigned char>(text[pos + 3]);
		pos += 4;
		return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
	}
	++pos;
	return 0xFFFD;
}

gse::glyph::glyph(const info& i)
	: m_ft_glyph_index(i.ft_glyph_index), m_atlas_uv(i.atlas_uv), m_plane_bounds(i.plane_bounds), m_x_advance(i.x_advance) {
}

auto gse::glyph::ft_glyph_index() const -> std::uint32_t {
	return m_ft_glyph_index;
}

auto gse::glyph::atlas_uv() const -> vec4f {
	return m_atlas_uv;
}

auto gse::glyph::plane_bounds() const -> vec4f {
	return m_plane_bounds;
}

auto gse::glyph::x_advance() const -> float {
	return m_x_advance;
}

gse::font::font(const std::filesystem::path& path)
	: identifiable(path, config::baked_resource_path), m_baked_path(path) {
	assert(exists(path), "Font file '{}' does not exist.", path.display_string());
}

gse::font::~font() {
	if (m_face) {
		FT_Done_Face(m_face);
	}
	if (m_ft) {
		FT_Done_FreeType(m_ft);
	}
}

auto gse::font::load(asset::load_ctx& ctx) -> async::task<> {
	font::baked baked{};
	if (!load_baked(m_baked_path, baked)) {
		co_return;
	}

	const auto source_font_path = config::resource_path / baked.source_path_relative;

	m_ascender = baked.ascender;
	m_descender = baked.descender;
	m_pixel_range = baked.pixel_range;
	m_glyphs = std::move(baked.glyphs);

	m_max_glyph_top = 0.0f;
	m_min_glyph_bottom = 0.0f;
	for (const glyph& g : std::views::values(m_glyphs)) {
		const vec4f pb = g.plane_bounds();
		if (pb.z() <= 0.0f || pb.w() <= 0.0f) {
			continue;
		}
		m_max_glyph_top = std::max(m_max_glyph_top, pb.y() + pb.w());
		m_min_glyph_bottom = std::min(m_min_glyph_bottom, pb.y());
	}

	m_texture = std::make_unique<gse::texture>(
		std::format("msdf_font_atlas_{}", m_baked_path.stem().native_encoded_string()),
		baked.rgba.storage,
		vec2u{ baked.atlas_width, baked.atlas_height },
		baked.channels,
		texture::profile::msdf
	);

	co_await m_texture->load(ctx);

	assert(FT_Init_FreeType(&m_ft) == 0, "Failed to initialize FreeType.");

	const std::string source_path = source_font_path.native_encoded_string();
	assert(
		FT_New_Face(m_ft, source_path.c_str(), 0, &m_face) == 0,
		"Failed to load source font face for kerning."
	);

	const double units_per_em = std::max<double>(m_face->units_per_EM, 1);

	m_kerning.clear();
	auto valid_glyphs = std::views::values(m_glyphs) | std::views::filter([](const glyph& g) {
							return g.ft_glyph_index() != 0;
						});
	for (const glyph& ga : valid_glyphs) {
		const auto prev = ga.ft_glyph_index();
		for (const glyph& gb : valid_glyphs) {
			const auto next = gb.ft_glyph_index();

			FT_Vector kv{};
			FT_Get_Kerning(m_face, prev, next, freetype_kerning_unscaled, &kv);
			const float k = static_cast<float>(kv.x) / static_cast<float>(units_per_em);
			if (k != 0.0f) {
				const std::uint64_t key = (static_cast<std::uint64_t>(prev) << 32) | next;
				m_kerning.emplace(key, k);
			}
		}
	}
}

auto gse::font::unload() -> void {
	m_baked_path = std::filesystem::path();
	m_glyphs.clear();
	m_kerning.clear();
	m_texture.reset();
	if (m_face) {
		FT_Done_Face(m_face);
		m_face = nullptr;
	}
	if (m_ft) {
		FT_Done_FreeType(m_ft);
		m_ft = nullptr;
	}
}

auto gse::font::texture() const -> const gse::texture* {
	return m_texture.get();
}

auto gse::font::text_layout(const std::string_view text, const vec2f start, const float scale) const -> std::vector<positioned_glyph> {
	std::vector<positioned_glyph> positioned_glyphs;
	if (text.empty() || m_glyphs.empty()) {
		return {};
	}

	auto baseline = start;
	baseline.y() = std::round(baseline.y() - m_ascender * scale);

	auto cursor = baseline;
	std::uint32_t previous_glyph_index = 0;

	positioned_glyphs.reserve(text.size());

	for (std::size_t pos = 0; pos < text.size();) {
		const char32_t cp = decode_utf8(text, pos);
		if (cp == U'\n') {
			cursor.x() = baseline.x();
			cursor.y() = std::round(cursor.y() - line_height(scale));
			previous_glyph_index = 0;
			continue;
		}

		const auto it = m_glyphs.find(cp);
		if (it == m_glyphs.end()) {
			continue;
		}

		const glyph& g = it->second;

		if (previous_glyph_index != 0 && g.ft_glyph_index() != 0) {
			const std::uint64_t key = (static_cast<std::uint64_t>(previous_glyph_index) << 32) | g.ft_glyph_index();
			if (const auto kit = m_kerning.find(key); kit != m_kerning.end()) {
				cursor.x() += kit->second * scale;
			}
		}

		const vec4f pb = g.plane_bounds();
		if (pb.z() > 0.0f && pb.w() > 0.0f) {
			const vec2f top_left{
				cursor.x() + pb.x() * scale,
				cursor.y() + (pb.y() + pb.w()) * scale,
			};
			const vec2f size{ pb.z() * scale, pb.w() * scale };

			positioned_glyphs.emplace_back(
				positioned_glyph{
					.screen_rect = rect_t<vec2f>::from_position_size(top_left, size),
					.uv_rect = g.atlas_uv(),
				}
			);
		}

		cursor.x() += g.x_advance() * scale;
		previous_glyph_index = g.ft_glyph_index();
	}

	return positioned_glyphs;
}

auto gse::font::line_height(const float scale) const -> float {
	return (m_ascender - m_descender) * scale;
}

auto gse::font::width(const std::string_view text, const float scale) const -> float {
	if (text.empty() || m_glyphs.empty()) {
		return 0.0f;
	}

	float total_width = 0.0f;
	std::uint32_t previous_glyph_index = 0;

	for (std::size_t pos = 0; pos < text.size();) {
		const char32_t cp = decode_utf8(text, pos);
		const auto it = m_glyphs.find(cp);
		if (it == m_glyphs.end()) {
			continue;
		}

		const glyph& current_glyph = it->second;
		if (current_glyph.ft_glyph_index() == 0) {
			continue;
		}

		if (previous_glyph_index != 0) {
			const std::uint64_t key =
				(static_cast<std::uint64_t>(previous_glyph_index) << 32) | current_glyph.ft_glyph_index();
			if (const auto kit = m_kerning.find(key); kit != m_kerning.end()) {
				total_width += kit->second * scale;
			}
		}

		total_width += current_glyph.x_advance() * scale;
		previous_glyph_index = current_glyph.ft_glyph_index();
	}

	return total_width;
}

auto gse::font::caret_offsets(const std::string_view text, const float scale) const -> std::vector<float> {
	std::vector<float> offsets(text.size() + 1, 0.0f);
	if (m_glyphs.empty()) {
		return offsets;
	}

	float total = 0.0f;
	std::uint32_t previous_glyph_index = 0;

	for (std::size_t pos = 0; pos < text.size();) {
		const std::size_t start = pos;
		const char32_t cp = decode_utf8(text, pos);

		if (const auto it = m_glyphs.find(cp); it != m_glyphs.end() && it->second.ft_glyph_index() != 0) {
			const glyph& current_glyph = it->second;
			if (previous_glyph_index != 0) {
				const std::uint64_t key =
					(static_cast<std::uint64_t>(previous_glyph_index) << 32) | current_glyph.ft_glyph_index();
				if (const auto kit = m_kerning.find(key); kit != m_kerning.end()) {
					total += kit->second * scale;
				}
			}
			total += current_glyph.x_advance() * scale;
			previous_glyph_index = current_glyph.ft_glyph_index();
		}

		for (std::size_t b = start + 1; b <= pos; ++b) {
			offsets[b] = total;
		}
	}

	return offsets;
}

auto gse::font::vertical_center_offset(const float scale) const -> float {
	return m_ascender * scale * 0.5f;
}

auto gse::font::ascender_height(const float scale) const -> float {
	return m_ascender * scale;
}

auto gse::font::max_glyph_top(const float scale) const -> float {
	return m_max_glyph_top * scale;
}

auto gse::font::min_glyph_bottom(const float scale) const -> float {
	return m_min_glyph_bottom * scale;
}

auto gse::font::pixel_range() const -> float {
	return m_pixel_range;
}
