module;

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <msdfgen.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <filesystem>
#include <freetype/freetype.h>
#include "ext/import-font.h"

export module gse.graphics:font;

import :texture;

import gse.assert;
import gse.physics.math;
import gse.platform;

export namespace gse {
    struct glyph {
		float ft_glyph_index = 0; 
        float u0 = 0, v0 = 0;
        float u1 = 0, v1 = 0;
        float width = 0, height = 0;
        float x_offset = 0, y_offset = 0;
        float x_advance = 0;
		float shape_w = 0, shape_h = 0; 

        auto uv() const -> unitless::vec4 {
            return { u0, v0, u1 - u0, v1 - v0 };
		}

        auto size() const -> unitless::vec2 {
            return { width, height };
		}

        auto bearing() const -> unitless::vec2 {
            return { x_offset, y_offset };
        }
    };

    struct positioned_glyph {
        unitless::vec2 position;
        unitless::vec2 size;
        unitless::vec4 uv;
    };

	class font : public identifiable {
    public:
        struct handle {
			handle(font& f) : font(&f) {}
            font* font;
        };

        font(const std::filesystem::path& path);
        ~font();

        auto load(const vulkan::config& config) -> void;
		auto unload() -> void;

        auto texture() const -> const texture*;
        auto text_layout(std::string_view text, unitless::vec2 start, float scale = 1.0f) const -> std::vector<positioned_glyph>;
        auto line_height(float scale = 1.0f) const -> float;
    private:
        std::unique_ptr<gse::texture> m_texture;
        std::unordered_map<char, glyph> m_glyphs;

        float m_glyph_cell_size = 64.f;
        float m_padding = 8.f;
        float m_ascender = 0.0f;
        float m_descender = 0.0f;

		FT_Face m_face = nullptr;
        FT_Library m_ft = nullptr;

		std::filesystem::path m_path;
    };
}

