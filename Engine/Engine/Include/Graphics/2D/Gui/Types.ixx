export module gse.graphics:types;

import std;

import gse.physics.math;
import gse.utility;

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

namespace gse::gui {
	struct menu_data {
		unitless::vec2 top_left;
		unitless::vec2 size;
		id parent_id;
	};

	struct menu : identifiable, identifiable_owned {
		explicit menu(
			const std::string& tag,
			const menu_data& data
		) : identifiable(tag),
			identifiable_owned(data.parent_id),
			rect(ui_rect::from_position_size(data.top_left, data.size)) {
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
		std::vector<std::string> items;

		std::uint32_t active_tab_index = 0;
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