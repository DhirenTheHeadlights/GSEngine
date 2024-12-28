#pragma once

#include <glm/glm.hpp>

#include "Font.h"
#include "Texture.h"

namespace gse::renderer2d {
	void initialize();
	void shutdown();

	void draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	void draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture);
	void draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture, const glm::vec4& uv);

	void draw_text(const font& font, const std::string& text, const glm::vec2& position, float scale, const glm::vec4& color);
}