#include "Graphics/2D/Font.h"

#include <fstream>
#include <iostream>
#include <msdfgen.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <vector>
#include <freetype/freetype.h>

#include "tests/caveview/glext.h"

gse::font::font(const std::string& path) {
	load(path);
}

namespace {
	bool read_file_binary(const std::string& path, std::vector<unsigned char>& out_data) {
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
}

void gse::font::load(const std::string& path) {
    std::vector<unsigned char> font_data_buffer;
    if (!read_file_binary(path, font_data_buffer)) {
        std::cerr << "Could not load font file!" << std::endl;
        return;
    }

    constexpr int bitmap_width = 512;
    constexpr int bitmap_height = 512;

    std::vector<unsigned char> baked_data_buffer(bitmap_width * bitmap_height, 0);

    constexpr int first_char = 32;
    constexpr int num_chars = 96;

    stbtt_bakedchar baked_chars[num_chars];
    const float pixel_height = 128;

    const int result = stbtt_BakeFontBitmap(
        font_data_buffer.data(),     
        0,                           // font index in file (for .ttc or multiple fonts)
        pixel_height,                
        baked_data_buffer.data(),
        bitmap_width,
        bitmap_height,
        first_char,                         
        num_chars,                         
        baked_chars
    );

    if (result <= 0) {
        std::cerr << "Failed to bake font bitmap: " << path << std::endl;
        return;
    }

	GLuint texture_id = m_texture.get_texture_id();

    if (texture_id == 0) {
        glGenTextures(1, &texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        bitmap_width,
        bitmap_height,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        baked_data_buffer.data()
    );

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    for (int i = 0; i < num_chars; ++i) { const auto& [x0, y0, x1, y1, x_offset, y_offset, x_advance] = baked_chars[i];
        char c = static_cast<char>(first_char + i);

        const float u0 = static_cast<float>(x0) / static_cast<float>(bitmap_width);
        const float v0 = static_cast<float>(y0) / static_cast<float>(bitmap_height);
        const float u1 = static_cast<float>(x1) / static_cast<float>(bitmap_width);
        const float v1 = static_cast<float>(y1) / static_cast<float>(bitmap_height);

        m_glyphs[c] = {
			.u0 = u0, .v0 = v0,
			.u1 = u1, .v1 = v1,
			.width = static_cast<float>(x1 - x0),
			.height = static_cast<float>(y1 - y0),
			.x_offset = x_offset,
			.y_offset = y_offset,
			.x_advance = x_advance
		};
    }

    std::cout << "Successfully loaded font: " << path << '\n';
}

const gse::glyph& gse::font::get_character(const char c) const {
    static glyph fallback;

    if (const auto it = m_glyphs.find(c); it != m_glyphs.end()) {
        return it->second;
    }
    return fallback;
}

const gse::texture& gse::font::get_texture() const {
	return m_texture;
}