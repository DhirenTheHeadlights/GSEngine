module;

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>

#include <msdfgen.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <filesystem>
#include <freetype/freetype.h>

#include "ext/import-font.h"

export module gse.graphics.font;

import gse.graphics.texture;

export namespace gse {
    struct glyph {
        float u0 = 0, v0 = 0;
        float u1 = 0, v1 = 0;
        float width = 0, height = 0;
        float x_offset = 0, y_offset = 0;
        float x_advance = 0;
    };

    class font {
    public:
        font() = default;
		font(const std::filesystem::path& path);

        auto load(const std::filesystem::path& path) -> void;

        auto get_character(char c) const -> const glyph&;
        auto get_texture() const -> const texture&;
    private:
        texture m_texture;
        std::unordered_map<char, glyph> m_glyphs;
    };
}

gse::font::font(const std::filesystem::path& path) {
    load(path);
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

auto gse::font::load(const std::filesystem::path& path) -> void {
    std::vector<unsigned char> font_data_buffer;
    if (!read_file_binary(path, font_data_buffer)) {
        std::cerr << "Could not load font file!" << '\n';
        return;
    }

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Failed to initialize FreeType library!" << '\n';
        return;
    }

    FT_Face face;
    const FT_Error error = FT_New_Memory_Face(
        ft,
        font_data_buffer.data(),
        static_cast<FT_Long>(font_data_buffer.size()),
        0, // face index
        &face
    );
    if (error) {
        std::cerr << "Failed to create FreeType face from memory data." << '\n';
        FT_Done_FreeType(ft);
        return;
    }

    msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
    if (!ft_handle) {
        std::cerr << "Failed to initialize msdfgen Freetype handle." << '\n';
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
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
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return;
    }

    constexpr int first_char = 32;
    constexpr int last_char = 126;
    constexpr int glyph_count = last_char - first_char + 1;
    constexpr int glyph_cell_size = 64;
    constexpr int atlas_cols = 16;

    const int atlas_rows = static_cast<int>(std::ceil(
        glyph_count / static_cast<float>(atlas_cols)
    ));

    constexpr int atlas_width = atlas_cols * glyph_cell_size;
    const int atlas_height = atlas_rows * glyph_cell_size;

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

        msdfgen::Bitmap<float, 3> msdf_bitmap(glyph_cell_size, glyph_cell_size);

        double scale_x = (glyph_cell_size - 2.0) / (shape.getBounds().r - shape.getBounds().l);
        double scale_y = (glyph_cell_size - 2.0) / (shape.getBounds().t - shape.getBounds().b);
        const double scale = std::min(scale_x, scale_y);
        msdfgen::Vector2 scale_vec(scale, scale);
        msdfgen::Vector2 translate_vec(-shape.getBounds().l, -shape.getBounds().b);

        generateMSDF(
            msdf_bitmap,
            shape,
            pixel_range,
            scale_vec,
            translate_vec,
            msdfgen::ErrorCorrectionConfig(),
            true // overlapSupport
        );

        if (msdf_bitmap.width() != glyph_cell_size || msdf_bitmap.height() != glyph_cell_size) {
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

                // Flip Y-axis if needed
                const int atlas_x = glyph_index % atlas_cols * glyph_cell_size + x;
                const int atlas_y = glyph_index / atlas_cols * glyph_cell_size
                    + (msdf_bitmap.height() - 1 - y);
                const int idx = (atlas_y * atlas_width + atlas_x) * 3;

                atlas_data[idx + 0] = static_cast<unsigned char>(r * 255.0f);
                atlas_data[idx + 1] = static_cast<unsigned char>(g * 255.0f);
                atlas_data[idx + 2] = static_cast<unsigned char>(b * 255.0f);
            }
        }

        if (FT_Load_Char(face, c, FT_LOAD_DEFAULT)) {
            std::cerr << "Warning: Could not load FreeType glyph metrics for character: " << static_cast<char>(c) << '\n';
            continue;
        }

        const FT_GlyphSlot glyph_slot = face->glyph;

        // Convert metrics from 26.6 fixed point to float
        const float advance = static_cast<float>(glyph_slot->advance.x) / 64.0f;
        const float bearing_x = static_cast<float>(glyph_slot->metrics.horiBearingX) / 64.0f;
        const float bearing_y = static_cast<float>(glyph_slot->metrics.horiBearingY) / 64.0f;
        const float width = static_cast<float>(glyph_slot->metrics.width) / 64.0f;
        const float height = static_cast<float>(glyph_slot->metrics.height) / 64.0f;

        // UV coordinates in the atlas
        const float u0 = static_cast<float>(glyph_index % atlas_cols * glyph_cell_size)
            / static_cast<float>(atlas_width);
        const float v0 = static_cast<float>(glyph_index / atlas_cols * glyph_cell_size)
            / static_cast<float>(atlas_height);
        const float u1 = static_cast<float>(glyph_index % atlas_cols * glyph_cell_size + glyph_cell_size)
            / static_cast<float>(atlas_width);
        const float v1 = static_cast<float>(glyph_index / atlas_cols * glyph_cell_size + glyph_cell_size)
            / static_cast<float>(atlas_height);

        m_glyphs[static_cast<char>(c)] = { u0, v0, u1, v1, width, height, bearing_x, bearing_y, advance };
    }

    GLuint texture_id = m_texture.get_texture_id();
    if (texture_id == 0) {
        glGenTextures(1, &texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        atlas_width,
        atlas_height,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        atlas_data.data()
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    destroyFont(font_handle);
    deinitializeFreetype(ft_handle);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    std::cout << "Successfully loaded font with MSDF: " << path << "\n";
}

auto gse::font::get_character(const char c) const -> const glyph& {
    static glyph fallback;

    if (const auto it = m_glyphs.find(c); it != m_glyphs.end()) {
        return it->second;
    }
    return fallback;
}

auto gse::font::get_texture() const -> const gse::texture& {
    return m_texture;
}