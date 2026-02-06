export module gse.graphics:input_layers;

import std;

import gse.physics.math;
import gse.utility;

import :rendering_context;

namespace gse::gui {
	using ui_rect = rect_t<unitless::vec2>;
}

export namespace gse::gui {
	class input_layer {
	public:
		auto begin_frame() -> void;

		auto register_hit_region(
			render_layer layer,
			const ui_rect& rect
		) -> void;

		[[nodiscard]] auto input_layer_at(
			unitless::vec2 position
		) const -> render_layer;

		[[nodiscard]] auto input_available_at(
			render_layer widget_layer,
			unitless::vec2 position
		) const -> bool;
	private:
		std::array<std::vector<ui_rect>, 7> m_hit_regions;
	};
}

auto gse::gui::input_layer::begin_frame() -> void {
	for (auto& regions : m_hit_regions) {
		regions.clear();
	}
}

auto gse::gui::input_layer::register_hit_region(const render_layer layer, const ui_rect& rect) -> void {
	if (const auto index = static_cast<std::size_t>(layer); index < m_hit_regions.size()) {
		m_hit_regions[index].push_back(rect);
	}
}

auto gse::gui::input_layer::input_layer_at(const unitless::vec2 position) const -> render_layer {
	for (int i = static_cast<int>(m_hit_regions.size()) - 1; i >= 0; --i) {
		for (const auto& rect : m_hit_regions[i]) {
			if (rect.contains(position)) {
				return static_cast<render_layer>(i);
			}
		}
	}
	return render_layer::background;
}

auto gse::gui::input_layer::input_available_at(const render_layer widget_layer, const unitless::vec2 position) const -> bool {
	const auto topmost = input_layer_at(position);
	return static_cast<std::uint8_t>(widget_layer) >= static_cast<std::uint8_t>(topmost);
}
