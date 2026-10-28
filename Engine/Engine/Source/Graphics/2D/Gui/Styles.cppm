export module gse.graphics:styles;

import std;

import gse.math;
import gse.meta;

import :widget_skin;

export namespace gse::gui {
	enum class theme {
		midnight,
		eclipse,
		ember,
		forest,
		frost,
		blueprint,
		high_contrast,
		neutral,
	};

	struct style;

	struct [[= style_group]]
	button_skin {
		state_set<surface> surfaces{};
		state_set<vec4f> label_colors{};
		label_spec label{};

		[[= scaled]]
		float glyph_gap = 0.f;
	};

	struct [[= style_group]]
	item_skin {
		state_set<surface> surfaces{};
		state_set<vec4f> label_colors{};
		label_spec label{};
		vec4f detail_color{};
		surface accent_bar{};
		surface divider{};
	};

	struct [[= style_group]]
	menu_skin {
		surface floating{};
		surface docked{};
		surface shadow{};
		surface border{};
		surface title_bar{};
		surface accent_bar{};
		surface overlay_body{};
		surface overlay_title_bar{};
		surface overlay_border{};
	};

	struct [[= style_group]]
	dock_skin {
		surface preview{};
		state_set<surface> target{};
		state_set<vec4f> marker_colors{};
	};

	struct [[= style_group]]
	screen_skin {
		surface backdrop{};
		surface body{};
		label_spec caption{};
		vec4f caption_color{};
		label_spec title{};
		vec4f title_color{};
		label_spec entry_name{};
		label_spec entry_path{};
		label_spec note{};
		surface preview{};
	};

	struct [[= style_group]]
	progress_skin {
		surface track{};
		surface fill{};
		surface fill_error{};
		label_spec label{};
		vec4f label_color{};
	};

	struct [[= style_group]]
	overlay_skin {
		surface header{};
		surface row{};
		label_spec cell{};
		vec4f cell_color{};
		surface drag_ghost{};
		surface drag_ghost_detached{};
		surface tooltip{};
		vec4f text_color{};
		vec4f glyph_color{};
	};

	struct [[= style_group]]
	context_menu_skin {
		surface separator{};
		surface row_hover{};
		state_set<vec4f> label_colors{};
		vec4f destructive_color{};

		[[= scaled]]
		float row_inset = 0.f;
	};

	struct [[= style_group]]
	icon_button_skin {
		state_set<surface> surfaces{};
		state_set<vec4f> icon_colors{};
	};

	struct [[= style_group]]
	modal_skin {
		surface scrim{};
		surface shadow{};
		surface panel{};
		label_spec title{};
		vec4f title_color{};
		label_spec message{};
		vec4f message_color{};
	};

	struct [[= style_group]]
	tab_skin {
		state_set<surface> cell{};
		state_set<vec4f> caption_colors{};
		state_set<vec4f> glyph_colors{};
		surface accent_bar{};

		[[= scaled]]
		float gap = 0.f;

		[[= scaled]]
		float scroll_bar_extent = 0.f;

		[[= scaled]]
		float wheel_step = 0.f;
	};

	struct [[= style_group]]
	scrollbar_skin {
		surface track{};
		state_set<surface> thumb{};

		[[= scaled]]
		float min_thumb = 0.f;
	};

	struct [[= style_group]]
	section_skin {
		surface accent_bar{};
		label_spec title{};
		vec4f title_color{};
		label_spec subtitle{};
		vec4f subtitle_color{};
		item_skin action{};
	};

	struct [[= style_group]]
	input_skin {
		label_spec label{};
		vec4f label_color{};
		surface box{};
		surface area{};
		surface gutter{};
		vec4f text_color{};
		surface selection{};
		surface caret{};

		[[= scaled]]
		float text_padding = 0.f;

		[[= scaled]]
		float caret_width = 0.f;
	};

	struct [[= style_group]]
	slider_skin {
		label_spec label{};
		vec4f label_color{};
		surface track{};
		surface fill{};
		label_spec value{};
		vec4f value_color{};
	};

	struct [[= style_group]]
	checkbox_skin {
		label_spec label{};
		state_set<vec4f> label_colors{};
		state_set<vec4f> frame{};
		state_set<vec4f> mark{};
	};

	struct [[= style_group]]
	dropdown_skin {
		label_spec label{};
		vec4f label_color{};
		item_skin header{};
		item_skin option{};
		surface panel{};
		vec4f arrow_color{};
	};

	struct [[= style_group]]
	toggle_skin {
		label_spec label{};
		vec4f label_color{};
		surface track_on{};
		surface track_off{};
		surface knob{};
		vec4f knob_hovered{};

		[[= scaled]]
		float track_width = 0.f;

		[[= scaled]]
		float track_height = 0.f;

		[[= scaled]]
		float knob_inset = 0.f;
	};

	struct dp {
		float value{};

		[[nodiscard]] constexpr auto px(
			const style& sty
		) const -> float;
	};

	struct dp_vec {
		vec2f value{};

		[[nodiscard]] constexpr auto px(
			const style& sty
		) const -> vec2f;
	};

	struct palette {
		// Menu chrome
		vec4f color_title_bar = { 0.10f, 0.14f, 0.20f, 1.0f };
		vec4f color_menu_body = { 0.08f, 0.12f, 0.16f, 0.15f };
		vec4f color_panel_alt = { 0.06f, 0.10f, 0.14f, 0.25f };
		vec4f color_separator = { 0.18f, 0.26f, 0.32f, 1.0f };

		// Interactive widget states
		vec4f color_widget_background = { 0.10f, 0.16f, 0.22f, 0.35f };
		vec4f color_widget_hovered = { 0.14f, 0.24f, 0.32f, 1.0f };
		vec4f color_widget_active = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_widget_selected = { 0.26f, 0.86f, 0.84f, 0.32f };

		// Buttons (distinct from passive widget surfaces so CTAs read as clickable)
		vec4f color_button_background = { 0.14f, 0.30f, 0.36f, 0.35f };
		vec4f color_button_hovered = { 0.20f, 0.44f, 0.50f, 1.0f };
		vec4f color_button_accent = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_button_accent_hovered = { 0.42f, 0.94f, 0.92f, 1.0f };
		vec4f color_button_accent_text = { 0.04f, 0.09f, 0.12f, 1.0f };
		vec4f color_button_ghost = { 0.f, 0.f, 0.f, 0.f };

