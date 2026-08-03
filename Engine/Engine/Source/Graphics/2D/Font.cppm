export module gse.graphics:font;

import std;

import :texture;

import gse.math;
import gse.gpu;
import gse.core;
import gse.assets;
import gse.containers;
import gse.freetype;

export namespace gse {
	class glyph {
	public:
		struct info {
			std::uint32_t ft_glyph_index = 0;
			vec4f atlas_uv;
			vec4f plane_bounds;
			float x_advance = 0;
		};

		glyph() = default;

		glyph(
			const info& i
		);

		[[nodiscard]] auto ft_glyph_index() const -> std::uint32_t;

		[[nodiscard]] auto atlas_uv() const -> vec4f;

		[[nodiscard]] auto plane_bounds() const -> vec4f;

		[[nodiscard]] auto x_advance() const -> float;

	private:
		std::uint32_t m_ft_glyph_index = 0;
		vec4f m_atlas_uv;
		vec4f m_plane_bounds;
		float m_x_advance = 0;
	};

	struct positioned_glyph {
		const rect_t<vec2f> screen_rect;
		const vec4f uv_rect;
	};

	class [[= asset::boot_critical{}]] font : public identifiable {
	public:
		struct [[
			= asset_format::baked_ext<".gfont">{},
			= asset_format::baked_dir<"Fonts">{},
			= asset_format::source_dir<"Fonts">{},
			= asset_format::source_exts<".ttf", ".otf">{},
			= asset_format::built_ins<"Inter-Regular.ttf", "MonaspaceNeon-Regular.otf">{},
			= asset_format::magic<0x47464E54>{},
			= asset_format::version<11>{}
		]] baked {
			std::string source_path_relative;
			float ascender = 0.0f;
			float descender = 0.0f;
			float pixel_range = 0.0f;
			std::uint32_t atlas_width = 0;
			std::uint32_t atlas_height = 0;
			std::uint32_t channels = 0;
			raw_blob_owned<std::byte> rgba;
			std::unordered_map<char32_t, glyph> glyphs;
		};

		explicit font(
			const std::filesystem::path& path
		);

		~font();

		auto load(
			asset::load_ctx& ctx
		) -> async::task<>;

		auto unload() -> void;

		[[nodiscard]] auto texture() const -> const gse::texture*;

		[[nodiscard]]
		auto text_layout(
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

		[[nodiscard]] auto caret_offsets(
			std::string_view text,
			float scale = 1.0f
		) const -> std::vector<float>;

		[[nodiscard]] auto vertical_center_offset(
			float scale
		) const -> float;

		[[nodiscard]] auto ascender_height(
			float scale
		) const -> float;

		[[nodiscard]] auto max_glyph_top(
			float scale
		) const -> float;

		[[nodiscard]] auto min_glyph_bottom(
			float scale
		) const -> float;

		[[nodiscard]] auto pixel_range() const -> float;

	private:
		std::unique_ptr<gse::texture> m_texture;
		std::unordered_map<char32_t, glyph> m_glyphs;
		std::unordered_map<std::uint64_t, float> m_kerning;

		float m_ascender = 0.0f;
		float m_descender = 0.0f;
		float m_pixel_range = 0.0f;
		float m_max_glyph_top = 0.0f;
		float m_min_glyph_bottom = 0.0f;

		FT_Face m_face = nullptr;
		FT_Library m_ft = nullptr;

		std::filesystem::path m_baked_path;
	};
}
