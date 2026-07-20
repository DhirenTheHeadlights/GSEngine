export module gse.graphics:types;

import std;

import gse.math;
import gse.os;
import gse.core;
import gse.time;

import :font;
import :texture;
import :ui_renderer;
import :styles;
import :render_layer;
import :input_layers;

export namespace gse::gui {

	namespace symbol {
		struct stroke {
			vec2f from;
			vec2f to;
		};
	}

	using symbol_glyph = std::span<const symbol::stroke> (*)();

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
		vec2f position;
		bool visible = false;

		static constexpr time show_delay = seconds(0.5f);
	};

	struct scroll_axis {
		float offset = 0.f;
		float target = 0.f;
		float velocity = 0.f;
		float content = 0.f;
		bool held = false;
		bool hovered = false;
		float grab = 0.f;
	};

	struct scroll_state {
		scroll_axis y;
		scroll_axis x;
		bool auto_scroll_active = false;
		vec2f auto_scroll_anchor;
	};

	struct scroll_config {
		float scrollbar_width = 8.f;
		float scrollbar_min_height = 20.f;
		float scroll_speed = 40.f;
		float smooth_factor = 0.15f;
		bool auto_hide_scrollbar = true;
		bool smooth_scrolling = true;
	};

	struct scroll_bar_input {
		rectf track_rect;
		float visible_extent = 0.f;
		float content_extent = 0.f;
		bool horizontal = false;
		vec2f mouse;
		bool mouse_pressed = false;
		bool mouse_held = false;
		float min_thumb = 20.f;
	};

	struct scroll_bar_result {
		rectf track_rect;
		rectf thumb_rect;
		bool visible = false;
		bool hovered = false;
		bool held = false;
		bool used_press = false;
	};

	struct scroll_region_info {
		std::string_view id;
		vec2f size{ 0.f, 0.f };
		scroll_config config{};
	};

	struct tab_strip_state {
		scroll_axis scroll{};
		std::uint64_t scroll_active = 0;
		std::uint64_t dragging = 0;
		std::uint32_t visible_rows = 1;
		float spinner_phase = 0.f;
	};
}

export namespace gse::gui::dock {
	enum class location {
		none,
		center,
		left,
		right,
		top,
		bottom
	};

	struct area {
		rectf rect;
		rectf target;
		location dock_location = location::none;

		~area() noexcept = default;
	};

	struct space {
		std::array<area, 5> areas;

		~space() noexcept = default;
	};
}

export namespace gse::gui {
	struct menu_data {
		rectf rect;
		id parent_id;
		dock::location docked_to = dock::location::none;
		float dock_split_ratio = 0.5f;
	};

	struct draw_context;
	struct layer_scope;

	struct font_set {
		resource::handle<font> text;
		resource::handle<font> code;
		std::unordered_map<std::string, resource::handle<font>> registry;

		[[nodiscard]] auto named(
			std::string_view name
		) const -> resource::handle<font>;
	};

	struct menu : identifiable, identifiable_owned {
		explicit menu(
			std::string_view tag,
			const menu_data& data
		);

		rectf rect;
		vec2f grab_offset;
		std::optional<vec2f> pre_docked_size;
		bool grabbed = false;
		bool chrome_drawn_this_frame = false;
		float dock_split_ratio = 0.5f;
		resize_handle active_resize_handle = resize_handle::none;
		dock::location docked_to = dock::location::none;
		bool fixed = false;
		bool bare = false;
		std::vector<std::string> tab_contents;
		std::uint32_t active_tab_index = 0;
		tab_strip_state tab_bar;
		std::uint32_t z_order = 0;
		bool was_begun_this_frame = false;
		bool was_visible_last_frame = false;
	};

	struct menu_item {
		std::string label;
		symbol_glyph icon = nullptr;
		std::uint32_t action_id = 0;
		bool destructive = false;
		bool enabled = true;
		bool separator_before = false;
	};

	struct context_menu_open {
		vec2f position;
		std::vector<menu_item> items;
		std::uint64_t target = 0;
		id tag;
	};

	struct context_menu_state {
		bool open = false;
		bool just_opened = false;
		vec2f position;
		std::vector<menu_item> items;
		std::uint64_t target = 0;
		id tag;
	};

	struct context_menu_result {
		id tag;
		std::uint32_t action_id = 0;
		std::uint64_t target = 0;
	};

	struct draw_context {
		menu* current_menu;
		const style& style;
		const input::state& input;
		const font_set& fonts;
		resource::handle<texture> blank_texture;
		vec2f& layout_cursor;
		std::vector<renderer::sprite_command>& sprites;
		std::vector<renderer::text_command>& texts;
		std::unordered_map<std::uint64_t, vec4f>& widget_anim_colors;
		std::unordered_map<std::uint64_t, scroll_state>& widget_scrolls;

		mutable render_layer current_layer = render_layer::content;
		std::uint32_t current_z_order = 0;

