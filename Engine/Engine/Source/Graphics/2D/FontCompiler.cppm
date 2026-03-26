module;

#include <msdfgen.h>
#include <freetype/freetype.h>
#include "ext/import-font.h"

export module gse.graphics:font_compiler;

import std;

import gse.platform;
import gse.assert;

import :font;

export template<>
struct gse::asset_compiler<gse::font> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".ttf", ".otf" };
    }

    static auto baked_extension() -> std::string {
        return ".gfont";
    }

    static auto source_directory() -> std::string {
        return "Fonts";
    }

    static auto baked_directory() -> std::string {
        return "Fonts";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        FT_Library ft_lib;
        if (FT_Init_FreeType(&ft_lib)) {
            std::println(stderr, "Error: Failed to initialize FreeType.");
            return false;
        }

        FT_Face ft_face;
        if (FT_New_Face(ft_lib, source.string().c_str(), 0, &ft_face)) {
            std::println(stderr, "Error: Failed to load font face from '{}'.", source.string());
            FT_Done_FreeType(ft_lib);
            return false;
        }

        constexpr int pixel_size = 64;
        FT_Set_Pixel_Sizes(ft_face, 0, pixel_size);

        const float ascender = static_cast<float>(ft_face->size->metrics.ascender) / 64.0f / pixel_size;
        const float descender = static_cast<float>(ft_face->size->metrics.descender) / 64.0f / pixel_size;

        msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
        msdfgen::FontHandle* font_handle = loadFont(ft_handle, source.string().c_str());
        if (!font_handle) {
            std::println(stderr, "Error: Failed to load font into msdfgen: {}", source.string());
            FT_Done_Face(ft_face);
            FT_Done_FreeType(ft_lib);
            return false;
        }

        constexpr int first_char = 32, last_char = 126;
        constexpr int glyph_count = last_char - first_char + 1;
        constexpr float glyph_cell_size = 64.f, padding = 8.f;

        constexpr int atlas_cols = 16;
        const int atlas_rows = static_cast<int>(std::ceil(glyph_count / static_cast<float>(atlas_cols)));
        const int atlas_width = atlas_cols * static_cast<int>(glyph_cell_size);
        const int atlas_height = atlas_rows * static_cast<int>(glyph_cell_size);

        std::vector<unsigned char> atlas_data(static_cast<std::size_t>(atlas_width) * atlas_height * 3, 0);
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

        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out_file(destination, std::ios::binary);
        if (!out_file.is_open()) {
            std::println(stderr, "Error: Failed to open baked font file for writing: {}", destination.string());
            return false;
        }

        constexpr std::uint32_t magic = 0x47464E54;
        constexpr std::uint32_t version = 1;
        out_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out_file.write(reinterpret_cast<const char*>(&version), sizeof(version));

        const std::string relative_src_str = source.lexically_relative(config::resource_path).string();
        std::uint64_t path_len = relative_src_str.length();
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

        const std::uint32_t u_atlas_width = atlas_width, u_atlas_height = atlas_height;
        out_file.write(reinterpret_cast<const char*>(&u_atlas_width), sizeof(u_atlas_width));
        out_file.write(reinterpret_cast<const char*>(&u_atlas_height), sizeof(u_atlas_height));
        out_file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
        std::uint64_t data_size = rgba_data.size();
        out_file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
        out_file.write(reinterpret_cast<const char*>(rgba_data.data()), data_size);

        std::uint64_t glyph_map_size = glyphs.size();
        out_file.write(reinterpret_cast<const char*>(&glyph_map_size), sizeof(glyph_map_size));
        for (const auto& [ch, glyph_data] : glyphs) {
            out_file.write(&ch, sizeof(ch));
            out_file.write(reinterpret_cast<const char*>(&glyph_data), sizeof(glyph_data));
        }

        std::println("Font compiled: {}", destination.filename().string());
        return true;
    }

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        if (!std::filesystem::exists(destination)) {
            return true;
        }
        return std::filesystem::last_write_time(source) > std::filesystem::last_write_time(destination);
    }

    static auto dependencies(
        const std::filesystem::path&
    ) -> std::vector<std::filesystem::path> {
        return {};
    }
};