		// Sliders / toggles
		vec4f color_slider_fill = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_toggle_on = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_toggle_off = { 0.22f, 0.25f, 0.34f, 1.0f };
		vec4f color_handle = { 0.96f, 0.97f, 0.99f, 1.0f };
		vec4f color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Inputs
		vec4f color_input_background = { 0.06f, 0.08f, 0.12f, 0.35f };
		vec4f color_selection = { 0.26f, 0.86f, 0.84f, 0.40f };
		vec4f color_caret = { 0.96f, 0.97f, 0.99f, 1.0f };
		vec4f color_danger = { 0.62f, 0.22f, 0.22f, 1.0f };
		vec4f color_danger_hovered = { 0.78f, 0.28f, 0.28f, 1.0f };

		// Tabs (own surface so strips read as chrome rather than widgets)
		vec4f color_tab_background = { 0.07f, 0.12f, 0.17f, 0.35f };
		vec4f color_tab_hovered = { 0.12f, 0.20f, 0.28f, 1.0f };
		vec4f color_tab_active = { 0.16f, 0.26f, 0.34f, 1.0f };

		// Docking
		vec4f color_dock_preview = { 0.26f, 0.86f, 0.84f, 0.35f };

		vec4f color_shadow = { 0.f, 0.f, 0.f, 0.35f };
		vec4f color_scrim = { 0.f, 0.f, 0.f, 0.45f };
	};

	struct style {
		vec4f color_border = { 0.22f, 0.32f, 0.42f, 1.0f };

		// Text
		vec4f color_text = { 0.96f, 0.97f, 0.99f, 1.0f };
		vec4f color_text_secondary = { 0.66f, 0.70f, 0.80f, 1.0f };
		vec4f color_text_disabled = { 0.40f, 0.43f, 0.50f, 1.0f };
		vec4f color_section_header = { 0.96f, 0.97f, 0.99f, 1.0f };

		// Icons
		vec4f color_icon = { 0.70f, 0.74f, 0.84f, 1.0f };
		vec4f color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f };
		vec4f color_folder = { 0.86f, 0.74f, 0.45f, 1.0f };
		vec4f color_file = { 0.55f, 0.66f, 0.82f, 1.0f };
		[[= scaled]] float icon_extent = 16.f;

		// Accent (primary theme color: drives sliders, toggle-on, selections, section bars)
		vec4f color_accent = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_accent_dim = { 0.26f, 0.86f, 0.84f, 0.30f };

		vec4f color_warning = { 0.71f, 0.57f, 0.11f, 1.0f };
		vec4f color_error = { 0.855f, 0.451f, 0.424f, 1.0f };
		vec4f color_fatal = { 0.94f, 0.30f, 0.30f, 1.0f };
		vec4f color_added = { 0.44f, 0.72f, 0.46f, 1.0f };
		vec4f color_removed = { 0.82f, 0.44f, 0.44f, 1.0f };

		[[= scaled]] float modal_shadow_offset = 4.f;

		// Runtime-resolved scale factor (set by apply_scale, not a styled dimension)
		float scale_factor = 1.f;

		// Widget sizing (auto-scaled via [[= gse::scaled]] reflection in apply_scale)
		[[= scaled]] float padding = 12.f;
		[[= scaled]] float title_bar_height = 32.f;
		[[= scaled]] float resize_border_thickness = 8.f;
		[[= scaled]] vec2f min_menu_size = { 200.f, 120.f };
		[[= scaled]] float font_size = 16.f;
		std::filesystem::path font;

		[[= scaled]] float corner_radius = 6.f;
		[[= scaled]] float corner_radius_menu = 10.f;
		[[= scaled]] float corner_radius_button = 6.f;
		[[= scaled]] float corner_radius_input = 6.f;
		float widget_height_padding = 0.7f;
		float label_column_ratio = 0.4f;
		[[= scaled]] float item_spacing = 4.f;
		[[= scaled]] float section_spacing_above = 18.f;
		[[= scaled]] float section_spacing_below = 10.f;
		float section_header_size_mult = 1.30f;
		[[= scaled]] float accent_bar_width = 3.f;
		[[= scaled]] float bare_header_height = 6.f;

		// Screen/card layout (referenced by SettingsScreen, MainMenuScreen, etc.)
		[[= scaled]] vec2f card_min_size = { 720.f, 480.f };
		[[= scaled]] vec2f card_max_size = { 1100.f, 760.f };
		[[= scaled]] vec2f card_margin = { 80.f, 60.f };
		[[= scaled]] float sidebar_width = 220.f;
		[[= scaled]] float header_height = 56.f;
		[[= scaled]] float footer_height = 64.f;
		[[= scaled]] float close_button_size = 28.f;
		[[= scaled]] float separator_thickness = 1.f;

		// Buttons
		[[= scaled]] float button_height = 32.f;
		[[= scaled]] float button_min_width = 108.f;
		[[= scaled]] float accent_button_min_width = 132.f;
		[[= scaled]] float button_spacing = 8.f;

		// Standalone panels
		[[= scaled]] float side_panel_max_width = 320.f;
		[[= scaled]] float progress_bar_max_width = 400.f;
		[[= scaled]] float progress_bar_height = 12.f;
		[[= scaled]] float preview_height = 160.f;

		button_skin button_standard{};
		button_skin button_accent{};
		button_skin button_danger{};
		button_skin button_ghost{};
		item_skin list_item{};
		item_skin nav_item{};
		dropdown_skin dropdown{};
		input_skin input{};
		slider_skin slider{};
		checkbox_skin checkbox{};
		scrollbar_skin scrollbar{};
		section_skin section{};
		tab_skin tab{};
		modal_skin modal{};
		icon_button_skin icon_button{};
		icon_button_skin icon_button_danger{};
		icon_button_skin caption_button{};
		icon_button_skin caption_button_danger{};
		icon_button_skin window_caption{};
		icon_button_skin window_caption_danger{};
		dock_skin dock{};
		menu_skin menu{};
		screen_skin screen{};
		progress_skin progress{};
		overlay_skin overlay{};
		context_menu_skin context_menu{};
		surface graph_node{};
		surface separator_line{};
		surface panel_alt{};
		toggle_skin toggle{};

		static constexpr auto midnight() -> style;
		static constexpr auto eclipse() -> style;
		static constexpr auto ember() -> style;
		static constexpr auto forest() -> style;
		static constexpr auto frost() -> style;
		static constexpr auto blueprint() -> style;
		static constexpr auto high_contrast() -> style;
		static constexpr auto neutral() -> style;

