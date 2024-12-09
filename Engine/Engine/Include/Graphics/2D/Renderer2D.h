#pragma once

#include <glm/glm.hpp>

#include "Texture.h"

namespace Engine::Renderer {
	void initialize2d();
	void beginFrame();
	void endFrame();
	void shutdown();

	void drawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	void drawQuad(const glm::vec2& position, const glm::vec2& size, const Texture& texture);
}