export module gse.graphics:styles;

import std;
	
import gse.math;

export namespace gse::gui {
    enum class theme {
        dark,
        darker,
        light,
        high_contrast
    };

	struct style {
		// Window chrome
		unitless::vec4 color_title_bar = { 0.18f, 0.18f, 0.20f, 0.95f };
		unitless::vec4 color_title_bar_inactive = { 0.12f, 0.12f, 0.14f, 0.95f };
		unitless::vec4 color_menu_body = { 0.10f, 0.10f, 0.12f, 0.98f };
		unitless::vec4 color_menu_bar = { 0.08f, 0.08f, 0.10f, 1.0f };
		unitless::vec4 color_border = { 0.06f, 0.06f, 0.08f, 1.0f };

		// Text
		unitless::vec4 color_text = { 0.92f, 0.92f, 0.94f, 1.0f };
		unitless::vec4 color_text_secondary = { 0.6f, 0.6f, 0.62f, 1.0f };
		unitless::vec4 color_text_disabled = { 0.5f, 0.5f, 0.52f, 1.0f };

		// Icons
		unitless::vec4 color_icon = { 0.7f, 0.7f, 0.72f, 1.0f };
		unitless::vec4 color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Interactive widget states (buttons, selectables, tree rows)
		unitless::vec4 color_widget_background = { 0.22f, 0.22f, 0.24f, 1.0f };
		unitless::vec4 color_widget_hovered = { 0.30f, 0.30f, 0.34f, 1.0f };
		unitless::vec4 color_widget_active = { 0.24f, 0.52f, 0.88f, 1.0f };
		unitless::vec4 color_widget_selected = { 0.24f, 0.52f, 0.88f, 0.85f };

		// Sliders
		unitless::vec4 color_slider_fill = { 0.24f, 0.52f, 0.88f, 0.9f };

		// Toggle switches
		unitless::vec4 color_toggle_on = { 0.2f, 0.65f, 0.35f, 1.0f };
		unitless::vec4 color_toggle_off = { 0.28f, 0.28f, 0.30f, 1.0f };

		// Knobs/handles (for toggles, sliders)
		unitless::vec4 color_handle = { 0.92f, 0.92f, 0.94f, 1.0f };
		unitless::vec4 color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Input fields
		unitless::vec4 color_input_background = { 0.08f, 0.08f, 0.10f, 1.0f };
		unitless::vec4 color_selection = { 0.24f, 0.52f, 0.88f, 0.45f };
		unitless::vec4 color_caret = { 0.92f, 0.92f, 0.94f, 1.0f };

		// Docking system
		unitless::vec4 color_dock_preview = { 0.24f, 0.52f, 0.88f, 0.35f };
		unitless::vec4 color_dock_tab_active = { 0.24f, 0.52f, 0.88f, 0.6f };

		// Sizing
		float padding = 10.f;
		float title_bar_height = 30.f;
		float resize_border_thickness = 8.f;
		unitless::vec2 min_menu_size = { 150.f, 100.f };
		float font_size = 16.f;
		float menu_bar_height = 32.f;
		std::filesystem::path font;

		static constexpr auto dark() -> style;
		static constexpr auto darker() -> style;
		static constexpr auto light() -> style;
		static constexpr auto high_contrast() -> style;

		static constexpr auto from_theme(theme t) -> style;
	};
}

constexpr auto gse::gui::style::dark() -> style {
    return style{
        // Window chrome
        .color_title_bar = { 0.18f, 0.18f, 0.20f, 0.95f },
        .color_title_bar_inactive = { 0.12f, 0.12f, 0.14f, 0.95f },
        .color_menu_body = { 0.10f, 0.10f, 0.12f, 0.01f },
        .color_menu_bar = { 0.08f, 0.08f, 0.10f, 1.0f },
        .color_border = { 0.06f, 0.06f, 0.08f, 1.0f },

        // Text
        .color_text = { 0.92f, 0.92f, 0.94f, 1.0f },
        .color_text_secondary = { 0.6f, 0.6f, 0.62f, 1.0f },
        .color_text_disabled = { 0.4f, 0.4f, 0.42f, 1.0f },

        // Icons
        .color_icon = { 0.7f, 0.7f, 0.72f, 1.0f },
        .color_icon_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },

        // Interactive widgets
        .color_widget_background = { 0.22f, 0.22f, 0.24f, 0.9f },
        .color_widget_hovered = { 0.30f, 0.30f, 0.34f, 0.95f },
        .color_widget_active = { 0.24f, 0.52f, 0.88f, 1.0f },
        .color_widget_selected = { 0.24f, 0.52f, 0.88f, 0.85f },

        // Sliders
        .color_slider_fill = { 0.24f, 0.52f, 0.88f, 0.9f },

        // Toggles
        .color_toggle_on = { 0.2f, 0.65f, 0.35f, 1.0f },
        .color_toggle_off = { 0.28f, 0.28f, 0.30f, 1.0f },

        // Handles
        .color_handle = { 0.92f, 0.92f, 0.94f, 1.0f },
        .color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },

        // Input fields
        .color_input_background = { 0.08f, 0.08f, 0.10f, 1.0f },
        .color_selection = { 0.24f, 0.52f, 0.88f, 0.45f },
        .color_caret = { 0.92f, 0.92f, 0.94f, 1.0f },

        // Docking
        .color_dock_preview = { 0.24f, 0.52f, 0.88f, 0.35f },
        .color_dock_tab_active = { 0.24f, 0.52f, 0.88f, 0.6f },

        // Sizing
        .padding = 10.f,
        .title_bar_height = 30.f,
        .resize_border_thickness = 8.f,
        .min_menu_size = { 150.f, 100.f },
        .font_size = 16.f,
        .menu_bar_height = 32.f,
        .font = {}
    };
}