gse::font::font(const std::filesystem::path& path) : identifiable(path.stem().string()), m_path(path) {
    assert(
        exists(path),
        std::format("Font file '{}' does not exist.", path.string())
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

auto read_file_binary(const std::filesystem::path& path, std::vector<unsigned char>& out_data) -> bool {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open font file: " << path << '\n';
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streampos size = file.tellg();
    file.seekg(0, std::ios::beg);

    out_data.resize(size);
    file.read(reinterpret_cast<char*>(out_data.data()), size);
    file.close();

    return true;
}

auto gse::font::load(const vulkan::config& config) -> void {
    std::vector<unsigned char> font_data_buffer;
    if (!read_file_binary(m_path, font_data_buffer)) {
        std::cerr << "Could not load font file!" << '\n';
        return;
    }

    if (FT_Init_FreeType(&m_ft)) {
        std::cerr << "Failed to initialize FreeType library!" << '\n';
        return;
    }

    const FT_Error error = FT_New_Memory_Face(
        m_ft,
        font_data_buffer.data(),
        static_cast<FT_Long>(font_data_buffer.size()),
        0, // face index
        &m_face
    );
    if (error) {
        std::cerr << "Failed to create FreeType face from memory data." << '\n';
        FT_Done_FreeType(m_ft);
        return;
    }

    constexpr int pixel_size = 64;
    if (FT_Set_Pixel_Sizes(m_face, 0, pixel_size)) {
        std::cerr << "Failed to set FreeType font size to 64px." << '\n';
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_ft);
        return;
    }

    const float ascender_in_pixels = static_cast<float>(m_face->size->metrics.ascender) / 64.0f;
    const float descender_in_pixels = static_cast<float>(m_face->size->metrics.descender) / 64.0f;

    m_ascender = ascender_in_pixels / pixel_size;
    m_descender = descender_in_pixels / pixel_size;

    msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
    if (!ft_handle) {
        std::cerr << "Failed to initialize msdfgen Freetype handle." << '\n';
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_ft);
        return;
    }

    msdfgen::FontHandle* font_handle = loadFontData(
        ft_handle,
        font_data_buffer.data(),
        static_cast<int>(font_data_buffer.size())
    );
    if (!font_handle) {
        std::cerr << "Failed to load font into msdfgen." << '\n';
        deinitializeFreetype(ft_handle);
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_ft);
        return;
    }

    constexpr int first_char = 32;
    constexpr int last_char = 126;
    constexpr int glyph_count = last_char - first_char + 1;

    constexpr int atlas_cols = 16;
    const int atlas_rows = static_cast<int>(std::ceil(glyph_count / static_cast<float>(atlas_cols)));
    const int atlas_width = atlas_cols * static_cast<int>(m_glyph_cell_size);
    const int atlas_height = atlas_rows * static_cast<int>(m_glyph_cell_size);

    std::vector<unsigned char> atlas_data(atlas_width * atlas_height * 3, 0);
    const msdfgen::Range pixel_range(4.0);

    int glyph_index = 0;
    for (int c = first_char; c <= last_char; ++c, ++glyph_index) {
	    msdfgen::Shape shape;
        if (!loadGlyph(shape, font_handle, c)) {
            std::cerr << "Warning: Could not load glyph for character: "
                << static_cast<char>(c) << '\n';
            continue;
        }

        shape.normalize();
        edgeColoringSimple(shape, 3.0);

        msdfgen::Bitmap<float, 3> msdf_bitmap(static_cast<int>(m_glyph_cell_size), static_cast<int>(m_glyph_cell_size));

        const double shape_w = shape.getBounds().r - shape.getBounds().l;
        const double shape_h = shape.getBounds().t - shape.getBounds().b;

        const double scale = std::min(
            (m_glyph_cell_size - m_padding) / shape_w,
            (m_glyph_cell_size - m_padding) / shape_h
        );
        const msdfgen::Vector2 scale_vec(scale, scale);

        const double tx = -shape.getBounds().l + (m_glyph_cell_size / scale - shape_w) / 2.0;
        const double ty = -shape.getBounds().b + (m_glyph_cell_size / scale - shape_h) / 2.0;
        const msdfgen::Vector2 translate_vec(tx, ty);

        generateMSDF(
            msdf_bitmap,
            shape,
            pixel_range,
            scale_vec,
            translate_vec,
            msdfgen::ErrorCorrectionConfig()
        );

        if (msdf_bitmap.width() != m_glyph_cell_size || msdf_bitmap.height() != m_glyph_cell_size) {
            std::cerr << "Warning: MSDF bitmap size mismatch for character: "
                << static_cast<char>(c) << std::endl;
            continue;
        }

        // Copy MSDF bitmap into our RGB atlas_data
        for (int y = 0; y < msdf_bitmap.height(); ++y) {
            for (int x = 0; x < msdf_bitmap.width(); ++x) {
                const float r = std::clamp(msdf_bitmap(x, y)[0], 0.0f, 1.0f);
                const float g = std::clamp(msdf_bitmap(x, y)[1], 0.0f, 1.0f);
                const float b = std::clamp(msdf_bitmap(x, y)[2], 0.0f, 1.0f);

                const int atlas_x = glyph_index % atlas_cols * m_glyph_cell_size + x;
                const int atlas_y = glyph_index / atlas_cols * m_glyph_cell_size + y;
                const int idx = (atlas_y * atlas_width + atlas_x) * 3;

                atlas_data[idx + 0] = static_cast<unsigned char>(r * 255.0f);
                atlas_data[idx + 1] = static_cast<unsigned char>(g * 255.0f);
                atlas_data[idx + 2] = static_cast<unsigned char>(b * 255.0f);
            }
        }

        if (FT_Load_Char(m_face, c, FT_LOAD_DEFAULT)) {
            std::cerr << "Warning: Could not load FreeType glyph metrics for character: " << static_cast<char>(c) << '\n';
            continue;
        }

        const FT_GlyphSlot glyph_slot = m_face->glyph;

        // Convert metrics from 26.6 fixed point to float
        const float advance = static_cast<float>(glyph_slot->advance.x) / 64.0f / pixel_size;
        const float bearing_x = static_cast<float>(glyph_slot->metrics.horiBearingX) / 64.0f / pixel_size;
        const float bearing_y = static_cast<float>(glyph_slot->metrics.horiBearingY) / 64.0f / pixel_size;
        const float width = static_cast<float>(glyph_slot->metrics.width) / 64.0f / pixel_size;
        const float height = static_cast<float>(glyph_slot->metrics.height) / 64.0f / pixel_size;

        // UV coordinates in the atlas
        const float u0 = glyph_index % atlas_cols * m_glyph_cell_size / static_cast<float>(atlas_width);
        const float v0 = glyph_index / atlas_cols * m_glyph_cell_size / static_cast<float>(atlas_height);
        const float u1 = (glyph_index % atlas_cols * m_glyph_cell_size + m_glyph_cell_size) / static_cast<float>(atlas_width);
        const float v1 = (glyph_index / atlas_cols * m_glyph_cell_size + m_glyph_cell_size) / static_cast<float>(atlas_height);

        m_glyphs[static_cast<char>(c)] = {
            .ft_glyph_index = static_cast<float>(glyph_slot->glyph_index),
            .u0 = u0,
            .v0 = v0,
            .u1 = u1,
            .v1 = v1,
            .width = width,
            .height = height,
            .x_offset = bearing_x,
            .y_offset = bearing_y,
            .x_advance = advance,
            .shape_w = static_cast<float>(shape_w),
            .shape_h = static_cast<float>(shape_h)
        };
    }

    // Convert atlas_data from 3-channel (RGB) to 4-channel (RGBA)
    std::vector<std::byte> rgba_data(static_cast<std::size_t>(atlas_width)* atlas_height * 4);
    for (int i = 0; i < atlas_width * atlas_height; ++i) {
        rgba_data[static_cast<std::size_t>(i) * 4 + 0] = static_cast<std::byte>(atlas_data[static_cast<std::size_t>(i) * 3 + 0]);
        rgba_data[static_cast<std::size_t>(i) * 4 + 1] = static_cast<std::byte>(atlas_data[static_cast<std::size_t>(i) * 3 + 1]);
        rgba_data[static_cast<std::size_t>(i) * 4 + 2] = static_cast<std::byte>(atlas_data[static_cast<std::size_t>(i) * 3 + 2]);
        rgba_data[static_cast<std::size_t>(i) * 4 + 3] = static_cast<std::byte>(255);
    }

	m_texture = std::make_unique<gse::texture>(
        config,
        rgba_data,
        unitless::vec2u{ static_cast<std::uint32_t>(atlas_width), static_cast<std::uint32_t>(atlas_height) },
        4,
        texture::profile::msdf
    );

    destroyFont(font_handle);
    deinitializeFreetype(ft_handle);

    std::cout << "Successfully loaded font with MSDF: " << m_path << "\n";
}