		static constexpr auto from_theme(
			theme t
		) -> style;
	};

	[[nodiscard]] constexpr auto accent_bar_extent(
		const style& sty
	) -> float;

	[[nodiscard]] auto style_key(
		const style& sty
	) -> std::uint64_t;

	struct button_role_info {
		button_skin style::* skin = &style::button_standard;
	};

	enum class button_role : std::uint8_t {
		standard
		[[= button_role_info{}]],
		accent
		[[= button_role_info{
			.skin = &style::button_accent,
		}]],
		danger
		[[= button_role_info{
			.skin = &style::button_danger,
		}]],
		ghost
		[[= button_role_info{
			.skin = &style::button_ghost,
		}]]
	};

	[[nodiscard]] auto skin_for(
		const style& sty,
		button_role role
	) -> button_skin;

	[[nodiscard]] constexpr auto with_widget_skins(
		const palette& pal,
		style sty
	) -> style;

	[[nodiscard]] constexpr auto with_outlined_buttons(
		const palette& pal,
		style sty
	) -> style;
}

namespace gse::gui {
	static_assert(
		has_annotation<style_group_tag>(^^surface)
			&& has_annotation<style_group_tag>(^^state_set<surface>)
			&& has_annotation<style_group_tag>(^^state_set<vec4f>)
			&& has_annotation<style_group_tag>(^^label_spec)
			&& has_annotation<style_group_tag>(^^button_skin)
			&& has_annotation<style_group_tag>(^^item_skin)
			&& has_annotation<style_group_tag>(^^dropdown_skin)
			&& has_annotation<style_group_tag>(^^input_skin)
			&& has_annotation<style_group_tag>(^^slider_skin)
			&& has_annotation<style_group_tag>(^^checkbox_skin)
			&& has_annotation<style_group_tag>(^^scrollbar_skin)
			&& has_annotation<style_group_tag>(^^section_skin)
			&& has_annotation<style_group_tag>(^^tab_skin)
			&& has_annotation<style_group_tag>(^^modal_skin)
			&& has_annotation<style_group_tag>(^^icon_button_skin)
			&& has_annotation<style_group_tag>(^^menu_skin)
			&& has_annotation<style_group_tag>(^^dock_skin)
			&& has_annotation<style_group_tag>(^^screen_skin)
			&& has_annotation<style_group_tag>(^^progress_skin)
			&& has_annotation<style_group_tag>(^^overlay_skin)
			&& has_annotation<style_group_tag>(^^context_menu_skin)
			&& has_annotation<style_group_tag>(^^toggle_skin),
		"style walks recurse only into types annotated style_group; without it they silently skip every widget skin, so style_key stops invalidating caches and scale_style stops scaling"
	);

	template <typename T>
	auto hash_style_members(
		const T& value,
		std::uint64_t& out
	) -> void;

	template <typename T>
	auto scale_style_members(
		T& value,
		float factor
	) -> void;
}

template <typename T>
auto gse::gui::hash_style_members(const T& value, std::uint64_t& out) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_type = std::remove_cvref_t<decltype(value.[:m:])>;
		if constexpr (has_annotation<style_group_tag>(^^member_type)) {
			hash_style_members(value.[:m:], out);
		}
		else if constexpr (std::is_same_v<member_type, float> || std::is_enum_v<member_type>) {
			out = gse::hash_of(out, value.[:m:]);
		}
		else if constexpr (requires { { *std::begin(value.[:m:]) } -> std::convertible_to<float>; }) {
			for (const float c : value.[:m:]) {
				out = gse::hash_of(out, c);
			}
		}
	}
}

template <typename T>
auto gse::gui::scale_style_members(T& value, const float factor) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using member_type = std::remove_cvref_t<decltype(value.[:m:])>;
		if constexpr (has_annotation<style_group_tag>(^^member_type)) {
			scale_style_members(value.[:m:], factor);
		}
		else if constexpr (has_annotation<scaled_tag>(m)) {
			value.[:m:] *= factor;
		}
	}
}

auto gse::gui::skin_for(const style& sty, const button_role role) -> button_skin {
	const button_role_info info = annotation_from_enum(role, button_role_info{});
	return sty.*info.skin;
}

