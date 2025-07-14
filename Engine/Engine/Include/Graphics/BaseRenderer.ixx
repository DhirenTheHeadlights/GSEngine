export module gse.graphics:base_renderer;

import std;

import :rendering_context;
import :render_component;

import gse.utility;

export namespace gse {
	class base_renderer {
	public:
		explicit base_renderer(const std::unique_ptr<renderer::context>& context, registry& registry) : m_context(context.get()), m_registry(registry) {}
		virtual ~base_renderer() = default;

		virtual auto initialize() -> void = 0;
		virtual auto render() -> void = 0;
	protected:
		renderer::context* m_context;
		registry& m_registry;
	};
}