auto gse::font::unload() -> void {
	m_path = std::filesystem::path();
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
    if (text.empty() || !m_face) return positioned_glyphs;

    auto baseline = start;
    baseline.y -= m_ascender * scale;

	auto cursor = baseline;
    std::uint32_t previous_glyph_index = 0;

    for (const char c : text) {
        auto it = m_glyphs.find(c);

        if (it == m_glyphs.end()) continue;

        const glyph& current_glyph = it->second;
        if (current_glyph.ft_glyph_index == 0) continue;

        if (previous_glyph_index != 0) {
            FT_Vector kerning_vector;
            FT_Get_Kerning(m_face, previous_glyph_index, static_cast<std::uint32_t>(current_glyph.ft_glyph_index), FT_KERNING_DEFAULT, &kerning_vector);
            cursor.x += static_cast<float>(kerning_vector.x) / 64.0f * scale;
        }

        const auto quad_pos = unitless::vec2{
		    cursor.x + current_glyph.bearing().x * scale,
            cursor.y + current_glyph.bearing().y * scale
        };
        const auto quad_size = current_glyph.size() * scale;

        const unitless::vec4 full_cell_uv = current_glyph.uv();

        const double atlas_generation_scale = std::min(
            (m_glyph_cell_size - m_padding) / current_glyph.shape_w,
            (m_glyph_cell_size - m_padding) / current_glyph.shape_h
        );

        const double glyph_pixel_width = current_glyph.shape_w * atlas_generation_scale;
        const double glyph_pixel_height = current_glyph.shape_h * atlas_generation_scale;

        const double margin_x = (m_glyph_cell_size - glyph_pixel_width) / 2.0;
        const double margin_y = (m_glyph_cell_size - glyph_pixel_height) / 2.0;

        const unitless::vec4 corrected_uv{
            full_cell_uv.x + static_cast<float>(margin_x) / m_glyph_cell_size * full_cell_uv.z,
            full_cell_uv.y + static_cast<float>(margin_y) / m_glyph_cell_size * full_cell_uv.w,
            static_cast<float>(glyph_pixel_width) / m_glyph_cell_size * full_cell_uv.z,
            static_cast<float>(glyph_pixel_height) / m_glyph_cell_size * full_cell_uv.w
        };

        positioned_glyphs.emplace_back(
            positioned_glyph{
	            .position = quad_pos,
	            .size = quad_size,
				.uv = corrected_uv
            }
        );

        cursor.x += current_glyph.x_advance * scale;
        previous_glyph_index = static_cast<std::uint32_t>(current_glyph.ft_glyph_index);
    }
    return positioned_glyphs;
}

auto gse::font::line_height(const float scale) const -> float {
	return (m_ascender - m_descender) * scale;
}
