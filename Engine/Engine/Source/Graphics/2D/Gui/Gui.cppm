export module gse.graphics:gui;

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

import :settings;
import :types;
import :layout;
import :font;
import :ui_renderer;
import :texture;
import :cursor;
import :save;
import :ids;
import :input_layers;
import :menu_bar;
import :styles;
import :builder;

namespace gse::gui {
	struct frame_state {
		style sty{};
		bool active = false;
	};
}

export namespace gse::gui {
	class system {
	public:
		struct settings {
			static constexpr std::string_view category = "UI";

			[[=gse::settings::describe<"Color theme applied to all UI panels and widgets.">{}]]
			theme current_theme = theme::dark;

			[[=gse::settings::describe<"Multiplier on UI element sizes and font metrics. Useful for high-DPI displays.">{}, =gse::settings::range<0.5f, 2.0f>{}]]
			float ui_scale = 1.0f;

			[[=gse::settings::describe<"Font used to render text in the UI.">{}]]
			gse::settings::choice<int> font;
		};

		struct state {
			id_mapped_collection<menu> menus;
			menu* current_menu = nullptr;

			resource::handle<font> gui_font;
			resource::handle<texture> blank_texture;

			std::optional<dock::space> active_dock_space;
			gui::state current_state{ states::idle{} };

			std::filesystem::path file_path = "Misc/gui_layout.ini";
			clock save_clock;

			id hot_widget_id;
			id active_widget_id;
			id focus_widget_id;

			frame_state fstate{};
			draw_context* context = nullptr;

			menu_bar::state menu_bar_state;
			gse::settings::panel_state settings_state;

			int last_font_index = 0;

			std::vector<renderer::sprite_command> sprite_commands;
			std::vector<renderer::text_command> text_commands;

			std::vector<id> visible_menu_ids_last_frame;
			vec2f previous_viewport_size;

			tooltip_state tooltip;
			render_layer input_layer_render = render_layer::content;
			input_layer input_layers_data;
			std::unordered_map<std::uint64_t, scroll_state> widget_scrolls;
			std::unordered_map<std::uint64_t, vec4f> widget_anim_colors;

			static constexpr time update_interval = seconds(30.f);
		};

		struct resources {
			std::unique_ptr<ids::scope> current_scope;
		};

		static auto run(
			run_context& ctx,
			const window::state& window_s,
			const asset::state& assets_s,
			const gse::input::system::state& input_state,
			settings& cfg,
			resources& r,
			state& s
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			resources& r,
			state& s
		) -> void;

		static auto save(state& s) -> void;

	private:
		static auto init_body(
			run_context& ctx,
			const window::state& window_s,
			asset::state& assets,
			settings& cfg,
			state& s
		) -> async::task<>;

		static auto update_body(
			run_context& ctx,
			const window::state& window_s,
			const asset::state& assets_s,
			const gse::input::system::state& input_state,
			const settings& cfg,
			resources& r,
			state& s
		) -> async::task<>;

		static auto handle_idle_state(
			state& s,
			const input::state& input_state,
			vec2f mouse_position,
			bool mouse_held,
			const style& style
		) -> gui::state;

		static auto handle_dragging_state(
			state& s,
			const settings& cfg,
			const states::dragging& current,
			const window::state& window_s,
			vec2f mouse_position,
			bool mouse_held
		) -> gui::state;

		static auto handle_resizing_state(
			state& s,
			const settings& cfg,
			const states::resizing& current,
			vec2f mouse_position,
			bool mouse_held,
			const style& style,
			const window::state& window_s
		) -> gui::state;

		static auto handle_resizing_divider_state(
			state& s,
			const states::resizing_divider& current,
			vec2f mouse_position,
			bool mouse_held,
			const style& style
		) -> gui::state;

		static auto handle_pending_drag_state(
			state& s,
			const states::pending_drag& current,
			vec2f mouse_position,
			bool mouse_held
		) -> gui::state;

		static auto draw_menu_chrome(
			state& s,
			const input::state& input_state,
			menu& current_menu,
			render_layer layer
		) -> void;

		static auto draw_tab_bar(
			state& s,
			const input::state& input_state,
			menu& current_menu,
			const ui_rect& title_bar_rect,
			render_layer layer
		) -> void;

		static auto usable_screen_rect(
			state& s,
			const settings& cfg,
			const window::state& window_s
		) -> ui_rect;

		static auto calculate_display_rect(
			state& s,
			const menu& m
		) -> ui_rect;

		static auto apply_scale(
			const settings& cfg,
			style sty,
			float viewport_height
		) -> style;

		static auto reload_font(
			state& s,
			const settings& cfg,
			const asset::state& assets
		) -> void;

		static auto begin_menu(
			resources& r,
			state& s,
			const std::string& name
		) -> bool;

		static auto end_menu(
			resources& r,
			state& s
		) -> void;

		static auto process_menu(
			resources& r,
			state& s,
			const input::state& input_state,
			const std::string& name,
			render_layer layer,
			const std::function<void(builder&)>& build
		) -> void;
	};
}
