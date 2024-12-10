#pragma once

#include <glm/glm.hpp>

#include "Texture.h"

namespace gse::renderer {
	void initialize2d();
	void begin_frame();
	void end_frame();
	void shutdown();

	void draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	void draw_quad(const glm::vec2& position, const glm::vec2& size, const Texture& texture);
}