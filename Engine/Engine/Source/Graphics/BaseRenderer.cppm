export module gse.graphics:base_renderer;

import std;

import :rendering_context;
import :render_component;

import gse.utility;
import gse.physics.math;

export namespace gse {
	class base_renderer {
	public:
		explicit base_renderer(renderer::context& context) : m_context(context) {}
		virtual ~base_renderer() = default;

		virtual auto initialize() -> void = 0;
		virtual auto update(std::span<const std::reference_wrapper<registry>> registries) -> void {}
		virtual auto render(std::span<const std::reference_wrapper<registry>> registries) -> void = 0;
	protected:
		renderer::context& m_context;

		static auto to_vulkan_scissor(const rect_t<unitless::vec2>& rect, const unitless::vec2& window_size) -> vk::Rect2D {
			return {
				.offset = {
					static_cast<std::int32_t>(rect.left()),
					static_cast<std::int32_t>(window_size.y() - rect.top())
				},
				.extent = {
					static_cast<std::uint32_t>(rect.width()),
					static_cast<std::uint32_t>(rect.height())
				}
			};
		}
	};
}