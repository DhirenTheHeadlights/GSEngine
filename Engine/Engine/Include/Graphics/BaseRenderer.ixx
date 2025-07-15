export module gse.graphics:base_renderer;

import std;

import :rendering_context;
import :render_component;

import gse.utility;

export namespace gse {
	class base_renderer {
	public:
		explicit base_renderer(renderer::context& context, const std::span<std::reference_wrapper<registry>> registries) : m_context(context), m_registries(registries) {}
		virtual ~base_renderer() = default;

		virtual auto initialize() -> void = 0;
		virtual auto render() -> void = 0;
	protected:
		renderer::context& m_context;
		std::span<std::reference_wrapper<registry>> m_registries;
	};
}