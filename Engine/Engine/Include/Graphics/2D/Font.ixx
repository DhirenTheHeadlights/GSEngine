export module gse.graphics.font;

import std;
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
		font(const std::string& path);

		auto load(const std::string& path) -> void;

		auto get_character(char c) const -> const glyph&;
		auto get_texture() const -> const texture&;
	private:
		texture m_texture;
		std::unordered_map<char, glyph> m_glyphs;
	};
}
