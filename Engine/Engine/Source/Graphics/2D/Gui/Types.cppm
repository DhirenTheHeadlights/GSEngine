export module gse.graphics:types;

import std;

import gse.physics.math;
import gse.utility;

import :font;
import :texture;
import :ui_renderer;
import :styles;

export namespace gse::gui {
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

	struct tooltip_state {
		std::string text;
		id widget_id;
		id pending_widget_id;
		time hover_time{};
		unitless::vec2 position;
		bool visible = false;

		static constexpr time show_delay = seconds(0.5f);
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
		dock::location docked_to = dock::location::none;
		float dock_split_ratio = 0.5f;
	};

	struct draw_context;

	struct menu : identifiable, identifiable_owned {
		explicit menu(
			std::string_view tag,
			const menu_data& data
		);

		ui_rect rect;
		unitless::vec2 grab_offset;
		std::optional<unitless::vec2> pre_docked_size;
		bool grabbed = false;
		bool chrome_drawn_this_frame = false;
		float dock_split_ratio = 0.5f;
		resize_handle active_resize_handle = resize_handle::none;
		dock::location docked_to = dock::location::none;
		std::vector<std::string> tab_contents;
		std::uint32_t active_tab_index = 0;
		bool was_begun_this_frame = false;
	};

	struct draw_context {
		menu* current_menu;
		const style& style;
		const input::state& input;
		resource::handle<font> font;
		resource::handle<texture> blank_texture;
		unitless::vec2& layout_cursor;
		std::vector<renderer::sprite_command>& sprites;
		std::vector<renderer::text_command>& texts;

		render_layer current_layer = render_layer::content;
		std::uint32_t current_z_order = 0;

		render_layer input_layer = render_layer::content;
		tooltip_state* tooltip = nullptr;

		auto queue_sprite(
			renderer::sprite_command cmd
		) const -> void;

		auto queue_text(
			renderer::text_command cmd
		) const -> void;

		[[nodiscard]] auto input_available(
		) const -> bool;

		auto set_tooltip(
			const id& widget_id,
			const std::string& text
		) const -> void;
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

	struct pending_drag {
		id menu_id;
		unitless::vec2 start_position;
		unitless::vec2 offset;
		std::optional<std::uint32_t> tab_index;
	};
}

namespace gse::gui {
	using state = std::variant<
		states::idle,
		states::dragging,
		states::resizing,
		states::resizing_divider,
		states::pending_drag
	>;
}

gse::gui::menu::menu(std::string_view tag, const menu_data& data)
	: identifiable(tag)
	, identifiable_owned(data.parent_id)
	, rect(data.rect)
	, dock_split_ratio(data.dock_split_ratio)
	, docked_to(data.docked_to) {
	tab_contents.emplace_back(tag);
}

auto gse::gui::draw_context::queue_sprite(renderer::sprite_command cmd) const -> void {
	cmd.layer = current_layer;
	cmd.z_order = current_z_order;
	sprites.push_back(std::move(cmd));
}

auto gse::gui::draw_context::queue_text(renderer::text_command cmd) const -> void {
	cmd.layer = current_layer;
	cmd.z_order = current_z_order;
	texts.push_back(std::move(cmd));
}

auto gse::gui::draw_context::input_available() const -> bool {
	return static_cast<std::uint8_t>(current_layer) >= static_cast<std::uint8_t>(input_layer);
}

auto gse::gui::draw_context::set_tooltip(const id& widget_id, const std::string& text) const -> void {
	if (!tooltip || text.empty()) {
		return;
	}

	tooltip->pending_widget_id = widget_id;
	tooltip->text = text;
	tooltip->position = input.mouse_position();
}