constexpr auto gse::gui::with_widget_skins(const palette& pal, style sty) -> style {
	const vec4f radii = uniform_radii(sty.corner_radius_button);
	const float gap = sty.padding * 0.5f;

	const auto role = [radii, gap](const vec4f idle, const vec4f hot, const vec4f active, const vec4f disabled, const vec4f label, const vec4f label_disabled) -> button_skin {
		return {
			.surfaces = {
				.idle = { .fill = idle, .radii = radii },
				.hot = { .fill = hot, .radii = radii },
				.active = { .fill = active, .radii = radii },
				.disabled = { .fill = disabled, .radii = radii },
				.selected = { .fill = active, .radii = radii },
			},
			.label_colors = {
				.idle = label,
				.hot = label,
				.active = label,
				.disabled = label_disabled,
				.selected = label,
			},
			.glyph_gap = gap,
		};
	};

	sty.button_standard = role(pal.color_button_background, pal.color_button_hovered, pal.color_widget_active, pal.color_widget_background, sty.color_text, sty.color_text_disabled);
	sty.button_accent = role(pal.color_button_accent, pal.color_button_accent_hovered, pal.color_widget_active, pal.color_widget_background, pal.color_button_accent_text, sty.color_text_disabled);
	sty.button_danger = role(pal.color_danger, pal.color_danger_hovered, pal.color_widget_active, pal.color_widget_background, sty.color_text, sty.color_text_disabled);
	sty.button_ghost = role(pal.color_button_ghost, pal.color_widget_hovered, pal.color_widget_active, pal.color_widget_background, sty.color_text_secondary, sty.color_text_disabled);

	const vec4f transparent{ 0.f, 0.f, 0.f, 0.f };
	const vec4f row_radii = uniform_radii(sty.corner_radius);

	sty.list_item = {
		.surfaces = {
			.idle = { .fill = pal.color_widget_background, .radii = row_radii },
			.hot = { .fill = pal.color_widget_hovered, .radii = row_radii },
			.active = { .fill = pal.color_widget_active, .radii = row_radii },
			.disabled = { .fill = pal.color_widget_background, .radii = row_radii },
			.selected = { .fill = pal.color_widget_selected, .radii = row_radii },
		},
		.label_colors = {
			.idle = sty.color_text,
			.hot = sty.color_text,
			.active = sty.color_text,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_text,
		},
		.label = {
			.h = layout::halign::start,
		},
		.detail_color = sty.color_text_secondary,
		.accent_bar = { .fill = sty.color_accent },
		.divider = { .fill = sty.color_text_secondary },
	};

	vec4f nav_hot = pal.color_widget_hovered;
	nav_hot.w() *= 0.55f;

	sty.nav_item = {
		.surfaces = {
			.idle = { .fill = transparent, .radii = row_radii },
			.hot = { .fill = nav_hot, .radii = row_radii },
			.active = { .fill = pal.color_widget_hovered, .radii = row_radii },
			.disabled = { .fill = transparent, .radii = row_radii },
			.selected = { .fill = sty.color_accent_dim, .radii = row_radii },
		},
		.label_colors = {
			.idle = sty.color_text_secondary,
			.hot = sty.color_text_secondary,
			.active = sty.color_text_secondary,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_text,
		},
		.label = {
			.h = layout::halign::start,
		},
		.detail_color = sty.color_text_secondary,
		.accent_bar = {
			.fill = sty.color_accent,
			.radii = uniform_radii(sty.accent_bar_width * 0.5f),
		},
	};

	sty.dropdown = {
		.label = {
			.h = layout::halign::start,
		},
		.label_color = sty.color_text,
		.header = {
			.surfaces = {
				.idle = { .fill = pal.color_widget_background },
				.hot = { .fill = pal.color_widget_hovered },
				.active = { .fill = pal.color_widget_active },
				.disabled = { .fill = pal.color_widget_background },
				.selected = { .fill = pal.color_widget_active },
			},
			.label_colors = {
				.idle = sty.color_text,
				.hot = sty.color_text,
				.active = sty.color_text,
				.disabled = sty.color_text_disabled,
				.selected = sty.color_text,
			},
			.label = {
				.h = layout::halign::start,
			},
		},
		.option = {
			.surfaces = {
				.idle = { .fill = transparent },
				.hot = { .fill = pal.color_widget_hovered },
				.active = { .fill = pal.color_widget_hovered },
				.disabled = { .fill = transparent },
				.selected = { .fill = sty.color_accent_dim },
			},
			.label_colors = {
				.idle = sty.color_text,
				.hot = sty.color_text,
				.active = sty.color_text,
				.disabled = sty.color_text_disabled,
				.selected = sty.color_text,
			},
			.label = {
				.h = layout::halign::start,
			},
		},
		.panel = {
			.fill = pal.color_panel_alt,
			.stroke = sty.color_border,
			.stroke_width = sty.separator_thickness,
		},
		.arrow_color = sty.color_text_secondary,
	};

	const label_spec field_label{
		.h = layout::halign::start,
	};

	vec4f scroll_track = pal.color_widget_background;
	scroll_track.w() *= 0.3f;

	sty.scrollbar = {
		.track = { .fill = scroll_track },
		.thumb = {
			.idle = { .fill = pal.color_widget_background },
			.hot = { .fill = pal.color_widget_hovered },
			.active = { .fill = pal.color_widget_active },
			.disabled = { .fill = pal.color_widget_background },
			.selected = { .fill = pal.color_widget_background },
		},
		.min_thumb = 24.f,
	};

	sty.section = {
		.accent_bar = {
			.fill = sty.color_accent,
			.radii = uniform_radii(sty.accent_bar_width * 0.5f),
		},
		.title = {
			.h = layout::halign::start,
			.size_mult = sty.section_header_size_mult,
		},
		.title_color = sty.color_section_header,
		.subtitle = {
			.h = layout::halign::start,
		},
		.subtitle_color = sty.color_text_secondary,
		.action = {
			.surfaces = {
				.idle = { .fill = pal.color_widget_background, .radii = row_radii },
				.hot = { .fill = pal.color_widget_hovered, .radii = row_radii },
				.active = { .fill = pal.color_widget_hovered, .radii = row_radii },
				.disabled = { .fill = pal.color_widget_background, .radii = row_radii },
				.selected = { .fill = pal.color_widget_background, .radii = row_radii },
			},
			.label_colors = {
				.idle = sty.color_text_secondary,
				.hot = sty.color_text,
				.active = sty.color_text,
				.disabled = sty.color_text_disabled,
				.selected = sty.color_text_secondary,
			},
		},
	};

	sty.separator_line = { .fill = pal.color_separator };
	sty.panel_alt = { .fill = pal.color_panel_alt };

	const state_set<vec4f> icon_palette{
		.idle = sty.color_icon,
		.hot = sty.color_icon_hovered,
		.active = sty.color_icon_hovered,
		.disabled = sty.color_text_disabled,
		.selected = sty.color_icon_hovered,
	};

	sty.icon_button = {
		.surfaces = {
			.idle = { .fill = pal.color_tab_background },
			.hot = { .fill = pal.color_tab_hovered },
			.active = { .fill = pal.color_tab_hovered },
			.disabled = { .fill = pal.color_tab_background },
			.selected = { .fill = pal.color_tab_hovered },
		},
		.icon_colors = icon_palette,
	};

	sty.icon_button_danger = {
		.surfaces = {
			.idle = { .fill = pal.color_tab_background },
			.hot = { .fill = pal.color_danger_hovered },
			.active = { .fill = pal.color_danger_hovered },
			.disabled = { .fill = pal.color_tab_background },
			.selected = { .fill = pal.color_danger_hovered },
		},
		.icon_colors = icon_palette,
	};

	const state_set<vec4f> caption_icon_palette{
		.idle = sty.color_text,
		.hot = sty.color_text,
		.active = sty.color_text,
		.disabled = sty.color_text_disabled,
		.selected = sty.color_text,
	};

	sty.caption_button = {
		.surfaces = {
			.idle = { .fill = pal.color_input_background },
			.hot = { .fill = pal.color_widget_hovered },
			.active = { .fill = pal.color_widget_hovered },
			.disabled = { .fill = pal.color_input_background },
			.selected = { .fill = pal.color_widget_hovered },
		},
		.icon_colors = caption_icon_palette,
	};

	sty.window_caption = {
		.surfaces = {
			.idle = { .fill = transparent },
			.hot = { .fill = pal.color_widget_hovered },
			.active = { .fill = pal.color_widget_hovered },
			.disabled = { .fill = transparent },
			.selected = { .fill = pal.color_widget_hovered },
		},
		.icon_colors = icon_palette,
	};

	sty.window_caption_danger = {
		.surfaces = {
			.idle = { .fill = transparent },
			.hot = { .fill = pal.color_danger_hovered },
			.active = { .fill = pal.color_danger_hovered },
			.disabled = { .fill = transparent },
			.selected = { .fill = pal.color_danger_hovered },
		},
		.icon_colors = icon_palette,
	};

	sty.caption_button_danger = {
		.surfaces = {
			.idle = { .fill = pal.color_input_background },
			.hot = { .fill = pal.color_danger_hovered },
			.active = { .fill = pal.color_danger_hovered },
			.disabled = { .fill = pal.color_input_background },
			.selected = { .fill = pal.color_danger_hovered },
		},
		.icon_colors = caption_icon_palette,
	};

	sty.context_menu = {
		.separator = { .fill = pal.color_separator },
		.row_hover = {
			.fill = pal.color_widget_hovered,
			.radii = row_radii,
		},
		.label_colors = {
			.idle = sty.color_text,
			.hot = sty.color_text,
			.active = sty.color_text,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_text,
		},
		.destructive_color = sty.color_error,
		.row_inset = 3.f,
	};

	sty.graph_node = { .radii = row_radii };

	const vec4f menu_radii = uniform_radii(sty.corner_radius_menu);
	const vec4f opaque_menu_body{ vec3f(pal.color_menu_body), 1.f };

	sty.menu = {
		.floating = {
			.fill = pal.color_menu_body,
			.radii = menu_radii,
		},
		.docked = { .fill = pal.color_menu_body },
		.shadow = { .fill = pal.color_shadow },
		.border = { .fill = sty.color_border },
		.title_bar = { .fill = pal.color_title_bar },
		.accent_bar = { .fill = sty.color_accent },
		.overlay_body = { .fill = { vec3f(pal.color_menu_body), 0.08f } },
		.overlay_title_bar = { .fill = { vec3f(pal.color_title_bar), 0.30f } },
		.overlay_border = { .fill = { vec3f(sty.color_border), 0.45f } },
	};

	sty.dock = {
		.preview = { .fill = pal.color_dock_preview },
		.target = {
			.idle = {
				.fill = pal.color_menu_body,
				.stroke = sty.color_border,
			},
			.hot = {
				.fill = pal.color_menu_body,
				.stroke = sty.color_border,
			},
			.active = {
				.fill = pal.color_widget_hovered,
				.stroke = sty.color_accent,
			},
			.disabled = {
				.fill = pal.color_menu_body,
				.stroke = sty.color_border,
			},
			.selected = {
				.fill = pal.color_widget_hovered,
				.stroke = sty.color_accent,
			},
		},
		.marker_colors = {
			.idle = sty.color_text_secondary,
			.hot = sty.color_text_secondary,
			.active = sty.color_accent,
			.disabled = sty.color_text_secondary,
			.selected = sty.color_accent,
		},
	};

	sty.screen = {
		.backdrop = { .fill = opaque_menu_body },
		.body = { .fill = pal.color_input_background },
		.caption = {
			.h = layout::halign::start,
		},
		.caption_color = sty.color_text_secondary,
		.title = {
			.h = layout::halign::start,
			.size_mult = 1.6f,
		},
		.title_color = sty.color_text,
		.entry_name = {
			.h = layout::halign::start,
			.size_mult = 0.85f,
		},
		.entry_path = {
			.face = text_face::code,
			.h = layout::halign::start,
			.size_mult = 0.85f * 0.9f,
		},
		.note = {
			.h = layout::halign::start,
			.size_mult = 0.85f,
		},
		.preview = {
			.fill = { vec3f(pal.color_input_background), 1.f },
			.stroke = sty.color_border,
			.stroke_width = sty.separator_thickness,
		},
	};

	sty.progress = {
		.track = { .fill = pal.color_widget_background },
		.fill = { .fill = pal.color_widget_active },
		.fill_error = { .fill = sty.color_error },
		.label_color = sty.color_text,
	};

	sty.overlay = {
		.header = { .fill = pal.color_title_bar },
		.row = { .fill = pal.color_widget_background },
		.cell = {
			.face = text_face::code,
			.h = layout::halign::start,
		},
		.cell_color = sty.color_text,
		.drag_ghost = {
			.fill = pal.color_tab_active,
			.radii = row_radii,
		},
		.drag_ghost_detached = {
			.fill = pal.color_menu_body,
			.stroke = sty.color_accent,
			.stroke_width = sty.separator_thickness,
			.radii = row_radii,
		},
		.tooltip = {
			.fill = pal.color_menu_body,
			.stroke = sty.color_border,
			.stroke_width = sty.separator_thickness,
		},
		.text_color = sty.color_text,
		.glyph_color = sty.color_accent,
	};

	sty.modal = {
		.scrim = { .fill = pal.color_scrim },
		.shadow = {
			.fill = pal.color_shadow,
			.radii = menu_radii,
		},
		.panel = {
			.fill = opaque_menu_body,
			.radii = menu_radii,
		},
		.title = {
			.h = layout::halign::start,
		},
		.title_color = sty.color_text,
		.message = {
			.h = layout::halign::start,
		},
		.message_color = sty.color_text_secondary,
	};

	sty.tab = {
		.cell = {
			.idle = { .fill = pal.color_tab_background },
			.hot = { .fill = pal.color_tab_hovered },
			.active = { .fill = pal.color_tab_active },
			.disabled = { .fill = pal.color_tab_background },
			.selected = { .fill = pal.color_tab_active },
		},
		.caption_colors = {
			.idle = sty.color_text_secondary,
			.hot = sty.color_text_secondary,
			.active = sty.color_text,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_text,
		},
		.glyph_colors = {
			.idle = sty.color_text_secondary,
			.hot = sty.color_text,
			.active = sty.color_text,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_text,
		},
		.accent_bar = { .fill = sty.color_accent },
		.gap = 2.f,
		.scroll_bar_extent = 6.f,
		.wheel_step = 80.f,
	};

	sty.input = {
		.label = field_label,
		.label_color = sty.color_text,
		.box = {
			.fill = pal.color_input_background,
			.radii = uniform_radii(sty.corner_radius_input),
		},
		.area = { .fill = pal.color_input_background },
		.gutter = { .fill = pal.color_panel_alt },
		.text_color = sty.color_text,
		.selection = { .fill = pal.color_selection },
		.caret = { .fill = pal.color_caret },
		.text_padding = 5.f,
		.caret_width = 2.f,
	};

	sty.slider = {
		.label = field_label,
		.label_color = sty.color_text,
		.track = {
			.fill = pal.color_widget_background,
			.radii = uniform_radii(sty.corner_radius),
		},
		.fill = {
			.fill = pal.color_slider_fill,
			.radii = uniform_radii(sty.corner_radius),
		},
		.value = {
			.face = text_face::code,
		},
		.value_color = sty.color_text,
	};

	sty.checkbox = {
		.label = field_label,
		.label_colors = {
			.idle = sty.color_text,
			.hot = sty.color_text,
			.active = sty.color_text,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_text,
		},
		.frame = {
			.idle = sty.color_border,
			.hot = pal.color_handle_hovered,
			.active = sty.color_accent,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_border,
		},
		.mark = {
			.idle = sty.color_accent,
			.hot = sty.color_accent,
			.active = sty.color_accent,
			.disabled = sty.color_text_disabled,
			.selected = sty.color_accent,
		},
	};

	const float track_height = sty.font_size * 1.25f;
	const float pill = track_height * 0.5f;
	const float knob_inset = sty.font_size * 0.125f;

	sty.toggle = {
		.label = {
			.h = layout::halign::start,
		},
		.label_color = sty.color_text,
		.track_on = { .fill = pal.color_toggle_on, .radii = uniform_radii(pill) },
		.track_off = { .fill = pal.color_toggle_off, .radii = uniform_radii(pill) },
		.knob = { .fill = pal.color_handle, .radii = uniform_radii(pill) },
		.knob_hovered = pal.color_handle_hovered,
		.track_width = sty.font_size * 2.5f,
		.track_height = track_height,
		.knob_inset = knob_inset,
	};

	return sty;
}