constexpr auto gse::gui::style::darker() -> style {
    return style{
        // Window chrome - fully opaque, darker
        .color_title_bar = { 0.12f, 0.12f, 0.14f, 1.0f },
        .color_title_bar_inactive = { 0.08f, 0.08f, 0.10f, 1.0f },
        .color_menu_body = { 0.06f, 0.06f, 0.07f, 1.0f },
        .color_menu_bar = { 0.04f, 0.04f, 0.05f, 1.0f },
        .color_border = { 0.02f, 0.02f, 0.03f, 1.0f },

        // Text
        .color_text = { 0.88f, 0.88f, 0.90f, 1.0f },
        .color_text_secondary = { 0.55f, 0.55f, 0.58f, 1.0f },
        .color_text_disabled = { 0.35f, 0.35f, 0.38f, 1.0f },

        // Icons
        .color_icon = { 0.6f, 0.6f, 0.62f, 1.0f },
        .color_icon_hovered = { 0.95f, 0.95f, 0.97f, 1.0f },

        // Interactive widgets - fully opaque
        .color_widget_background = { 0.14f, 0.14f, 0.16f, 1.0f },
        .color_widget_hovered = { 0.20f, 0.20f, 0.23f, 1.0f },
        .color_widget_active = { 0.20f, 0.45f, 0.78f, 1.0f },
        .color_widget_selected = { 0.20f, 0.45f, 0.78f, 1.0f },

        // Sliders
        .color_slider_fill = { 0.20f, 0.45f, 0.78f, 1.0f },

        // Toggles
        .color_toggle_on = { 0.18f, 0.55f, 0.30f, 1.0f },
        .color_toggle_off = { 0.20f, 0.20f, 0.22f, 1.0f },

        // Handles
        .color_handle = { 0.85f, 0.85f, 0.87f, 1.0f },
        .color_handle_hovered = { 1.0f, 1.0f, 1.0f, 1.0f },

        // Input fields
        .color_input_background = { 0.04f, 0.04f, 0.05f, 1.0f },
        .color_selection = { 0.20f, 0.45f, 0.78f, 0.5f },
        .color_caret = { 0.88f, 0.88f, 0.90f, 1.0f },

        // Docking
        .color_dock_preview = { 0.20f, 0.45f, 0.78f, 0.4f },
        .color_dock_tab_active = { 0.20f, 0.45f, 0.78f, 0.7f },

        // Sizing
        .padding = 10.f,
        .title_bar_height = 30.f,
        .resize_border_thickness = 8.f,
        .min_menu_size = { 150.f, 100.f },
        .font_size = 16.f,
        .menu_bar_height = 32.f,
        .font = {}
    };
}

