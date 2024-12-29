#pragma once

#include <functional>
#include <string>
#include <glm/glm.hpp>

namespace gse::gui {
	void initialize();
	void render();
	void shutdown();

	void create_menu(const std::string& name, const glm::vec2& position, const glm::vec2& size, const std::function<void()>& contents);

	void text(const std::string& text);
}