constexpr auto gse::gui::with_outlined_buttons(const palette& pal, style sty) -> style {
	const float edge_width = sty.separator_thickness;

	const vec4f edge_disabled = sty.color_text_disabled;

	const auto outline = [edge_width, edge_disabled](button_skin skin, const vec4f edge) -> button_skin {
		const auto edged = [edge_width](surface s, const vec4f stroke) -> surface {
			s.stroke = stroke;
			s.stroke_width = edge_width;
			return s;
		};

		skin.surfaces = {
			.idle = edged(skin.surfaces.idle, edge),
			.hot = edged(skin.surfaces.hot, edge),
			.active = edged(skin.surfaces.active, edge),
			.disabled = edged(skin.surfaces.disabled, edge_disabled),
			.selected = edged(skin.surfaces.selected, edge),
		};

		skin.label.transform = text_case::upper;
		skin.label.face = text_face::text_strong;
		return skin;
	};

	sty.button_standard = outline(sty.button_standard, sty.color_border);
	sty.button_accent = outline(sty.button_accent, sty.color_accent);
	sty.button_danger = outline(sty.button_danger, pal.color_danger_hovered);
	sty.button_ghost = outline(sty.button_ghost, sty.color_border);

	return sty;
}

constexpr auto gse::gui::style::midnight() -> style {
	constexpr vec4f accent{ 0.26f, 0.86f, 0.84f, 1.0f };
	constexpr vec4f accent_dim{ 0.26f, 0.86f, 0.84f, 0.30f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.10f, 0.14f, 0.20f, 1.0f },
		.color_menu_body = { 0.08f, 0.12f, 0.16f, 0.15f },
		.color_panel_alt = { 0.06f, 0.10f, 0.14f, 0.25f },
		.color_separator = { 0.16f, 0.26f, 0.32f, 1.0f },
		.color_widget_background = { 0.10f, 0.18f, 0.24f, 0.35f },
		.color_widget_hovered = { 0.16f, 0.28f, 0.36f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.14f, 0.32f, 0.36f, 0.35f },
		.color_button_hovered = { 0.20f, 0.46f, 0.50f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.22f, 0.25f, 0.34f, 1.0f },
		.color_handle = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.06f, 0.08f, 0.12f, 0.35f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_tab_background = { 0.07f, 0.12f, 0.17f, 0.35f },
		.color_tab_hovered = { 0.12f, 0.20f, 0.28f, 1.0f },
		.color_tab_active = { 0.16f, 0.26f, 0.34f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
	}, style{
		.color_border = { 0.22f, 0.32f, 0.42f, 1.0f },
		.color_text = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_text_secondary = { 0.66f, 0.70f, 0.80f, 1.0f },
		.color_text_disabled = { 0.40f, 0.43f, 0.50f, 1.0f },
		.color_section_header = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_icon = { 0.70f, 0.74f, 0.84f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 12.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 6.f,
		.corner_radius_menu = 10.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 18.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 3.f,
	});
}

