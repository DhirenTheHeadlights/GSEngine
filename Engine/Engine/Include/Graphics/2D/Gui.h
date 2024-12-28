#pragma once

#include <functional>
#include <string>
#include <glm/glm.hpp>

namespace gse::gui {
	void initialize();
	void render();
	void shutdown();

	void create_menu(const std::string& name, const std::function<void()>& contents, const glm::vec2& position, const glm::vec2& size);

	void text(const std::string& text);
}