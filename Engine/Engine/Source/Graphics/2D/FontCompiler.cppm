export module gse.graphics:font_compiler;

import std;

import gse.os;
import gse.config;
import gse.assets;
import gse.gpu;
import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.freetype;
import gse.msdfgen;

import :font;

export namespace gse {

    auto bake(
        const std::filesystem::path& src,
        font::baked& out
    ) -> bool;
}

auto gse::bake(const std::filesystem::path& src, font::baked& out) -> bool {
    FT_Library ft_lib;
    if (FT_Init_FreeType(&ft_lib)) {
        log::println(log::level::error, log::category::assets, "Failed to initialize FreeType");
        return false;
    }

    FT_Face ft_face;
    if (FT_New_Face(ft_lib, src.string().c_str(), 0, &ft_face)) {
        log::println(log::level::error, log::category::assets, "Failed to load font face from '{}'", src.string());
        FT_Done_FreeType(ft_lib);
        return false;
    }

    constexpr int pixel_size = 64;
    FT_Set_Pixel_Sizes(ft_face, 0, pixel_size);

    const float ascender = static_cast<float>(ft_face->size->metrics.ascender) / 64.0f / pixel_size;
    const float descender = static_cast<float>(ft_face->size->metrics.descender) / 64.0f / pixel_size;

    msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
    msdfgen::FontHandle* font_handle = loadFont(ft_handle, src.string().c_str());
    if (!font_handle) {
        log::println(log::level::error, log::category::assets, "Failed to load font into msdfgen: {}", src.string());
        FT_Done_Face(ft_face);
        FT_Done_FreeType(ft_lib);
        return false;
    }

    constexpr int first_char = 32, last_char = 126;
    constexpr int glyph_count = last_char - first_char + 1;
    constexpr float glyph_cell_size = 64.f;

    constexpr int atlas_cols = 16;
    const int atlas_rows = static_cast<int>(std::ceil(glyph_count / static_cast<float>(atlas_cols)));
    const int atlas_width = atlas_cols * static_cast<int>(glyph_cell_size);
    const int atlas_height = atlas_rows * static_cast<int>(glyph_cell_size);

    std::vector<unsigned char> atlas_data(static_cast<std::size_t>(atlas_width) * atlas_height * 3, 0);
    std::unordered_map<char, glyph> glyphs;
    const msdfgen::Range pixel_range(4.0);
    int glyph_index = 0;

    for (int c = first_char; c <= last_char; ++c, ++glyph_index) {
        constexpr float padding = 8.f;
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

        FT_Load_Char(ft_face, c, freetype_load_default);
        const FT_GlyphSlot ft_glyph = ft_face->glyph;
        const float u0 = (glyph_index % atlas_cols * glyph_cell_size) / atlas_width;
        const float v0 = (glyph_index / atlas_cols * glyph_cell_size) / atlas_height;
        const float u1 = (glyph_index % atlas_cols * glyph_cell_size + glyph_cell_size) / atlas_width;
        const float v1 = (glyph_index / atlas_cols * glyph_cell_size + glyph_cell_size) / atlas_height;
        glyphs[static_cast<char>(c)] = glyph(glyph::info{
            .ft_glyph_index = static_cast<float>(ft_glyph->glyph_index),
            .uv = vec4f{ u0, v0, u1 - u0, v1 - v0 },
            .size = vec2f{
                (static_cast<float>(ft_glyph->metrics.width) / 64.0f) / pixel_size,
                (static_cast<float>(ft_glyph->metrics.height) / 64.0f) / pixel_size,
            },
            .bearing = vec2f{
                (static_cast<float>(ft_glyph->metrics.horiBearingX) / 64.0f) / pixel_size,
                (static_cast<float>(ft_glyph->metrics.horiBearingY) / 64.0f) / pixel_size,
            },
            .x_advance = (static_cast<float>(ft_glyph->advance.x) / 64.0f) / pixel_size,
            .shape_size = vec2f{ static_cast<float>(shape_w), static_cast<float>(shape_h) },
        });
    }

    destroyFont(font_handle);
    deinitializeFreetype(ft_handle);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);

    constexpr std::uint32_t channels = 4;
    std::vector<std::byte> rgba_data(static_cast<std::size_t>(atlas_width) * atlas_height * channels);
    for (int i = 0; i < atlas_width * atlas_height; ++i) {
        rgba_data[static_cast<std::size_t>(i) * 4 + 0] = static_cast<std::byte>(atlas_data[static_cast<std::size_t>(i) * 3 + 0]);
        rgba_data[static_cast<std::size_t>(i) * 4 + 1] = static_cast<std::byte>(atlas_data[static_cast<std::size_t>(i) * 3 + 1]);
        rgba_data[static_cast<std::size_t>(i) * 4 + 2] = static_cast<std::byte>(atlas_data[static_cast<std::size_t>(i) * 3 + 2]);
        rgba_data[static_cast<std::size_t>(i) * 4 + 3] = static_cast<std::byte>(255);
    }

    out.source_path_relative = src.lexically_relative(config::resource_path).string();
    out.ascender = ascender;
    out.descender = descender;
    out.atlas_width = static_cast<std::uint32_t>(atlas_width);
    out.atlas_height = static_cast<std::uint32_t>(atlas_height);
    out.channels = channels;
    out.rgba.storage = std::move(rgba_data);
    out.glyphs = std::move(glyphs);
    return true;
}