constexpr auto gse::gui::style::eclipse() -> style {
	constexpr vec4f accent{ 0.92f, 0.38f, 0.82f, 1.0f };
	constexpr vec4f accent_dim{ 0.92f, 0.38f, 0.82f, 0.28f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.08f, 0.04f, 0.10f, 1.0f },
		.color_menu_body = { 0.06f, 0.03f, 0.08f, 0.15f },
		.color_panel_alt = { 0.04f, 0.02f, 0.06f, 0.25f },
		.color_separator = { 0.22f, 0.10f, 0.26f, 1.0f },
		.color_widget_background = { 0.14f, 0.07f, 0.18f, 0.35f },
		.color_widget_hovered = { 0.24f, 0.10f, 0.30f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.26f, 0.10f, 0.26f, 0.35f },
		.color_button_hovered = { 0.40f, 0.16f, 0.40f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.14f, 0.12f, 0.16f, 1.0f },
		.color_handle = { 0.95f, 0.93f, 0.96f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.02f, 0.02f, 0.03f, 0.35f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.95f, 0.93f, 0.96f, 1.0f },
		.color_tab_background = { 0.10f, 0.05f, 0.13f, 0.35f },
		.color_tab_hovered = { 0.19f, 0.09f, 0.24f, 1.0f },
		.color_tab_active = { 0.27f, 0.13f, 0.33f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.50f },
	}, style{
		.color_border = { 0.32f, 0.16f, 0.36f, 1.0f },
		.color_text = { 0.95f, 0.93f, 0.96f, 1.0f },
		.color_text_secondary = { 0.62f, 0.58f, 0.66f, 1.0f },
		.color_text_disabled = { 0.36f, 0.34f, 0.40f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 0.70f, 0.66f, 0.74f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 12.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 6.f,
		.corner_radius_menu = 10.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 18.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 3.f,
	});
}

constexpr auto gse::gui::style::ember() -> style {
	constexpr vec4f accent{ 0.98f, 0.62f, 0.20f, 1.0f };
	constexpr vec4f accent_dim{ 0.98f, 0.62f, 0.20f, 0.28f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.20f, 0.14f, 0.10f, 1.0f },
		.color_menu_body = { 0.14f, 0.10f, 0.07f, 0.15f },
		.color_panel_alt = { 0.12f, 0.08f, 0.06f, 0.25f },
		.color_separator = { 0.32f, 0.22f, 0.13f, 1.0f },
		.color_widget_background = { 0.24f, 0.16f, 0.10f, 0.35f },
		.color_widget_hovered = { 0.34f, 0.22f, 0.13f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.36f, 0.22f, 0.10f, 0.35f },
		.color_button_hovered = { 0.52f, 0.32f, 0.14f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.26f, 0.22f, 0.18f, 1.0f },
		.color_handle = { 0.98f, 0.94f, 0.88f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.08f, 0.06f, 0.05f, 0.35f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.98f, 0.94f, 0.88f, 1.0f },
		.color_tab_background = { 0.16f, 0.11f, 0.07f, 0.35f },
		.color_tab_hovered = { 0.28f, 0.19f, 0.11f, 1.0f },
		.color_tab_active = { 0.38f, 0.25f, 0.13f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
	}, style{
		.color_border = { 0.44f, 0.28f, 0.15f, 1.0f },
		.color_text = { 0.98f, 0.94f, 0.88f, 1.0f },
		.color_text_secondary = { 0.74f, 0.68f, 0.60f, 1.0f },
		.color_text_disabled = { 0.46f, 0.42f, 0.38f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 0.78f, 0.72f, 0.64f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 12.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 6.f,
		.corner_radius_menu = 10.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 18.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 3.f,
	});
}

