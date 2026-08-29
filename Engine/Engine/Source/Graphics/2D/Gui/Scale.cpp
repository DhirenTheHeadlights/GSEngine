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

auto gse::gui::usable_screen_rect(const float top_inset, const rectf& frame_rect) -> rectf {
	const float usable_height = std::max(0.f, frame_rect.height() - top_inset);
	return rectf::from_position_size(
		{ frame_rect.left(), frame_rect.top() - top_inset },
		{ frame_rect.width(), usable_height }
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

auto gse::gui::font_available(const std::span<const std::string> available, const std::string_view name) -> bool {
	return !name.empty() && std::ranges::find(available, name) != available.end();
}

auto gse::gui::variant_of(const std::span<const std::string> available, const std::string_view base, const std::span<const std::string_view> suffixes) -> std::string {
	constexpr std::string_view regular = "-Regular";
	if (!base.ends_with(regular)) {
		return {};
	}

	const std::string_view stem = base.substr(0, base.size() - regular.size());
	for (const std::string_view suffix : suffixes) {
		std::string candidate = std::string(stem) + std::string(suffix);
		if (font_available(available, candidate)) {
			return candidate;
		}
	}
	return {};
}

auto gse::gui::assign_faces(font_set& fonts, const shared_view<asset::data> assets, const std::string& ui_name, const std::string& code_name) -> void {
	const std::vector<std::string> available = asset::enumerate_resources<font>();

	auto load = [&fonts, assets, &available](const std::string_view name) -> resource::handle<font> {
		if (!font_available(available, name)) {
			return {};
		}
		const std::string key(name);
		resource::handle<font> loaded = asset::get<font>(assets, key);
		fonts.registry[key] = loaded;
		return loaded;
	};

	constexpr std::array strong_suffixes{
		std::string_view("-SemiBold"),
		std::string_view("-Bold"),
		std::string_view("-ExtraBold"),
	};
	constexpr std::array emphasis_suffixes{
		std::string_view("-Italic"),
	};

	if (font_available(available, ui_name)) {
		fonts.text = load(ui_name);
		fonts.text_strong = load(variant_of(available, ui_name, strong_suffixes));
		fonts.text_emphasis = load(variant_of(available, ui_name, emphasis_suffixes));
	}
	if (font_available(available, code_name)) {
		fonts.code = load(code_name);
		fonts.code_strong = load(variant_of(available, code_name, strong_suffixes));
	}
}

auto gse::gui::reload_font(data& d, const shared_view<asset::data> assets) -> void {
	assign_faces(d.fonts, assets, d.ui_font.value, d.code_font.value);
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
