export module gse.graphics:font;

import std;

import :texture;

import gse.math;
import gse.gpu;
import gse.core;
import gse.assets;
import gse.freetype;

export namespace gse {
    class glyph {
    public:
        struct info {
            float ft_glyph_index = 0;
            vec4f uv;
            vec2f size;
            vec2f bearing;
            float x_advance = 0;
            vec2f shape_size;
        };

        glyph(
        ) = default;

        glyph(
            const info& i
        );

        [[nodiscard]] auto ft_glyph_index(
        ) const -> float;

        [[nodiscard]] auto uv(
        ) const -> vec4f;

        [[nodiscard]] auto size(
        ) const -> vec2f;

        [[nodiscard]] auto bearing(
        ) const -> vec2f;

        [[nodiscard]] auto x_advance(
        ) const -> float;

        [[nodiscard]] auto shape_size(
        ) const -> vec2f;

    private:
        float m_ft_glyph_index = 0;
        vec4f m_uv;
        vec2f m_size;
        vec2f m_bearing;
        float m_x_advance = 0;
        vec2f m_shape_size;
    };

    struct positioned_glyph {
        const rect_t<vec2f> screen_rect;
        const vec4f uv_rect;
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

        [[nodiscard]] auto texture(
        ) const -> const gse::texture*;

        [[nodiscard]] auto text_layout(
            std::string_view text,
            vec2f start,
            float scale = 1.0f
        ) const -> std::vector<positioned_glyph>;

        [[nodiscard]] auto line_height(
            float scale = 1.0f
        ) const -> float;

        [[nodiscard]] auto width(
            std::string_view text,
            float scale = 1.0f
        ) const -> float;

        [[nodiscard]] auto pixel_range(
        ) const -> float;

        [[nodiscard]] auto glyph_cell_size(
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
