export module gse.graphics.gui;

import std;
import glm;

export namespace gse::gui {
	auto initialize() -> void;
	auto render() -> void;
	auto shutdown() -> void;

	auto create_menu(const std::string& name, const glm::vec2& position, const glm::vec2& size, const std::function<void()>& contents) -> void;

	auto text(const std::string& text) -> void;
}
