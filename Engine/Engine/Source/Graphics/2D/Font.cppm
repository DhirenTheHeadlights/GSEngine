module;

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

export module gse.graphics:font;

import <freetype/freetype.h>;

import :texture;

import gse.assert;
import gse.math;
import gse.platform;
import gse.utility;

export namespace gse {
    struct glyph {
        float ft_glyph_index = 0;
        float u0 = 0, v0 = 0;
        float u1 = 0, v1 = 0;
        float width = 0, height = 0;
        float x_offset = 0, y_offset = 0;
        float x_advance = 0;
        float shape_w = 0, shape_h = 0;

        auto uv() const -> unitless::vec4;
        auto size() const -> unitless::vec2;
        auto bearing() const -> unitless::vec2;
    };
}

auto gse::glyph::uv() const -> unitless::vec4 {
	return { u0, v0, u1 - u0, v1 - v0 };
}

auto gse::glyph::size() const -> unitless::vec2 {
	return { width, height };
}

auto gse::glyph::bearing() const -> unitless::vec2 {
	return { x_offset, y_offset };
}

export namespace gse {
    struct positioned_glyph {
        rect_t<unitless::vec2> screen_rect;
        unitless::vec4 uv_rect;
    };

    class font : public identifiable {
    public:
	    explicit font(
            const std::filesystem::path& path
        );

        ~font();

        auto load(
            const gpu::context& context
        ) -> void;

        auto unload(
        ) -> void;

        auto texture(
        ) const -> const texture*;

        auto text_layout(
            std::string_view text, 
            unitless::vec2 start, 
            float scale = 1.0f
        ) const -> std::vector<positioned_glyph>;

        auto line_height(
            float scale = 1.0f
        ) const -> float;

		auto width(
            std::string_view text, 
            float scale = 1.0f
        ) const -> float;
    private:
        std::unique_ptr<gse::texture> m_texture;
        std::unordered_map<char, glyph> m_glyphs;
        std::unordered_map<std::uint64_t, float> m_kerning;

        float m_glyph_cell_size = 64.f;
        float m_padding = 8.f;
        float m_ascender = 0.0f;
        float m_descender = 0.0f;

        FT_Face m_face = nullptr;
        FT_Library m_ft = nullptr;

        std::filesystem::path m_baked_path;
    };
}

gse::font::font(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), m_baked_path(path) {
    assert(
        exists(path),
        std::source_location::current(),
        "Font file '{}' does not exist.", path.string()
    );
}

gse::font::~font() {
    if (m_face) {
        FT_Done_Face(m_face);
    }
    if (m_ft) {
        FT_Done_FreeType(m_ft);
    }
}