constexpr auto gse::gui::style::light() -> style {
    return style{
        // Window chrome
        .color_title_bar = { 0.85f, 0.85f, 0.87f, 1.0f },
        .color_title_bar_inactive = { 0.78f, 0.78f, 0.80f, 1.0f },
        .color_menu_body = { 0.94f, 0.94f, 0.95f, 1.0f },
        .color_menu_bar = { 0.90f, 0.90f, 0.92f, 1.0f },
        .color_border = { 0.75f, 0.75f, 0.78f, 1.0f },

        // Text
        .color_text = { 0.10f, 0.10f, 0.12f, 1.0f },
        .color_text_secondary = { 0.40f, 0.40f, 0.42f, 1.0f },
        .color_text_disabled = { 0.60f, 0.60f, 0.62f, 1.0f },

        // Icons
        .color_icon = { 0.35f, 0.35f, 0.38f, 1.0f },
        .color_icon_hovered = { 0.10f, 0.10f, 0.12f, 1.0f },

        // Interactive widgets
        .color_widget_background = { 0.82f, 0.82f, 0.84f, 1.0f },
        .color_widget_hovered = { 0.75f, 0.75f, 0.78f, 1.0f },
        .color_widget_active = { 0.30f, 0.58f, 0.92f, 1.0f },
        .color_widget_selected = { 0.30f, 0.58f, 0.92f, 0.9f },

        // Sliders
        .color_slider_fill = { 0.30f, 0.58f, 0.92f, 1.0f },

        // Toggles
        .color_toggle_on = { 0.25f, 0.72f, 0.40f, 1.0f },
        .color_toggle_off = { 0.70f, 0.70f, 0.72f, 1.0f },

        // Handles
        .color_handle = { 1.0f, 1.0f, 1.0f, 1.0f },
        .color_handle_hovered = { 0.95f, 0.95f, 0.97f, 1.0f },

        // Input fields
        .color_input_background = { 1.0f, 1.0f, 1.0f, 1.0f },
        .color_selection = { 0.30f, 0.58f, 0.92f, 0.35f },
        .color_caret = { 0.10f, 0.10f, 0.12f, 1.0f },

        // Docking
        .color_dock_preview = { 0.30f, 0.58f, 0.92f, 0.3f },
        .color_dock_tab_active = { 0.30f, 0.58f, 0.92f, 0.5f },

        // Sizing
        .padding = 10.f,
        .title_bar_height = 30.f,
        .resize_border_thickness = 8.f,
        .min_menu_size = { 150.f, 100.f },
        .font_size = 16.f,
        .menu_bar_height = 32.f,
        .font = {}
    };
}

constexpr auto gse::gui::style::high_contrast() -> style {
    return style{
        // Window chrome - pure black
        .color_title_bar = { 0.0f, 0.0f, 0.0f, 1.0f },
        .color_title_bar_inactive = { 0.0f, 0.0f, 0.0f, 1.0f },
        .color_menu_body = { 0.0f, 0.0f, 0.0f, 1.0f },
        .color_menu_bar = { 0.0f, 0.0f, 0.0f, 1.0f },
        .color_border = { 1.0f, 1.0f, 1.0f, 1.0f },

        // Text - pure white
        .color_text = { 1.0f, 1.0f, 1.0f, 1.0f },
        .color_text_secondary = { 0.8f, 0.8f, 0.8f, 1.0f },
        .color_text_disabled = { 0.5f, 0.5f, 0.5f, 1.0f },

        // Icons
        .color_icon = { 1.0f, 1.0f, 1.0f, 1.0f },
        .color_icon_hovered = { 1.0f, 1.0f, 0.0f, 1.0f },

        // Interactive widgets - high contrast yellow/cyan accents
        .color_widget_background = { 0.0f, 0.0f, 0.0f, 1.0f },
        .color_widget_hovered = { 0.2f, 0.2f, 0.0f, 1.0f },
        .color_widget_active = { 0.0f, 1.0f, 1.0f, 1.0f },
        .color_widget_selected = { 1.0f, 1.0f, 0.0f, 1.0f },

        // Sliders
        .color_slider_fill = { 0.0f, 1.0f, 1.0f, 1.0f },

        // Toggles
        .color_toggle_on = { 0.0f, 1.0f, 0.0f, 1.0f },
        .color_toggle_off = { 0.3f, 0.3f, 0.3f, 1.0f },

        // Handles
        .color_handle = { 1.0f, 1.0f, 1.0f, 1.0f },
        .color_handle_hovered = { 1.0f, 1.0f, 0.0f, 1.0f },

        // Input fields
        .color_input_background = { 0.0f, 0.0f, 0.0f, 1.0f },
        .color_selection = { 0.0f, 0.5f, 1.0f, 0.6f },
        .color_caret = { 1.0f, 1.0f, 1.0f, 1.0f },

        // Docking
        .color_dock_preview = { 1.0f, 1.0f, 0.0f, 0.5f },
        .color_dock_tab_active = { 1.0f, 1.0f, 0.0f, 0.8f },

        // Sizing
        .padding = 12.f,
        .title_bar_height = 32.f,
        .resize_border_thickness = 10.f,
        .min_menu_size = { 150.f, 100.f },
        .font_size = 18.f,
        .menu_bar_height = 36.f,
        .font = {}
    };
}

constexpr auto gse::gui::style::from_theme(const theme t) -> style {
    switch (t) {
        case theme::dark:          return dark();
        case theme::darker:        return darker();
        case theme::light:         return light();
        case theme::high_contrast: return high_contrast();
        default:                   return dark();
    }
}