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

	auto break_at_width(
		const font& face,
		std::string_view text,
		float max_width,
		float scale
	) -> std::size_t;
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

gse::glyph::glyph(const info& i) : m_ft_glyph_index(i.ft_glyph_index), m_atlas_uv(i.atlas_uv), m_plane_bounds(i.plane_bounds), m_x_advance(i.x_advance) {}

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
	: identifiable(config::asset_tag(path)), m_baked_path(path) {
	assert(exists(path), "Font file '{}' does not exist.", path.generic_display_string());
}

gse::font::~font() = default;

auto gse::font::load(asset::load_ctx& ctx) -> async::task<asset_result> {
	auto baked = load_baked<font::baked>(m_baked_path);
	if (!baked) {
		co_return std::unexpected(std::move(baked.error()));
	}

	m_ascender = baked->ascender;
	m_descender = baked->descender;
	m_pixel_range = baked->pixel_range;
	m_glyphs = std::move(baked->glyphs);
	m_kerning = std::move(baked->kerning);

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
		std::format("msdf_font_atlas_{}", config::asset_tag(m_baked_path)),
		baked->rgba.storage,
		vec2u{ baked->atlas_width, baked->atlas_height },
		baked->channels,
		texture::profile::msdf
	);

	if (auto texture_loaded = co_await m_texture->load(ctx); !texture_loaded) {
		co_return std::unexpected(std::move(texture_loaded.error()));
	}

	co_return asset_result{};
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

auto gse::break_at_width(const font& face, const std::string_view text, const float max_width, const float scale) -> std::size_t {
	std::string_view visible = text;
	while (!visible.empty() && visible.back() == ' ') {
		visible.remove_suffix(1);
	}

	if (visible.empty() || face.width(visible, scale) <= max_width) {
		return text.size();
	}

	const std::vector<float> offsets = face.caret_offsets(visible, scale);
	std::size_t fitted = 0;
	std::size_t first = 0;

	for (std::size_t b = 1; b <= visible.size(); ++b) {
		if (b < visible.size() && (static_cast<unsigned char>(visible[b]) & 0xC0) == 0x80) {
			continue;
		}
		if (first == 0) {
			first = b;
		}
		if (offsets[b] > max_width) {
			break;
		}
		fitted = b;
	}

	return fitted > 0 ? fitted : std::max<std::size_t>(first, 1);
}

auto gse::font::wrap(const std::string_view text, const float max_width, const float scale) const -> std::vector<std::string_view> {
	std::vector<std::string_view> lines;
	if (text.empty() || max_width <= 0.0f) {
		lines.push_back(text);
		return lines;
	}

	std::size_t start = 0;
	while (start < text.size()) {
		std::size_t accepted = std::string_view::npos;
		std::size_t cursor = start;
		while (cursor < text.size()) {
			const std::size_t space = text.find(' ', cursor);
			const std::size_t candidate = space == std::string_view::npos ? text.size() : space + 1;
			if (accepted != std::string_view::npos && width(text.substr(start, candidate - start), scale) > max_width) {
				break;
			}
			accepted = candidate;
			cursor = candidate;
		}

		const std::size_t line_end = accepted == std::string_view::npos ? text.size() : accepted;
		const std::size_t taken = break_at_width(*this, text.substr(start, line_end - start), max_width, scale);
		lines.push_back(text.substr(start, taken));
		start += taken;
	}
	return lines;
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
	return (m_ascender - (m_max_glyph_top + m_min_glyph_bottom) * 0.5f) * scale;
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