constexpr auto gse::gui::style::forest() -> style {
	constexpr vec4f accent{ 0.42f, 0.92f, 0.62f, 1.0f };
	constexpr vec4f accent_dim{ 0.42f, 0.92f, 0.62f, 0.28f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.08f, 0.16f, 0.12f, 1.0f },
		.color_menu_body = { 0.06f, 0.12f, 0.09f, 0.15f },
		.color_panel_alt = { 0.05f, 0.10f, 0.07f, 0.25f },
		.color_separator = { 0.14f, 0.28f, 0.20f, 1.0f },
		.color_widget_background = { 0.10f, 0.24f, 0.16f, 0.35f },
		.color_widget_hovered = { 0.14f, 0.34f, 0.22f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.14f, 0.34f, 0.22f, 0.35f },
		.color_button_hovered = { 0.22f, 0.50f, 0.32f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.16f, 0.22f, 0.18f, 1.0f },
		.color_handle = { 0.94f, 0.97f, 0.94f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.05f, 0.08f, 0.06f, 0.35f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.94f, 0.97f, 0.94f, 1.0f },
		.color_tab_background = { 0.06f, 0.15f, 0.10f, 0.35f },
		.color_tab_hovered = { 0.12f, 0.28f, 0.19f, 1.0f },
		.color_tab_active = { 0.17f, 0.38f, 0.25f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
	}, style{
		.color_border = { 0.18f, 0.38f, 0.26f, 1.0f },
		.color_text = { 0.94f, 0.97f, 0.94f, 1.0f },
		.color_text_secondary = { 0.66f, 0.74f, 0.68f, 1.0f },
		.color_text_disabled = { 0.40f, 0.46f, 0.42f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 0.70f, 0.78f, 0.72f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 12.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 6.f,
		.corner_radius_menu = 10.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 18.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 3.f,
	});
}

constexpr auto gse::gui::style::frost() -> style {
	constexpr vec4f accent{ 0.20f, 0.50f, 0.95f, 1.0f };
	constexpr vec4f accent_dim{ 0.20f, 0.50f, 0.95f, 0.20f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.90f, 0.94f, 0.99f, 1.0f },
		.color_menu_body = { 0.94f, 0.96f, 0.99f, 0.15f },
		.color_panel_alt = { 0.90f, 0.93f, 0.97f, 0.25f },
		.color_separator = { 0.78f, 0.86f, 0.96f, 1.0f },
		.color_widget_background = { 0.84f, 0.91f, 0.99f, 0.35f },
		.color_widget_hovered = { 0.72f, 0.84f, 0.96f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.74f, 0.86f, 0.99f, 0.35f },
		.color_button_hovered = { 0.56f, 0.76f, 0.97f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.78f, 0.82f, 0.88f, 1.0f },
		.color_handle = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_handle_hovered = { 0.97f, 0.98f, 1.0f, 1.0f },
		.color_input_background = { 1.0f, 1.0f, 1.0f, 0.35f },
		.color_selection = { vec3f(accent), 0.30f },
		.color_caret = { 0.10f, 0.12f, 0.18f, 1.0f },
		.color_tab_background = { 0.80f, 0.86f, 0.94f, 0.35f },
		.color_tab_hovered = { 0.88f, 0.93f, 0.99f, 1.0f },
		.color_tab_active = { 0.98f, 0.99f, 1.0f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.30f },
		.color_shadow = { 0.10f, 0.12f, 0.18f, 0.18f },
	}, style{
		.color_border = { 0.62f, 0.74f, 0.92f, 1.0f },
		.color_text = { 0.10f, 0.12f, 0.18f, 1.0f },
		.color_text_secondary = { 0.40f, 0.45f, 0.55f, 1.0f },
		.color_text_disabled = { 0.62f, 0.66f, 0.72f, 1.0f },
		.color_section_header = { 0.08f, 0.12f, 0.20f, 1.0f },
		.color_icon = { 0.35f, 0.40f, 0.50f, 1.0f },
		.color_icon_hovered = { 0.05f, 0.08f, 0.15f, 1.0f },
		.color_folder = { 0.72f, 0.55f, 0.20f, 1.0f },
		.color_file = { 0.30f, 0.44f, 0.66f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 12.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 6.f,
		.corner_radius_menu = 10.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 18.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 3.f,
	});
}

constexpr auto gse::gui::style::blueprint() -> style {
	constexpr vec4f accent{ 0.11f, 0.34f, 0.72f, 1.0f };
	constexpr vec4f accent_dim{ 0.11f, 0.34f, 0.72f, 0.18f };
	constexpr vec4f ink{ 0.13f, 0.13f, 0.14f, 1.0f };
	constexpr vec4f paper{ 0.88f, 0.88f, 0.86f, 1.0f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.84f, 0.84f, 0.82f, 1.0f },
		.color_menu_body = { 0.88f, 0.88f, 0.86f, 1.0f },
		.color_panel_alt = { 0.85f, 0.85f, 0.83f, 1.0f },
		.color_separator = { 0.62f, 0.62f, 0.60f, 1.0f },
		.color_widget_background = { 0.82f, 0.82f, 0.80f, 1.0f },
		.color_widget_hovered = { 0.76f, 0.76f, 0.74f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.80f, 0.81f, 0.84f, 1.0f },
		.color_button_hovered = { 0.72f, 0.75f, 0.82f, 1.0f },
		.color_button_accent = accent,
		.color_button_accent_hovered = { 0.16f, 0.42f, 0.82f, 1.0f },
		.color_button_accent_text = paper,
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.74f, 0.74f, 0.72f, 1.0f },
		.color_handle = { 0.98f, 0.98f, 0.97f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.95f, 0.95f, 0.93f, 1.0f },
		.color_selection = { vec3f(accent), 0.25f },
		.color_caret = ink,
		.color_danger = { 0.88f, 0.62f, 0.58f, 1.0f },
		.color_danger_hovered = { 0.84f, 0.50f, 0.46f, 1.0f },
		.color_tab_background = { 0.85f, 0.85f, 0.83f, 1.0f },
		.color_tab_hovered = { 0.80f, 0.80f, 0.78f, 1.0f },
		.color_tab_active = { 0.92f, 0.92f, 0.90f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.25f },
		.color_shadow = { 0.13f, 0.13f, 0.14f, 0.12f },
		.color_scrim = { 0.13f, 0.13f, 0.14f, 0.40f },
	}, style{
		.color_border = { 0.62f, 0.62f, 0.60f, 1.0f },
		.color_text = ink,
		.color_text_secondary = { 0.40f, 0.40f, 0.42f, 1.0f },
		.color_text_disabled = { 0.64f, 0.64f, 0.63f, 1.0f },
		.color_section_header = ink,
		.color_icon = { 0.34f, 0.34f, 0.36f, 1.0f },
		.color_icon_hovered = ink,
		.color_folder = { 0.62f, 0.48f, 0.14f, 1.0f },
		.color_file = { 0.28f, 0.38f, 0.56f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.color_warning = { 0.62f, 0.44f, 0.05f, 1.0f },
		.color_error = { 0.70f, 0.16f, 0.12f, 1.0f },
		.color_fatal = { 0.75f, 0.10f, 0.10f, 1.0f },
		.color_added = { 0.16f, 0.46f, 0.22f, 1.0f },
		.color_removed = { 0.70f, 0.20f, 0.20f, 1.0f },
		.padding = 14.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 0.f,
		.corner_radius_menu = 0.f,
		.corner_radius_button = 0.f,
		.corner_radius_input = 0.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 20.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 2.f,
	});
}

