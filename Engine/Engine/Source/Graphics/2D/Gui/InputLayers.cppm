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
}

export namespace gse::gui {
	class input_layer {
	public:
		auto begin_frame() -> void;

		auto register_hit_region(
			render_layer layer,
			std::uint32_t z_order,
			const rectf& rect
		) -> void;

		[[nodiscard]] auto input_available_at(
			render_layer widget_layer,
			std::uint32_t widget_z,
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

		auto register_resize_block(
			const rectf& rect
		) -> void;

		[[nodiscard]] auto is_resize_blocked(
			vec2f position
		) const -> bool;

	private:
		struct hit_region {
			std::uint32_t z_order = 0;
			rectf rect;
		};

		[[nodiscard]] auto topmost_at(
			vec2f position
		) const -> std::pair<std::uint8_t, std::uint32_t>;

		static constexpr std::size_t k_layer_count = 7;
		static constexpr std::size_t k_button_count = 8;

		std::array<std::vector<hit_region>, k_layer_count> m_current_regions;
		std::array<std::vector<hit_region>, k_layer_count> m_previous_regions;
		std::array<bool, k_button_count> m_press_consumed{};
		std::array<bool, k_button_count> m_release_consumed{};
		bool m_scroll_consumed = false;
		std::unordered_set<int> m_consumed_keys;
		double_buffer<std::vector<rectf>> m_resize_blocks;
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
	m_resize_blocks.flip();
	m_resize_blocks.write().clear();
	m_press_consumed.fill(false);
	m_release_consumed.fill(false);
	m_scroll_consumed = false;
	m_consumed_keys.clear();
}

auto gse::gui::input_layer::register_hit_region(const render_layer layer, const std::uint32_t z_order, const rectf& rect) -> void {
	if (const auto index = static_cast<std::size_t>(layer); index < m_current_regions.size()) {
		m_current_regions[index].push_back({ z_order, rect });
	}
}

auto gse::gui::input_layer::register_resize_block(const rectf& rect) -> void {
	m_resize_blocks.write().push_back(rect);
}

auto gse::gui::input_layer::is_resize_blocked(const vec2f position) const -> bool {
	for (const rectf& rect : m_resize_blocks.read()) {
		if (rect.contains(position)) {
			return true;
		}
	}
	return false;
}

auto gse::gui::input_layer::topmost_at(const vec2f position) const -> std::pair<std::uint8_t, std::uint32_t> {
	for (int i = static_cast<int>(m_previous_regions.size()) - 1; i >= 0; --i) {
		std::uint32_t best_z = 0;
		bool found = false;
		for (const auto& region : m_previous_regions[i]) {
			if (region.rect.contains(position)) {
				found = true;
				best_z = std::max(best_z, region.z_order);
			}
		}
		if (found) {
			return { static_cast<std::uint8_t>(i), best_z };
		}
	}
	return { static_cast<std::uint8_t>(render_layer::background), 0 };
}

auto gse::gui::input_layer::input_available_at(const render_layer widget_layer, const std::uint32_t widget_z, const vec2f position) const -> bool {
	const auto [top_layer, top_z] = topmost_at(position);
	if (const auto wl = static_cast<std::uint8_t>(widget_layer); wl != top_layer) {
		return wl > top_layer;
	}
	return widget_z >= top_z;
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
