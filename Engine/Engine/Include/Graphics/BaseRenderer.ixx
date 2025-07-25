export module gse.graphics:base_renderer;

import std;

import :rendering_context;
import :render_component;

import gse.utility;

export namespace gse {
	class base_renderer {
	public:
		explicit base_renderer(renderer::context& context) : m_context(context) {}
		virtual ~base_renderer() = default;

		virtual auto initialize() -> void = 0;
		virtual auto render(std::span<std::reference_wrapper<registry>> registries) -> void = 0;
	protected:
		renderer::context& m_context;
	};
}