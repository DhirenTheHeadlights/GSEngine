#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <string>

namespace gse::gui {
	void initialize();
	void shutdown();

	void create_menu(const std::string& name, const std::function<void>& contents, const glm::vec2& position, const glm::vec2& size);
}