export module gse.graphics.renderer2d;

import std;
import glm;

import gse.graphics.font;
import gse.graphics.texture;

export namespace gse::renderer2d {
	auto initialize() -> void;
	auto shutdown() -> void;

	auto draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) -> void;
	auto draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture) -> void;
	auto draw_quad(const glm::vec2& position, const glm::vec2& size, const texture& texture, const glm::vec4& uv) -> void;

	auto draw_text(const font& font, const std::string& text, const glm::vec2& position, float scale, const glm::vec4& color) -> void;
}