export module gse.graphics:font;

import std;

import :texture;

import gse.math;
import gse.gpu;
import gse.core;
import gse.assets;

extern "C" {
    struct FT_LibraryRec_;
    struct FT_FaceRec_;
}

export namespace gse {
    struct glyph {
        float ft_glyph_index = 0;
        float u0 = 0, v0 = 0;
        float u1 = 0, v1 = 0;
        float width = 0, height = 0;
        float x_offset = 0, y_offset = 0;
        float x_advance = 0;
        float shape_w = 0, shape_h = 0;

        auto uv(
        ) const -> vec4f;

        auto size(
        ) const -> vec2f;

        auto bearing(
        ) const -> vec2f;
    };

    struct positioned_glyph {
        rect_t<vec2f> screen_rect;
        vec4f uv_rect;
    };

    class font : public identifiable {
    public:
        explicit font(
            const std::filesystem::path& path
        );

        ~font();

        auto load(
            gpu::context& context
        ) -> void;

        auto unload(
        ) -> void;

        auto texture(
        ) const -> const texture*;

        auto text_layout(
            std::string_view text,
            vec2f start,
            float scale = 1.0f
        ) const -> std::vector<positioned_glyph>;

        auto line_height(
            float scale = 1.0f
        ) const -> float;

        auto width(
            std::string_view text,
            float scale = 1.0f
        ) const -> float;

        auto pixel_range(
        ) const -> float;

        auto glyph_cell_size(
        ) const -> float;

    private:
        std::unique_ptr<gse::texture> m_texture;
        std::unordered_map<char, glyph> m_glyphs;
        std::unordered_map<std::uint64_t, float> m_kerning;

        float m_glyph_cell_size = 64.f;
        float m_padding = 8.f;
        float m_ascender = 0.0f;
        float m_descender = 0.0f;

        FT_FaceRec_* m_face = nullptr;
        FT_LibraryRec_* m_ft = nullptr;

        std::filesystem::path m_baked_path;
    };
}
