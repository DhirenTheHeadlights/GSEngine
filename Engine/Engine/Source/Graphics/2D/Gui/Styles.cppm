export module gse.graphics:styles;

import std;

import gse.math;

export namespace gse {
	struct scaled_tag {};
	constexpr scaled_tag scaled{};
}

export namespace gse::gui {
	enum class theme {
		midnight,
		eclipse,
		ember,
		forest,
		frost,
		high_contrast,
	};

	struct style;

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

	struct style {
		// Menu chrome
		vec4f color_title_bar = { 0.10f, 0.14f, 0.20f, 0.55f };
		vec4f color_title_bar_inactive = { 0.07f, 0.10f, 0.14f, 0.55f };
		vec4f color_menu_body = { 0.08f, 0.12f, 0.16f, 0.15f };
		vec4f color_panel_alt = { 0.06f, 0.10f, 0.14f, 0.25f };
		vec4f color_border = { 0.22f, 0.32f, 0.42f, 1.0f };
		vec4f color_separator = { 0.18f, 0.26f, 0.32f, 1.0f };

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
		[[= gse::scaled]] float icon_extent = 14.f;

		// Interactive widget states
		vec4f color_widget_background = { 0.10f, 0.16f, 0.22f, 1.0f };
		vec4f color_widget_hovered = { 0.14f, 0.24f, 0.32f, 1.0f };
		vec4f color_widget_active = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_widget_selected = { 0.26f, 0.86f, 0.84f, 0.32f };

		// Buttons (distinct from passive widget surfaces so CTAs read as clickable)
		vec4f color_button_background = { 0.14f, 0.30f, 0.36f, 1.0f };
		vec4f color_button_hovered = { 0.20f, 0.44f, 0.50f, 1.0f };

		// Accent (primary theme color: drives sliders, toggle-on, selections, section bars)
		vec4f color_accent = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_accent_dim = { 0.26f, 0.86f, 0.84f, 0.30f };

		// Sliders / toggles
		vec4f color_slider_fill = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_toggle_on = { 0.26f, 0.86f, 0.84f, 1.0f };
		vec4f color_toggle_off = { 0.22f, 0.25f, 0.34f, 1.0f };
		vec4f color_handle = { 0.96f, 0.97f, 0.99f, 1.0f };
		vec4f color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Inputs
		vec4f color_input_background = { 0.06f, 0.08f, 0.12f, 1.0f };
		vec4f color_selection = { 0.26f, 0.86f, 0.84f, 0.40f };
		vec4f color_caret = { 0.96f, 0.97f, 0.99f, 1.0f };

		// Tabs (own surface so strips read as chrome rather than widgets)
		vec4f color_tab_background = { 0.07f, 0.12f, 0.17f, 1.0f };
		vec4f color_tab_hovered = { 0.12f, 0.20f, 0.28f, 1.0f };
		vec4f color_tab_active = { 0.16f, 0.26f, 0.34f, 1.0f };

		// Docking
		vec4f color_dock_preview = { 0.26f, 0.86f, 0.84f, 0.35f };
		vec4f color_dock_tab_active = { 0.26f, 0.86f, 0.84f, 0.6f };

		vec4f color_shadow = { 0.f, 0.f, 0.f, 0.35f };

		// Runtime-resolved scale factor (set by apply_scale, not a styled dimension)
		float scale_factor = 1.f;

		// Widget sizing (auto-scaled via [[= gse::scaled]] reflection in apply_scale)
		[[= gse::scaled]] float padding = 12.f;
		[[= gse::scaled]] float title_bar_height = 32.f;
		[[= gse::scaled]] float resize_border_thickness = 8.f;
		[[= gse::scaled]] vec2f min_menu_size = { 200.f, 120.f };
		[[= gse::scaled]] float font_size = 16.f;
		std::filesystem::path font;

		[[= gse::scaled]] float corner_radius = 6.f;
		[[= gse::scaled]] float corner_radius_menu = 10.f;
		float widget_height_padding = 0.7f;
		[[= gse::scaled]] float item_spacing = 4.f;
		[[= gse::scaled]] float section_spacing_above = 18.f;
		[[= gse::scaled]] float section_spacing_below = 10.f;
		float section_header_size_mult = 1.30f;
		[[= gse::scaled]] float accent_bar_width = 3.f;