constexpr auto gse::gui::style::high_contrast() -> style {
	constexpr vec4f accent{ 1.0f, 0.92f, 0.0f, 1.0f };
	constexpr vec4f accent_dim{ 1.0f, 0.92f, 0.0f, 0.32f };
	constexpr palette pal{
		.color_title_bar = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_menu_body = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_panel_alt = { 0.05f, 0.05f, 0.05f, 1.0f },
		.color_separator = { 0.6f, 0.6f, 0.6f, 1.0f },
		.color_widget_background = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_widget_hovered = { 0.25f, 0.25f, 0.0f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_button_hovered = { 0.35f, 0.32f, 0.0f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = { 0.0f, 1.0f, 0.0f, 1.0f },
		.color_toggle_off = { 0.3f, 0.3f, 0.3f, 1.0f },
		.color_handle = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_handle_hovered = accent,
		.color_input_background = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_selection = { 0.0f, 0.5f, 1.0f, 0.6f },
		.color_caret = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_tab_background = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_tab_hovered = { 0.20f, 0.18f, 0.0f, 1.0f },
		.color_tab_active = { 0.38f, 0.35f, 0.0f, 1.0f },
		.color_dock_preview = { 1.0f, 0.92f, 0.0f, 0.5f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.5f },
	};

	return with_outlined_buttons(pal, with_widget_skins(pal, style{
		.color_border = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_text = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_text_secondary = { 0.82f, 0.82f, 0.82f, 1.0f },
		.color_text_disabled = { 0.5f, 0.5f, 0.5f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_icon_hovered = accent,
		.color_folder = accent,
		.color_file = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 14.f,
		.title_bar_height = 36.f,
		.resize_border_thickness = 10.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 18.f,
		.font = {},
		.corner_radius = 0.f,
		.corner_radius_menu = 0.f,
		.widget_height_padding = 0.8f,
		.item_spacing = 6.f,
		.section_spacing_above = 22.f,
		.section_spacing_below = 12.f,
		.section_header_size_mult = 1.40f,
		.accent_bar_width = 4.f,
	}));
}

constexpr auto gse::gui::style::neutral() -> style {
	constexpr vec4f accent{ 0.66f, 0.70f, 0.74f, 1.0f };
	constexpr vec4f accent_dim{ 0.66f, 0.70f, 0.74f, 0.30f };
	return with_widget_skins(palette{
		.color_title_bar = { 0.13f, 0.13f, 0.14f, 1.0f },
		.color_menu_body = { 0.11f, 0.11f, 0.12f, 0.15f },
		.color_panel_alt = { 0.09f, 0.09f, 0.10f, 0.25f },
		.color_separator = { 0.22f, 0.23f, 0.25f, 1.0f },
		.color_widget_background = { 0.17f, 0.17f, 0.19f, 0.35f },
		.color_widget_hovered = { 0.26f, 0.27f, 0.29f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.24f, 0.25f, 0.27f, 0.35f },
		.color_button_hovered = { 0.36f, 0.37f, 0.40f, 1.0f },
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.26f, 0.27f, 0.29f, 1.0f },
		.color_handle = { 0.94f, 0.94f, 0.95f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.08f, 0.08f, 0.09f, 0.35f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.94f, 0.94f, 0.95f, 1.0f },
		.color_tab_background = { 0.10f, 0.10f, 0.11f, 0.35f },
		.color_tab_hovered = { 0.18f, 0.18f, 0.20f, 1.0f },
		.color_tab_active = { 0.24f, 0.25f, 0.27f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
	}, style{
		.color_border = { 0.30f, 0.31f, 0.33f, 1.0f },
		.color_text = { 0.94f, 0.94f, 0.95f, 1.0f },
		.color_text_secondary = { 0.66f, 0.67f, 0.70f, 1.0f },
		.color_text_disabled = { 0.42f, 0.43f, 0.45f, 1.0f },
		.color_section_header = { 0.94f, 0.94f, 0.95f, 1.0f },
		.color_icon = { 0.72f, 0.73f, 0.76f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.padding = 12.f,
		.title_bar_height = 32.f,
		.resize_border_thickness = 8.f,
		.min_menu_size = { 200.f, 120.f },
		.font_size = 16.f,
		.font = {},
		.corner_radius = 6.f,
		.corner_radius_menu = 10.f,
		.widget_height_padding = 0.7f,
		.item_spacing = 4.f,
		.section_spacing_above = 18.f,
		.section_spacing_below = 10.f,
		.section_header_size_mult = 1.30f,
		.accent_bar_width = 3.f,
	});
}

constexpr auto gse::gui::style::from_theme(const theme t) -> style {
	switch (t) {
		case theme::midnight:
			return midnight();
		case theme::eclipse:
			return eclipse();
		case theme::ember:
			return ember();
		case theme::forest:
			return forest();
		case theme::frost:
			return frost();
		case theme::blueprint:
			return blueprint();
		case theme::high_contrast:
			return high_contrast();
		case theme::neutral:
			return neutral();
		default:
			return midnight();
	}
}

auto gse::gui::style_key(const style& sty) -> std::uint64_t {
	std::uint64_t h = 0;
	hash_style_members(sty, h);
	return h;
}

constexpr auto gse::gui::accent_bar_extent(const style& sty) -> float {
	return std::max(2.f, sty.accent_bar_width);
}

constexpr auto gse::gui::dp::px(const style& sty) const -> float {
	return value * sty.scale_factor;
}

constexpr auto gse::gui::dp_vec::px(const style& sty) const -> vec2f {
	return value * sty.scale_factor;
}