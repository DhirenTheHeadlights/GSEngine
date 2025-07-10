export module gse.graphics:base_renderer;

import std;

import :rendering_context;
import :render_component;

export namespace gse {
	class base_renderer {
	public:
		explicit base_renderer(const std::unique_ptr<renderer::context>& context) : m_context(context.get()) {}
		virtual ~base_renderer() = default;

		virtual auto initialize() -> void = 0;
		virtual auto render(std::span<render_component> components) -> void = 0;
	protected:
		renderer::context* m_context;
	};
}