auto gse::font::load(const gpu::context& context) -> void {
    std::ifstream in_file(m_baked_path, std::ios::binary);
    assert(
        in_file.is_open(), std::source_location::current(),
        "Failed to open baked font file: {}",
        m_baked_path.string()
    );

    std::uint32_t magic, version;
    in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in_file.read(reinterpret_cast<char*>(&version), sizeof(version));

    assert(
        magic == 0x47464E54 && version == 1, 
        std::source_location::current(), "Invalid baked font file format or version: {}", m_baked_path.string()
    );

    std::uint64_t path_len;
    in_file.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
    std::string relative_src_str(path_len, '\0');
    in_file.read(&relative_src_str[0], path_len);
    const auto source_font_path = config::resource_path / relative_src_str;

    in_file.read(reinterpret_cast<char*>(&m_ascender), sizeof(m_ascender));
    in_file.read(reinterpret_cast<char*>(&m_descender), sizeof(m_descender));

    std::uint32_t atlas_width, atlas_height, channels;
    std::uint64_t data_size;
    in_file.read(reinterpret_cast<char*>(&atlas_width), sizeof(atlas_width));
    in_file.read(reinterpret_cast<char*>(&atlas_height), sizeof(atlas_height));
    in_file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
    in_file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
    std::vector<std::byte> atlas_pixel_data(data_size);
    in_file.read(reinterpret_cast<char*>(atlas_pixel_data.data()), data_size);

    m_texture = std::make_unique<gse::texture>(
        std::format("msdf_font_atlas_{}", m_baked_path.stem().string()),
        atlas_pixel_data,
        unitless::vec2u{ atlas_width, atlas_height },
        channels,
        texture::profile::msdf
    );

    m_texture->load(context);

    std::uint64_t glyph_count;
    in_file.read(reinterpret_cast<char*>(&glyph_count), sizeof(glyph_count));
    m_glyphs.reserve(glyph_count);
    for (std::uint64_t i = 0; i < glyph_count; ++i) {
        char ch;
        glyph glyph_data;
        in_file.read(&ch, sizeof(ch));
        in_file.read(reinterpret_cast<char*>(&glyph_data), sizeof(glyph_data));
        m_glyphs[ch] = glyph_data;
    }

    assert(
        FT_Init_FreeType(&m_ft) == 0, 
        std::source_location::current(),
        "Failed to initialize FreeType."
    );

    assert(
        FT_New_Face(
			m_ft, 
			source_font_path.string().c_str(), 
			0, 
			&m_face
		) == 0, 
        std::source_location::current(), 
        "Failed to load source font face for kerning."
    );

    assert(
        FT_Set_Pixel_Sizes(m_face, 0, 64) == 0, 
        std::source_location::current(),
        "Failed to set font pixel size for kerning."
    );

    m_kerning.clear();
    auto valid_glyphs = m_glyphs | std::views::values
        | std::views::filter([](const auto& g) { return g.ft_glyph_index != 0; });
    for (const auto& ga : valid_glyphs) {
        const auto prev = static_cast<std::uint32_t>(ga.ft_glyph_index);
        for (const auto& gb : valid_glyphs) {
            const auto next = static_cast<std::uint32_t>(gb.ft_glyph_index);

            FT_Vector kv{};
            FT_Get_Kerning(m_face, prev, next, FT_KERNING_DEFAULT, &kv);
            const float k = static_cast<float>(kv.x) / 64.0f;
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

auto gse::font::text_layout(const std::string_view text, const unitless::vec2 start, const float scale) const -> std::vector<positioned_glyph> {
    std::vector<positioned_glyph> positioned_glyphs;
    if (text.empty() || m_glyphs.empty()) {
	    return {};
    }

    auto baseline = start;
    baseline.y() -= std::isfinite(m_ascender) ? m_ascender * scale : 0.0f;

    auto cursor = baseline;
    std::uint32_t previous_glyph_index = 0;

    float fallback_advance = m_glyph_cell_size * 0.5f * scale;

	if (const auto it = m_glyphs.find(' '); it != m_glyphs.end() && std::isfinite(it->second.height)) {
		fallback_advance = it->second.height * scale;
	}

    positioned_glyphs.reserve(text.size());

    for (char c : text) {
        if (c == '\n') {
            cursor.x() = baseline.x();
            cursor.y() -= line_height(scale);
            previous_glyph_index = 0;
            continue;
        }

        auto it = m_glyphs.find(c);
        if (it == m_glyphs.end()) {
            cursor.x() += fallback_advance;
            previous_glyph_index = 0;
            continue;
        }

        const glyph& g = it->second;

        if (g.ft_glyph_index == 0) {
            cursor.x() += fallback_advance;
            previous_glyph_index = 0;
            continue;
        }

        if (previous_glyph_index != 0) {
            const auto next = static_cast<std::uint32_t>(g.ft_glyph_index);
            const std::uint64_t key = (static_cast<std::uint64_t>(previous_glyph_index) << 32) | next;
            if (auto kerning_it = m_kerning.find(key); kerning_it != m_kerning.end()) {
                cursor.x() += kerning_it->second * scale;
            }
        }

        const float bx = std::isfinite(g.x_offset) ? g.x_offset : 0.0f;
        const float by = std::isfinite(g.y_offset) ? g.y_offset : 0.0f;

        unitless::vec2 quad_pos{
            cursor.x() + bx * scale,
            cursor.y() + by * scale
        };

        const float gw = std::isfinite(g.width)  ? std::max(g.width,  0.0f) : 0.0f;
        const float gh = std::isfinite(g.height) ? std::max(g.height, 0.0f) : 0.0f;
        unitless::vec2 quad_size{ gw * scale, gh * scale };

        const bool emit_rect = (quad_size.x() > 0.0f && quad_size.y() > 0.0f);

        unitless::vec4 full_cell_uv = g.uv();
        if (!std::isfinite(full_cell_uv.x()) || !std::isfinite(full_cell_uv.y()) ||
            !std::isfinite(full_cell_uv.z()) || !std::isfinite(full_cell_uv.w()) ||
            full_cell_uv.z() <= 0.0f || full_cell_uv.w() <= 0.0f) {
            full_cell_uv = { 0.0f, 0.0f, 1.0f, 1.0f };
        }

        unitless::vec4 corrected_uv = full_cell_uv;

        const double sw = g.shape_w;
        const double sh = g.shape_h;
        const bool cell_ok =
            std::isfinite(static_cast<double>(m_glyph_cell_size)) && m_glyph_cell_size > 0.0f &&
            std::isfinite(static_cast<double>(m_padding)) && m_padding >= 0.0f &&
            (m_glyph_cell_size - m_padding) > 0.0f;

        if (std::isfinite(sw) && std::isfinite(sh) && sw > 0.0 && sh > 0.0 && cell_ok) {
            const double sx = (m_glyph_cell_size - m_padding) / sw;
            const double sy = (m_glyph_cell_size - m_padding) / sh;
            const double atlas_generation_scale = std::min(sx, sy);

            if (std::isfinite(atlas_generation_scale) && atlas_generation_scale > 0.0) {
                const double pw = sw * atlas_generation_scale;
                const double ph = sh * atlas_generation_scale;

                if (std::isfinite(pw) && std::isfinite(ph) && pw >= 0.0 && ph >= 0.0) {
                    const double mx = (m_glyph_cell_size - pw) * 0.5;
                    const double my = (m_glyph_cell_size - ph) * 0.5;

                    if (std::isfinite(mx) && std::isfinite(my)) {
                        const float dx = static_cast<float>(mx / m_glyph_cell_size);
                        const float dy = static_cast<float>(my / m_glyph_cell_size);
                        const float dw = static_cast<float>(pw / m_glyph_cell_size);
                        const float dh = static_cast<float>(ph / m_glyph_cell_size);

                        unitless::vec4 uv{
                            full_cell_uv.x() + dx * full_cell_uv.z(),
                            full_cell_uv.y() + dy * full_cell_uv.w(),
                            dw * full_cell_uv.z(),
                            dh * full_cell_uv.w()
                        };

                        uv.z() = std::clamp(uv.z(), 0.0f, full_cell_uv.z());
                        uv.w() = std::clamp(uv.w(), 0.0f, full_cell_uv.w());
                        uv.x() = std::clamp(uv.x(), full_cell_uv.x(), full_cell_uv.x() + full_cell_uv.z());
                        uv.y() = std::clamp(uv.y(), full_cell_uv.y(), full_cell_uv.y() + full_cell_uv.w());

                        corrected_uv = uv;
                    }
                }
            }
        }

        if (emit_rect) {
            positioned_glyphs.emplace_back(positioned_glyph{
                .screen_rect = rect_t<unitless::vec2>::from_position_size(quad_pos, quad_size),
				.uv_rect = corrected_uv
            });
        }

        const float adv = std::isfinite(g.x_advance) ? (g.x_advance * scale) : fallback_advance;
        cursor.x() += adv;
        previous_glyph_index = static_cast<std::uint32_t>(g.ft_glyph_index);
    }

    return positioned_glyphs;
}

auto gse::font::line_height(const float scale) const -> float {
    return (m_ascender - m_descender) * scale;
}

auto gse::font::width(const std::string_view text, const float scale) const -> float {
    if (text.empty() || !m_face) {
        return 0.0f;
    }

    float total_width = 0.0f;
    std::uint32_t previous_glyph_index = 0;

    for (const char c : text) {
        auto it = m_glyphs.find(c);
        if (it == m_glyphs.end()) {
            continue;
        }

        const glyph& current_glyph = it->second;
        if (current_glyph.ft_glyph_index == 0) {
            continue;
        }

        if (previous_glyph_index != 0) {
            FT_Vector kerning_vector;
            FT_Get_Kerning(m_face, previous_glyph_index, static_cast<std::uint32_t>(current_glyph.ft_glyph_index), FT_KERNING_DEFAULT, &kerning_vector);
            total_width += (static_cast<float>(kerning_vector.x) / 64.0f) * scale;
        }

        total_width += current_glyph.x_advance * scale;
        previous_glyph_index = static_cast<std::uint32_t>(current_glyph.ft_glyph_index);
    }

    return total_width;
}
