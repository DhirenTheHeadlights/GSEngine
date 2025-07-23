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
        font(const std::filesystem::path& path);
        ~font();

		static auto compile() -> std::set<std::filesystem::path>;

        auto load(renderer::context& context) -> void;
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

		std::filesystem::path m_baked_path;
    };
}

gse::font::font(const std::filesystem::path& path) : identifiable(path), m_baked_path(path) {
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

auto gse::font::compile() -> std::set<std::filesystem::path> {
    const auto source_root = config::resource_path / "Fonts";
    const auto baked_root = config::baked_resource_path / "Fonts";

    if (!exists(source_root)) return {};

    if (!exists(baked_root)) {
        create_directories(baked_root);
    }

    std::println("Compiling fonts...");

    const std::vector<std::string> supported_extensions = { ".ttf", ".otf" };
    std::set<std::filesystem::path> resources;

    for (const auto& entry : std::filesystem::directory_iterator(source_root)) {
        if (!entry.is_regular_file() || std::ranges::find(supported_extensions, entry.path().extension().string()) == supported_extensions.end()) {
            continue;
        }

        const auto source_path = entry.path();
        const auto baked_path = baked_root / source_path.filename().replace_extension(".gfont");
        resources.insert(baked_path);

        if (exists(baked_path) && last_write_time(source_path) <= last_write_time(baked_path)) {
            continue; // Up-to-date
        }

        FT_Library ft_lib;
        if (FT_Init_FreeType(&ft_lib)) {
            std::println(stderr, "Error: Failed to initialize FreeType.");
            continue;
        }

        FT_Face ft_face;
        if (FT_New_Face(ft_lib, source_path.string().c_str(), 0, &ft_face)) {
            std::println(stderr, "Error: Failed to load font face from '{}'.", source_path.string());
            FT_Done_FreeType(ft_lib);
            continue;
        }

        constexpr int pixel_size = 64;
        FT_Set_Pixel_Sizes(ft_face, 0, pixel_size);

        const float ascender = static_cast<float>(ft_face->size->metrics.ascender) / 64.0f / pixel_size;
        const float descender = static_cast<float>(ft_face->size->metrics.descender) / 64.0f / pixel_size;

        msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
        msdfgen::FontHandle* font_handle = loadFont(ft_handle, source_path.string().c_str());
        assert(font_handle, "Failed to load font into msdfgen.");

        constexpr int first_char = 32, last_char = 126;
        constexpr int glyph_count = last_char - first_char + 1;
        constexpr float glyph_cell_size = 64.f, padding = 8.f;

        constexpr int atlas_cols = 16;
        const int atlas_rows = static_cast<int>(std::ceil(glyph_count / static_cast<float>(atlas_cols)));
        const int atlas_width = atlas_cols * static_cast<int>(glyph_cell_size);
        const int atlas_height = atlas_rows * static_cast<int>(glyph_cell_size);

        std::vector<unsigned char> atlas_data(static_cast<size_t>(atlas_width) * atlas_height * 3, 0);
        std::unordered_map<char, glyph> glyphs;
        const msdfgen::Range pixel_range(4.0);
        int glyph_index = 0;

        for (int c = first_char; c <= last_char; ++c, ++glyph_index) {
            msdfgen::Shape shape;
            if (!loadGlyph(shape, font_handle, c)) continue;

            shape.normalize();
            edgeColoringSimple(shape, 3.0);
            msdfgen::Bitmap<float, 3> msdf_bitmap(glyph_cell_size, glyph_cell_size);

            const double shape_w = shape.getBounds().r - shape.getBounds().l;
            const double shape_h = shape.getBounds().t - shape.getBounds().b;
            const double scale = std::min((glyph_cell_size - padding) / shape_w, (glyph_cell_size - padding) / shape_h);
            const double tx = -shape.getBounds().l + (glyph_cell_size / scale - shape_w) / 2.0;
            const double ty = -shape.getBounds().b + (glyph_cell_size / scale - shape_h) / 2.0;

            generateMSDF(msdf_bitmap, shape, pixel_range, { scale, scale }, { tx, ty });

            for (int y = 0; y < msdf_bitmap.height(); ++y) {
                for (int x = 0; x < msdf_bitmap.width(); ++x) {
                    const int atlas_x = glyph_index % atlas_cols * glyph_cell_size + x;
                    const int atlas_y = glyph_index / atlas_cols * glyph_cell_size + y;
                    const int idx = (atlas_y * atlas_width + atlas_x) * 3;
                    atlas_data[idx + 0] = static_cast<unsigned char>(std::clamp(msdf_bitmap(x, y)[0], 0.f, 1.f) * 255.f);
                    atlas_data[idx + 1] = static_cast<unsigned char>(std::clamp(msdf_bitmap(x, y)[1], 0.f, 1.f) * 255.f);
                    atlas_data[idx + 2] = static_cast<unsigned char>(std::clamp(msdf_bitmap(x, y)[2], 0.f, 1.f) * 255.f);
                }
            }

            FT_Load_Char(ft_face, c, FT_LOAD_DEFAULT);
            const FT_GlyphSlot ft_glyph = ft_face->glyph;
            glyphs[static_cast<char>(c)] = {
                .ft_glyph_index = static_cast<float>(ft_glyph->glyph_index),
                .u0 = (glyph_index % atlas_cols * glyph_cell_size) / atlas_width,
                .v0 = (glyph_index / atlas_cols * glyph_cell_size) / atlas_height,
                .u1 = (glyph_index % atlas_cols * glyph_cell_size + glyph_cell_size) / atlas_width,
                .v1 = (glyph_index / atlas_cols * glyph_cell_size + glyph_cell_size) / atlas_height,
                .width = (static_cast<float>(ft_glyph->metrics.width) / 64.0f) / pixel_size,
                .height = (static_cast<float>(ft_glyph->metrics.height) / 64.0f) / pixel_size,
                .x_offset = (static_cast<float>(ft_glyph->metrics.horiBearingX) / 64.0f) / pixel_size,
                .y_offset = (static_cast<float>(ft_glyph->metrics.horiBearingY) / 64.0f) / pixel_size,
                .x_advance = (static_cast<float>(ft_glyph->advance.x) / 64.0f) / pixel_size,
                .shape_w = static_cast<float>(shape_w),
                .shape_h = static_cast<float>(shape_h)
            };
        }

        destroyFont(font_handle);
        deinitializeFreetype(ft_handle);
        FT_Done_Face(ft_face);
        FT_Done_FreeType(ft_lib);

        std::ofstream out_file(baked_path, std::ios::binary);
        assert(out_file.is_open(), "Failed to open baked font file for writing.");

        constexpr uint32_t magic = 0x47464E54; // 'GFNT'
        constexpr uint32_t version = 1;
        out_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out_file.write(reinterpret_cast<const char*>(&version), sizeof(version));

        const std::string relative_src_str = source_path.lexically_relative(config::resource_path).string();
        uint64_t path_len = relative_src_str.length();
        out_file.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
        out_file.write(relative_src_str.c_str(), path_len);

        out_file.write(reinterpret_cast<const char*>(&ascender), sizeof(ascender));
        out_file.write(reinterpret_cast<const char*>(&descender), sizeof(descender));

        constexpr uint32_t channels = 4;
        std::vector<std::byte> rgba_data(static_cast<size_t>(atlas_width) * atlas_height * channels);
        for (int i = 0; i < atlas_width * atlas_height; ++i) {
            rgba_data[static_cast<size_t>(i) * 4 + 0] = static_cast<std::byte>(atlas_data[static_cast<size_t>(i) * 3 + 0]);
            rgba_data[static_cast<size_t>(i) * 4 + 1] = static_cast<std::byte>(atlas_data[static_cast<size_t>(i) * 3 + 1]);
            rgba_data[static_cast<size_t>(i) * 4 + 2] = static_cast<std::byte>(atlas_data[static_cast<size_t>(i) * 3 + 2]);
            rgba_data[static_cast<size_t>(i) * 4 + 3] = static_cast<std::byte>(255);
        }

        const uint32_t u_atlas_width = atlas_width, u_atlas_height = atlas_height;
        out_file.write(reinterpret_cast<const char*>(&u_atlas_width), sizeof(u_atlas_width));
        out_file.write(reinterpret_cast<const char*>(&u_atlas_height), sizeof(u_atlas_height));
        out_file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
        uint64_t data_size = rgba_data.size();
        out_file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
        out_file.write(reinterpret_cast<const char*>(rgba_data.data()), data_size);

        uint64_t glyph_map_size = glyphs.size();
        out_file.write(reinterpret_cast<const char*>(&glyph_map_size), sizeof(glyph_map_size));
        for (const auto& [ch, glyph_data] : glyphs) {
            out_file.write(&ch, sizeof(ch));
            out_file.write(reinterpret_cast<const char*>(&glyph_data), sizeof(glyph_data));
        }

        out_file.close();
        std::print("Font compiled: {}\n", baked_path.filename().string());
    }

    return resources;
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

auto gse::font::load(renderer::context& context) -> void {
    std::ifstream in_file(m_baked_path, std::ios::binary);
    assert(in_file.is_open(), std::format( 
        "Failed to open baked font file: {}",
        m_baked_path.string()
	));

    uint32_t magic, version;
    in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in_file.read(reinterpret_cast<char*>(&version), sizeof(version));
    assert(magic == 0x47464E54 && version == 1, std::format( 
        "Invalid baked font file format or version: {}",
		m_baked_path.string()
	));

    uint64_t path_len;
    in_file.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
    std::string relative_src_str(path_len, '\0');
    in_file.read(&relative_src_str[0], path_len);
    const auto source_font_path = config::resource_path / relative_src_str;

    in_file.read(reinterpret_cast<char*>(&m_ascender), sizeof(m_ascender));
    in_file.read(reinterpret_cast<char*>(&m_descender), sizeof(m_descender));

    uint32_t atlas_width, atlas_height, channels;
    uint64_t data_size;
    in_file.read(reinterpret_cast<char*>(&atlas_width), sizeof(atlas_width));
    in_file.read(reinterpret_cast<char*>(&atlas_height), sizeof(atlas_height));
    in_file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
    in_file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
    std::vector<std::byte> atlas_pixel_data(data_size);
    in_file.read(reinterpret_cast<char*>(atlas_pixel_data.data()), data_size);

    m_texture = std::make_unique<gse::texture>(
        std::format("msdf_font_atlas_{}", m_baked_path.stem().string()),
        context.config(),
        atlas_pixel_data,
        unitless::vec2u{ atlas_width, atlas_height },
        channels,
        texture::profile::msdf
    );

    uint64_t glyph_count;
    in_file.read(reinterpret_cast<char*>(&glyph_count), sizeof(glyph_count));
    m_glyphs.reserve(glyph_count);
    for (uint64_t i = 0; i < glyph_count; ++i) {
        char ch;
        glyph glyph_data;
        in_file.read(&ch, sizeof(ch));
        in_file.read(reinterpret_cast<char*>(&glyph_data), sizeof(glyph_data));
        m_glyphs[ch] = glyph_data;
    }

    assert(FT_Init_FreeType(&m_ft) == 0, "Failed to initialize FreeType.");
    assert(FT_New_Face(m_ft, source_font_path.string().c_str(), 0, &m_face) == 0, "Failed to load source font face for kerning.");
    assert(FT_Set_Pixel_Sizes(m_face, 0, 64) == 0, "Failed to set font pixel size for kerning.");
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
