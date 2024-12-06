#pragma once

#include <glm/glm.hpp>

namespace Engine::Renderer {
	void drawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
	void drawTexturedQuad(const glm::vec2& position, const glm::vec2& size, const GLuint textureID);
}