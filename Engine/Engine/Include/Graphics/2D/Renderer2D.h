#pragma once

#include <glm/glm.hpp>

#include "Texture.h"

namespace gse::renderer2d {
	void initialize();
	void shutdown();

	void draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	void draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture);
}