		render_layer input_layer = render_layer::content;
		class input_layer* hit_regions = nullptr;
		tooltip_state* tooltip = nullptr;
		context_menu_state* context_menu = nullptr;
		std::vector<rectf> clip_stack;

		auto queue_sprite(
			renderer::sprite_command cmd
		) const -> void;

		auto queue_text(
			renderer::text_command cmd
		) const -> void;

		auto open_context_menu(
			context_menu_open request
		) const -> void;

		auto set_clipboard(
			const std::string& text
		) const -> void;

		auto clipboard() const -> std::string;

		auto register_hit_region(
			render_layer layer,
			const rectf& rect
		) const -> void;

		[[nodiscard]] auto input_available() const -> bool;

		[[nodiscard]] auto input_available_at(
			vec2f position
		) const -> bool;

		[[nodiscard]]
		auto mouse_pressed_for(
			const rectf& rect,
			mouse_button button = mouse_button::button_1
		) const -> bool;

		[[nodiscard]]
		auto mouse_released_for(
			const rectf& rect,
			mouse_button button = mouse_button::button_1
		) const -> bool;

		auto consume_press(
			mouse_button button = mouse_button::button_1
		) const -> void;

		auto consume_release(
			mouse_button button = mouse_button::button_1
		) const -> void;

		[[nodiscard]] auto is_press_consumed(
			mouse_button button = mouse_button::button_1
		) const -> bool;

		[[nodiscard]] auto scroll_delta_for(
			const rectf& rect
		) const -> vec2f;

		auto consume_scroll() const -> void;

		[[nodiscard]] auto is_scroll_consumed() const -> bool;

		[[nodiscard]] auto key_pressed_for(
			key k
		) const -> bool;

		auto consume_key_press(
			key k
		) const -> void;

		auto set_tooltip(
			const id& widget_id,
			std::string_view text
		) const -> void;

		auto next_row(
			const resource::handle<font>& font,
			float height_multiplier = 1.f
		) const -> rectf;

		auto animated_color(
			const id& widget_id,
			vec4f target,
			float speed = 10.f
		) const -> vec4f;

		[[nodiscard]] auto current_clip() const -> std::optional<rectf>;

		[[nodiscard]] auto scoped_layer(
			render_layer layer
		) const -> layer_scope;
	};

	struct layer_scope : non_copyable {
		layer_scope() noexcept = default;

		layer_scope(
			const draw_context& ctx,
			render_layer layer
		) noexcept;

		layer_scope(
			layer_scope&& other
		) noexcept;

		auto operator=(
			layer_scope&& other
		) noexcept -> layer_scope&;

		~layer_scope() noexcept;

	private:
		const draw_context* m_ctx = nullptr;
		render_layer m_saved = render_layer::content;
		bool m_active = false;
	};

	struct scroll_handle : non_copyable {
		scroll_handle() noexcept = default;

		scroll_handle(
			draw_context& ctx,
			scroll_state& state,
			const rectf& visible_rect,
			float saved_layout_y,
			const scroll_config& config
		) noexcept;

		scroll_handle(
			scroll_handle&& other
		) noexcept;

		auto operator=(
			scroll_handle&& other
		) noexcept -> scroll_handle&;

		~scroll_handle() noexcept;

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto visible_rect() const -> const rectf&;

		[[nodiscard]] auto offset() const -> float;

	private:
		draw_context* m_ctx = nullptr;
		scroll_state* m_state = nullptr;
		rectf m_visible_rect;
		rectf m_saved_menu_rect;
		float m_saved_layout_y = 0.f;
		float m_content_start_y = 0.f;
		scroll_config m_config{};
		bool m_active = false;
	};

	[[nodiscard]] auto scroll_region(
		draw_context& ctx,
		const scroll_region_info& info
	) -> scroll_handle;

	auto scroll_area(
		const draw_context& ctx,
		scroll_state& state,
		const rectf& visible_rect,
		vec2f content_size,
		const scroll_config& config = {}
	) -> vec2f;

	auto update_scroll_bar(
		scroll_axis& axis,
		const scroll_bar_input& input
	) -> scroll_bar_result;
}

namespace gse::gui::states {
	struct idle {};

	struct dragging {
		id menu_id;
		vec2f offset;
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
		vec2f start_position;
		vec2f offset;
		std::optional<std::uint32_t> tab_index;
	};
}

namespace gse::gui {
	struct state {
		using value_type = std::
			variant<states::idle, states::dragging, states::resizing, states::resizing_divider, states::pending_drag>;

		value_type v;

		template <typename T>
		requires(!std::same_as<std::remove_cvref_t<T>, state>) && std::constructible_from<value_type, T&&>
		state(T&& x) : v(std::forward<T>(x)) {
		}

		template <typename T>
		requires(!std::same_as<std::remove_cvref_t<T>, state>) && std::assignable_from<value_type&, T&&>
		auto operator=(T&& x) -> state& {
			v = std::forward<T>(x);
			return *this;
		}
	};
}