		// Screen/card layout (referenced by SettingsScreen, MainMenuScreen, etc.)
		[[= gse::scaled]] vec2f card_min_size = { 720.f, 480.f };
		[[= gse::scaled]] vec2f card_max_size = { 1100.f, 760.f };
		[[= gse::scaled]] vec2f card_margin = { 80.f, 60.f };
		[[= gse::scaled]] float sidebar_width = 220.f;
		[[= gse::scaled]] float header_height = 56.f;
		[[= gse::scaled]] float footer_height = 64.f;
		[[= gse::scaled]] float close_button_size = 28.f;
		[[= gse::scaled]] float separator_thickness = 1.f;

		// Buttons
		[[= gse::scaled]] float button_height = 32.f;
		[[= gse::scaled]] float button_min_width = 108.f;
		[[= gse::scaled]] float accent_button_min_width = 132.f;
		[[= gse::scaled]] float button_spacing = 8.f;

		// Standalone panels
		[[= gse::scaled]] float side_panel_max_width = 320.f;
		[[= gse::scaled]] float progress_bar_max_width = 400.f;
		[[= gse::scaled]] float progress_bar_height = 12.f;
		[[= gse::scaled]] float preview_height = 160.f;

		static constexpr auto midnight() -> style;
		static constexpr auto eclipse() -> style;
		static constexpr auto ember() -> style;
		static constexpr auto forest() -> style;
		static constexpr auto frost() -> style;
		static constexpr auto high_contrast() -> style;

		static constexpr auto from_theme(
			theme t
		) -> style;
	};
}

