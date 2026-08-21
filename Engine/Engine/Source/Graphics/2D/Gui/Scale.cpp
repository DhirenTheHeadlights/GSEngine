module gse.graphics:gui_scale_impl;

import std;

import gse.os;
import gse.config;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.save;

import :gui;
import :gui_scale;

import :types;
import :layout;
import :font;
import :ui_renderer;
import :texture;
import :cursor;
import :save;
import :ids;
import :input_layers;
import :settings;
import :styles;
import :builder;
import :menu_stack;
import :render_layer;
import :interaction;
import :symbols;
import :tab_strip;
import :widget_context;

auto gse::gui::intern_text(data& d, const std::string_view text) -> std::string_view {
	std::deque<std::string>& pool = d.text_pools[d.text_pool_slot];
	if (d.text_pool_used == pool.size()) {
		pool.emplace_back();
	}
	std::string& slot = pool[d.text_pool_used++];
	slot.assign(text);
	return slot;
}

auto gse::gui::usable_screen_rect(const float top_inset, const shared_view<window::data> window_s) -> rectf {
	const auto viewport_size = vec2f(window::viewport(window_s));
	const float usable_height = std::max(0.f, viewport_size.y() - top_inset);
	return rectf::from_position_size(
		{ 0.f, usable_height },
		{ viewport_size.x(), usable_height }
	);
}

auto gse::gui::sync_monitor_scale(data& d, viewport_state& vp, const std::string& monitor_key) -> void {
	if (monitor_key.empty()) {
		return;
	}

	if (monitor_key != vp.active_monitor_key) {
		if (!vp.active_monitor_key.empty()) {
			d.ui_scale_by_monitor[vp.active_monitor_key] = d.ui_scale;
		}

		if (const auto it = d.ui_scale_by_monitor.find(monitor_key); it != d.ui_scale_by_monitor.end()) {
			d.ui_scale = it->second;
		}

		vp.active_monitor_key = monitor_key;
	}

	d.ui_scale_by_monitor[vp.active_monitor_key] = d.ui_scale;
}

auto gse::gui::scale_factor_for(const data& d, const viewport_state& vp, const float viewport_height) -> float {
	constexpr float reference_height = 1080.f;
	const float base_scale = d.scale_with_resolution ? viewport_height / reference_height : vp.display_scale;
	return base_scale * d.ui_scale;
}

auto gse::gui::reload_font(data& d, const shared_view<asset::data> assets) -> void {
	if (const std::string& name = d.ui_font.value; !name.empty() && gse::exists(name)) {
		d.fonts.text = asset::get<font>(assets, name);
		d.fonts.registry[name] = d.fonts.text;
	}
	if (const std::string& name = d.code_font.value; !name.empty() && gse::exists(name)) {
		d.fonts.code = asset::get<font>(assets, name);
		d.fonts.registry[name] = d.fonts.code;
	}
}

auto gse::gui::apply_scale(const data& d, const viewport_state& vp, style sty, const float viewport_height) -> style {
	const float final_scale = scale_factor_for(d, vp, viewport_height);

	sty.scale_factor = final_scale;

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^style, std::meta::access_context::unchecked()))) {
		if constexpr (has_annotation<scaled_tag>(m)) {
			sty.[:m:] *= final_scale;
		}
	}

	return sty;
}
