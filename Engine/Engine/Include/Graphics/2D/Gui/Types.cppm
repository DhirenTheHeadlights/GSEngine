export module gse.graphics:types;

import std;

import gse.physics.math;
import gse.utility;

import :font;
import :texture;
import :text_renderer;
import :sprite_renderer;

export namespace gse::gui {
	struct style {
		unitless::vec4 color_title_bar = { 0.15f, 0.15f, 0.15f, 0.9f };
        unitless::vec4 color_menu_body = { 0.1f, 0.1f, 0.1f, 0.01f };
        unitless::vec4 color_text = { 0.9f, 0.9f, 0.9f, 1.0f };
        unitless::vec4 color_dock_preview = { 0.2f, 0.2f, 0.8f, 0.4f };
        unitless::vec4 color_dock_widget = { 0.8f, 0.8f, 0.8f, 0.5f };
		unitless::vec4 color_widget_background = { 0.2f, 0.2f, 0.2f, 0.5f };
		unitless::vec4 color_widget_fill = { 0.2f, 0.6f, 0.2f, 0.8f };

        float padding = 10.f;
        float title_bar_height = 30.f;
        float resize_border_thickness = 8.f;
        unitless::vec2 min_menu_size = { 150.f, 100.f };

        float font_size = 16.f;
		std::filesystem::path font;
	};

	using ui_rect = rect_t<unitless::vec2>;

	enum class resize_handle {
		none,
		left,
		right,
		top,
		bottom,
		top_left,
		top_right,
		bottom_left,
		bottom_right,
	};
}
namespace gse::gui::dock {
	enum class location {
		none,
		center,
		left,
		right,
		top,
		bottom
	};

	struct area {
		ui_rect rect;
		ui_rect target;
		location dock_location = location::none;
	};

	struct space {
		std::array<area, 5> areas;
	};
}

export namespace gse::gui {
	struct menu_data {
		ui_rect rect;
		id parent_id;
	};

	struct widget_context;

	struct menu : identifiable, identifiable_owned {
		explicit menu(
			const std::string& tag,
			const menu_data& data
		) : identifiable(tag),
			identifiable_owned(data.parent_id),
			rect(data.rect) {
			tab_contents.emplace_back(tag);
		}

		ui_rect rect;

		unitless::vec2 grab_offset;
		std::optional<unitless::vec2> pre_docked_size;

		bool grabbed = false;
		float dock_split_ratio = 0.5f;

		resize_handle active_resize_handle = resize_handle::none;
		dock::location docked_to = dock::location::none;

		std::vector<std::string> tab_contents;
		std::vector<std::function<void(widget_context&)>> commands;

		std::uint32_t active_tab_index = 0;
		bool was_begun_this_frame = false;
	};

	struct widget_context {
		menu* current_menu;
        const style& style;
        renderer::sprite& sprite_renderer;
        renderer::text& text_renderer;
        resource::handle<font> font;
        resource::handle<texture> blank_texture;
        unitless::vec2& layout_cursor;
	};
}

namespace gse::gui::states {
	struct idle {};

	struct dragging {
		id menu_id;
		unitless::vec2 offset;
	};

	struct resizing {
		id menu_id;
		resize_handle handle;
	};

	struct resizing_divider {
		id parent_id;
		id child_id;
	};
}

namespace gse::gui {
	using state = std::variant<
		states::idle,
		states::dragging,
		states::resizing,
		states::resizing_divider
	>;
}