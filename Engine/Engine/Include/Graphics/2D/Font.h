#pragma once

#include <string>
#include <unordered_map>

#include "Texture.h"

namespace gse {
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

		void load(const std::string& path);

		const glyph& get_character(char c) const;
		const texture& get_texture() const;
	private:
		texture m_texture;
		std::unordered_map<char, glyph> m_glyphs;
	};
}