constexpr auto gse::gui::style::midnight() -> style {
	constexpr vec4f accent{ 0.26f, 0.86f, 0.84f, 1.0f };
	constexpr vec4f accent_dim{ 0.26f, 0.86f, 0.84f, 0.30f };
	return style{
		.color_title_bar = { 0.10f, 0.14f, 0.20f, 0.55f },
		.color_title_bar_inactive = { 0.07f, 0.10f, 0.14f, 0.55f },
		.color_menu_body = { 0.08f, 0.12f, 0.16f, 0.15f },
		.color_panel_alt = { 0.06f, 0.10f, 0.14f, 0.25f },
		.color_border = { 0.22f, 0.32f, 0.42f, 1.0f },
		.color_separator = { 0.16f, 0.26f, 0.32f, 1.0f },
		.color_text = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_text_secondary = { 0.66f, 0.70f, 0.80f, 1.0f },
		.color_text_disabled = { 0.40f, 0.43f, 0.50f, 1.0f },
		.color_section_header = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_icon = { 0.70f, 0.74f, 0.84f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_widget_background = { 0.10f, 0.18f, 0.24f, 1.0f },
		.color_widget_hovered = { 0.16f, 0.28f, 0.36f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.14f, 0.32f, 0.36f, 1.0f },
		.color_button_hovered = { 0.20f, 0.46f, 0.50f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.22f, 0.25f, 0.34f, 1.0f },
		.color_handle = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.06f, 0.08f, 0.12f, 1.0f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.96f, 0.97f, 0.99f, 1.0f },
		.color_tab_background = { 0.07f, 0.12f, 0.17f, 1.0f },
		.color_tab_hovered = { 0.12f, 0.20f, 0.28f, 1.0f },
		.color_tab_active = { 0.16f, 0.26f, 0.34f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_dock_tab_active = { vec3f(accent), 0.6f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
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
	};
}

constexpr auto gse::gui::style::eclipse() -> style {
	constexpr vec4f accent{ 0.92f, 0.38f, 0.82f, 1.0f };
	constexpr vec4f accent_dim{ 0.92f, 0.38f, 0.82f, 0.28f };
	return style{
		.color_title_bar = { 0.08f, 0.04f, 0.10f, 0.55f },
		.color_title_bar_inactive = { 0.04f, 0.02f, 0.05f, 0.55f },
		.color_menu_body = { 0.06f, 0.03f, 0.08f, 0.15f },
		.color_panel_alt = { 0.04f, 0.02f, 0.06f, 0.25f },
		.color_border = { 0.32f, 0.16f, 0.36f, 1.0f },
		.color_separator = { 0.22f, 0.10f, 0.26f, 1.0f },
		.color_text = { 0.95f, 0.93f, 0.96f, 1.0f },
		.color_text_secondary = { 0.62f, 0.58f, 0.66f, 1.0f },
		.color_text_disabled = { 0.36f, 0.34f, 0.40f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 0.70f, 0.66f, 0.74f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_widget_background = { 0.14f, 0.07f, 0.18f, 1.0f },
		.color_widget_hovered = { 0.24f, 0.10f, 0.30f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.26f, 0.10f, 0.26f, 1.0f },
		.color_button_hovered = { 0.40f, 0.16f, 0.40f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.14f, 0.12f, 0.16f, 1.0f },
		.color_handle = { 0.95f, 0.93f, 0.96f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.02f, 0.02f, 0.03f, 1.0f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.95f, 0.93f, 0.96f, 1.0f },
		.color_tab_background = { 0.10f, 0.05f, 0.13f, 1.0f },
		.color_tab_hovered = { 0.19f, 0.09f, 0.24f, 1.0f },
		.color_tab_active = { 0.27f, 0.13f, 0.33f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_dock_tab_active = { vec3f(accent), 0.6f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.50f },
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
	};
}

constexpr auto gse::gui::style::ember() -> style {
	constexpr vec4f accent{ 0.98f, 0.62f, 0.20f, 1.0f };
	constexpr vec4f accent_dim{ 0.98f, 0.62f, 0.20f, 0.28f };
	return style{
		.color_title_bar = { 0.20f, 0.14f, 0.10f, 0.55f },
		.color_title_bar_inactive = { 0.14f, 0.10f, 0.07f, 0.55f },
		.color_menu_body = { 0.14f, 0.10f, 0.07f, 0.15f },
		.color_panel_alt = { 0.12f, 0.08f, 0.06f, 0.25f },
		.color_border = { 0.44f, 0.28f, 0.15f, 1.0f },
		.color_separator = { 0.32f, 0.22f, 0.13f, 1.0f },
		.color_text = { 0.98f, 0.94f, 0.88f, 1.0f },
		.color_text_secondary = { 0.74f, 0.68f, 0.60f, 1.0f },
		.color_text_disabled = { 0.46f, 0.42f, 0.38f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 0.78f, 0.72f, 0.64f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_widget_background = { 0.24f, 0.16f, 0.10f, 1.0f },
		.color_widget_hovered = { 0.34f, 0.22f, 0.13f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.36f, 0.22f, 0.10f, 1.0f },
		.color_button_hovered = { 0.52f, 0.32f, 0.14f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.26f, 0.22f, 0.18f, 1.0f },
		.color_handle = { 0.98f, 0.94f, 0.88f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.08f, 0.06f, 0.05f, 1.0f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.98f, 0.94f, 0.88f, 1.0f },
		.color_tab_background = { 0.16f, 0.11f, 0.07f, 1.0f },
		.color_tab_hovered = { 0.28f, 0.19f, 0.11f, 1.0f },
		.color_tab_active = { 0.38f, 0.25f, 0.13f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_dock_tab_active = { vec3f(accent), 0.6f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
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
	};
}

constexpr auto gse::gui::style::forest() -> style {
	constexpr vec4f accent{ 0.42f, 0.92f, 0.62f, 1.0f };
	constexpr vec4f accent_dim{ 0.42f, 0.92f, 0.62f, 0.28f };
	return style{
		.color_title_bar = { 0.08f, 0.16f, 0.12f, 0.55f },
		.color_title_bar_inactive = { 0.05f, 0.11f, 0.08f, 0.55f },
		.color_menu_body = { 0.06f, 0.12f, 0.09f, 0.15f },
		.color_panel_alt = { 0.05f, 0.10f, 0.07f, 0.25f },
		.color_border = { 0.18f, 0.38f, 0.26f, 1.0f },
		.color_separator = { 0.14f, 0.28f, 0.20f, 1.0f },
		.color_text = { 0.94f, 0.97f, 0.94f, 1.0f },
		.color_text_secondary = { 0.66f, 0.74f, 0.68f, 1.0f },
		.color_text_disabled = { 0.40f, 0.46f, 0.42f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 0.70f, 0.78f, 0.72f, 1.0f },
		.color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_widget_background = { 0.10f, 0.24f, 0.16f, 1.0f },
		.color_widget_hovered = { 0.14f, 0.34f, 0.22f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.14f, 0.34f, 0.22f, 1.0f },
		.color_button_hovered = { 0.22f, 0.50f, 0.32f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.16f, 0.22f, 0.18f, 1.0f },
		.color_handle = { 0.94f, 0.97f, 0.94f, 1.0f },
		.color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_input_background = { 0.05f, 0.08f, 0.06f, 1.0f },
		.color_selection = { vec3f(accent), 0.40f },
		.color_caret = { 0.94f, 0.97f, 0.94f, 1.0f },
		.color_tab_background = { 0.06f, 0.15f, 0.10f, 1.0f },
		.color_tab_hovered = { 0.12f, 0.28f, 0.19f, 1.0f },
		.color_tab_active = { 0.17f, 0.38f, 0.25f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.35f },
		.color_dock_tab_active = { vec3f(accent), 0.6f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.40f },
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
	};
}

constexpr auto gse::gui::style::frost() -> style {
	constexpr vec4f accent{ 0.20f, 0.50f, 0.95f, 1.0f };
	constexpr vec4f accent_dim{ 0.20f, 0.50f, 0.95f, 0.20f };
	return style{
		.color_title_bar = { 0.90f, 0.94f, 0.99f, 0.55f },
		.color_title_bar_inactive = { 0.84f, 0.88f, 0.94f, 0.55f },
		.color_menu_body = { 0.94f, 0.96f, 0.99f, 0.15f },
		.color_panel_alt = { 0.90f, 0.93f, 0.97f, 0.25f },
		.color_border = { 0.62f, 0.74f, 0.92f, 1.0f },
		.color_separator = { 0.78f, 0.86f, 0.96f, 1.0f },
		.color_text = { 0.10f, 0.12f, 0.18f, 1.0f },
		.color_text_secondary = { 0.40f, 0.45f, 0.55f, 1.0f },
		.color_text_disabled = { 0.62f, 0.66f, 0.72f, 1.0f },
		.color_section_header = { 0.08f, 0.12f, 0.20f, 1.0f },
		.color_icon = { 0.35f, 0.40f, 0.50f, 1.0f },
		.color_icon_hovered = { 0.05f, 0.08f, 0.15f, 1.0f },
		.color_folder = { 0.72f, 0.55f, 0.20f, 1.0f },
		.color_file = { 0.30f, 0.44f, 0.66f, 1.0f },
		.color_widget_background = { 0.84f, 0.91f, 0.99f, 1.0f },
		.color_widget_hovered = { 0.72f, 0.84f, 0.96f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.74f, 0.86f, 0.99f, 1.0f },
		.color_button_hovered = { 0.56f, 0.76f, 0.97f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
		.color_slider_fill = accent,
		.color_toggle_on = accent,
		.color_toggle_off = { 0.78f, 0.82f, 0.88f, 1.0f },
		.color_handle = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_handle_hovered = { 0.97f, 0.98f, 1.0f, 1.0f },
		.color_input_background = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_selection = { vec3f(accent), 0.30f },
		.color_caret = { 0.10f, 0.12f, 0.18f, 1.0f },
		.color_tab_background = { 0.80f, 0.86f, 0.94f, 1.0f },
		.color_tab_hovered = { 0.88f, 0.93f, 0.99f, 1.0f },
		.color_tab_active = { 0.98f, 0.99f, 1.0f, 1.0f },
		.color_dock_preview = { vec3f(accent), 0.30f },
		.color_dock_tab_active = { vec3f(accent), 0.55f },
		.color_shadow = { 0.10f, 0.12f, 0.18f, 0.18f },
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
	};
}

constexpr auto gse::gui::style::high_contrast() -> style {
	constexpr vec4f accent{ 1.0f, 0.92f, 0.0f, 1.0f };
	constexpr vec4f accent_dim{ 1.0f, 0.92f, 0.0f, 0.32f };
	return style{
		.color_title_bar = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_title_bar_inactive = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_menu_body = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_panel_alt = { 0.05f, 0.05f, 0.05f, 1.0f },
		.color_border = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_separator = { 0.6f, 0.6f, 0.6f, 1.0f },
		.color_text = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_text_secondary = { 0.82f, 0.82f, 0.82f, 1.0f },
		.color_text_disabled = { 0.5f, 0.5f, 0.5f, 1.0f },
		.color_section_header = accent,
		.color_icon = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_icon_hovered = accent,
		.color_folder = accent,
		.color_file = { 1.0f, 1.0f, 1.0f, 1.0f },
		.color_widget_background = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_widget_hovered = { 0.25f, 0.25f, 0.0f, 1.0f },
		.color_widget_active = accent,
		.color_widget_selected = accent_dim,
		.color_button_background = { 0.0f, 0.0f, 0.0f, 1.0f },
		.color_button_hovered = { 0.35f, 0.32f, 0.0f, 1.0f },
		.color_accent = accent,
		.color_accent_dim = accent_dim,
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
		.color_dock_tab_active = { 1.0f, 0.92f, 0.0f, 0.8f },
		.color_shadow = { 0.f, 0.f, 0.f, 0.5f },
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
	};
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
		case theme::high_contrast:
			return high_contrast();
		default:
			return midnight();
	}
}

constexpr auto gse::gui::dp::px(const style& sty) const -> float {
	return value * sty.scale_factor;
}

constexpr auto gse::gui::dp_vec::px(const style& sty) const -> vec2f {
	return value * sty.scale_factor;
}
