export module gse.graphics:font;

import <algorithm>;
import <fstream>;
import <iostream>;
import <unordered_map>;
import <vector>;
import <filesystem>;

import <msdfgen.h>;
#define STB_TRUETYPE_IMPLEMENTATION
import <freetype/freetype.h>;
import "ext/import-font.h";

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
        rect_t<unitless::vec2> uv_rect;
    };

    class font : public identifiable {
    public:
        font(const std::filesystem::path& path);
        ~font();

        static auto compile() -> std::set<std::filesystem::path>;

        auto load(const renderer::context& context) -> void;
        auto unload() -> void;

        auto texture() const -> const texture*;
        auto text_layout(std::string_view text, unitless::vec2 start, float scale = 1.0f) const -> std::vector<positioned_glyph>;
        auto line_height(float scale = 1.0f) const -> float;
		auto width(std::string_view text, float scale = 1.0f) const -> float;
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
            continue;
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

        std::ofstream out_file(baked_path, std::ios::binary);
        assert(out_file.is_open(), "Failed to open baked font file for writing.");

        constexpr uint32_t magic = 0x47464E54;
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

auto gse::font::load(const renderer::context& context) -> void {
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
        atlas_pixel_data,
        unitless::vec2u{ atlas_width, atlas_height },
        channels,
        texture::profile::msdf
    );

    m_texture->load(context);

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
    if (text.empty() || !m_face) {
	    return positioned_glyphs;
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
            FT_Vector kern{};
            if (FT_Get_Kerning(m_face, previous_glyph_index, static_cast<std::uint32_t>(g.ft_glyph_index), FT_KERNING_DEFAULT, &kern) == 0) {
                cursor.x() += static_cast<float>(kern.x) / 64.0f * scale;
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
                .uv_rect = rect_t<unitless::vec2>::from_position_size(
                    {
                    	corrected_uv.x(),
                    	corrected_uv.y()
                    },
                    {
                    	std::clamp(corrected_uv.z(), 0.0f, full_cell_uv.z()),
						std::clamp(corrected_uv.w(), 0.0f, full_cell_uv.w())
                    }
                )
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
