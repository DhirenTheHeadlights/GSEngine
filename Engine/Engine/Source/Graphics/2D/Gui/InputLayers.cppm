export module gse.graphics:input_layers;

import std;

import gse.math;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

import :render_layer;

namespace gse::gui {
	using ui_rect = rect_t<vec2f>;
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
			vec2f position
		) const -> render_layer;

		[[nodiscard]] auto input_available_at(
			render_layer widget_layer,
			vec2f position
		) const -> bool;

		auto consume_press(
			mouse_button button
		) -> void;

		auto consume_release(
			mouse_button button
		) -> void;

		[[nodiscard]] auto is_press_consumed(
			mouse_button button
		) const -> bool;

		[[nodiscard]] auto is_release_consumed(
			mouse_button button
		) const -> bool;

		auto consume_scroll() -> void;

		[[nodiscard]] auto is_scroll_consumed() const -> bool;

		auto consume_key_press(
			key k
		) -> void;

		[[nodiscard]] auto is_key_press_consumed(
			key k
		) const -> bool;

	private:
		static constexpr std::size_t k_layer_count = 7;
		static constexpr std::size_t k_button_count = 8;

		std::array<std::vector<ui_rect>, k_layer_count> m_current_regions;
		std::array<std::vector<ui_rect>, k_layer_count> m_previous_regions;
		std::array<bool, k_button_count> m_press_consumed{};
		std::array<bool, k_button_count> m_release_consumed{};
		bool m_scroll_consumed = false;
		std::unordered_set<int> m_consumed_keys;
	};
}

namespace gse::gui {
	[[nodiscard]] constexpr auto mouse_button_index(
		mouse_button button
	) -> std::size_t;
}

constexpr auto gse::gui::mouse_button_index(const mouse_button button) -> std::size_t {
	return static_cast<std::size_t>(button);
}

auto gse::gui::input_layer::begin_frame() -> void {
	std::swap(m_current_regions, m_previous_regions);
	for (auto& regions : m_current_regions) {
		regions.clear();
	}
	m_press_consumed.fill(false);
	m_release_consumed.fill(false);
	m_scroll_consumed = false;
	m_consumed_keys.clear();
}

auto gse::gui::input_layer::register_hit_region(const render_layer layer, const ui_rect& rect) -> void {
	if (const auto index = static_cast<std::size_t>(layer); index < m_current_regions.size()) {
		m_current_regions[index].push_back(rect);
	}
}

auto gse::gui::input_layer::input_layer_at(const vec2f position) const -> render_layer {
	for (int i = static_cast<int>(m_previous_regions.size()) - 1; i >= 0; --i) {
		for (const auto& rect : m_previous_regions[i]) {
			if (rect.contains(position)) {
				return static_cast<render_layer>(i);
			}
		}
	}
	return render_layer::background;
}

auto gse::gui::input_layer::input_available_at(const render_layer widget_layer, const vec2f position) const -> bool {
	const auto topmost = input_layer_at(position);
	return static_cast<std::uint8_t>(widget_layer) >= static_cast<std::uint8_t>(topmost);
}

auto gse::gui::input_layer::consume_press(const mouse_button button) -> void {
	if (const auto i = mouse_button_index(button); i < m_press_consumed.size()) {
		m_press_consumed[i] = true;
	}
}

auto gse::gui::input_layer::consume_release(const mouse_button button) -> void {
	if (const auto i = mouse_button_index(button); i < m_release_consumed.size()) {
		m_release_consumed[i] = true;
	}
}

auto gse::gui::input_layer::is_press_consumed(const mouse_button button) const -> bool {
	if (const auto i = mouse_button_index(button); i < m_press_consumed.size()) {
		return m_press_consumed[i];
	}
	return false;
}

auto gse::gui::input_layer::is_release_consumed(const mouse_button button) const -> bool {
	if (const auto i = mouse_button_index(button); i < m_release_consumed.size()) {
		return m_release_consumed[i];
	}
	return false;
}

auto gse::gui::input_layer::consume_scroll() -> void {
	m_scroll_consumed = true;
}

auto gse::gui::input_layer::is_scroll_consumed() const -> bool {
	return m_scroll_consumed;
}

auto gse::gui::input_layer::consume_key_press(const key k) -> void {
	m_consumed_keys.insert(static_cast<int>(k));
}

auto gse::gui::input_layer::is_key_press_consumed(const key k) const -> bool {
	return m_consumed_keys.contains(static_cast<int>(k));
}
