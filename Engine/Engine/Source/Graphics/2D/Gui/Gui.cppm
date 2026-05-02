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
import gse.save;

import :settings_panel;
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

export namespace gse::gui {
	struct system_state;
}

namespace gse::gui {
	struct frame_state {
		style sty{};
		bool active = false;
	};

	auto handle_idle_state(
		system_state& s,
		const input::state& input_state,
		vec2f mouse_position,
		bool mouse_held,
		const style& style
	) -> state;

	auto handle_dragging_state(
		system_state& s,
		const states::dragging& current,
		const window& window,
		vec2f mouse_position,
		bool mouse_held,
		const gpu::context& ctx
	) -> state;

	auto handle_resizing_state(
		system_state& s,
		const states::resizing& current,
		vec2f mouse_position,
		bool mouse_held,
		const style& style,
		const gpu::context& ctx
	) -> state;

	auto handle_resizing_divider_state(
		system_state& s,
		const states::resizing_divider& current,
		vec2f mouse_position,
		bool mouse_held,
		const style& style
	) -> state;

	auto handle_pending_drag_state(
		system_state& s,
		const states::pending_drag& current,
		vec2f mouse_position,
		bool mouse_held
	) -> state;

	auto draw_menu_chrome(
		system_state& s,
		const input::state& input_state,
		menu& current_menu
	) -> void;

	auto draw_tab_bar(
		system_state& s,
		const input::state& input_state,
		menu& current_menu,
		const ui_rect& title_bar_rect
	) -> void;

	auto usable_screen_rect(
		system_state& s,
		const gpu::context& ctx
	) -> ui_rect;

	auto calculate_display_rect(
		system_state& s,
		const menu& m
	) -> ui_rect;

	auto apply_scale(
		system_state& s,
		style sty,
		float viewport_height
	) -> style;

	auto reload_font(
		system_state& s,
		const asset::registry& assets
	) -> void;
}

export namespace gse::gui {
	struct system {
		struct resources {
			std::unique_ptr<ids::scope> current_scope;
		};

		static auto initialize(init_context& phase, resources& r, system_state& s) -> void;
		static auto update(update_context& ctx, resources& r, system_state& s) -> async::task<>;
		static auto shutdown(shutdown_context& phase, resources& r, system_state& s) -> void;

		static auto save(system_state& s) -> void;
	};

	struct system_state {
		system_state();
		~system_state();
		system_state(system_state&&) noexcept;
		auto operator=(system_state&&) noexcept -> system_state&;

		id_mapped_collection<menu> menus;
		menu* current_menu = nullptr;

		resource::handle<font> gui_font;
		resource::handle<texture> blank_texture;

		std::optional<dock::space> active_dock_space;
		state current_state;

		std::filesystem::path file_path = "Misc/gui_layout.toml";
		clock save_clock;

		id hot_widget_id;
		id active_widget_id;
		id focus_widget_id;

		frame_state fstate{};
		draw_context* context = nullptr;

		menu_bar::state menu_bar_state;
		settings_panel::state settings_panel_state;
		theme current_theme = theme::dark;
		float ui_scale = 1.0f;
		int font_index = 0;
		std::vector<std::string> available_fonts;

		std::vector<renderer::sprite_command> sprite_commands;
		std::vector<renderer::text_command> text_commands;

		std::vector<id> visible_menu_ids_last_frame;
		vec2f previous_viewport_size;

		tooltip_state tooltip;
		render_layer input_layer_render = render_layer::content;
		input_layer input_layers_data;
		std::unordered_map<std::uint64_t, scroll_state> widget_scrolls;
		std::unordered_map<std::uint64_t, vec4f> widget_anim_colors;

		auto scroll_for(const std::uint64_t key) -> scroll_state& {
			return widget_scrolls[key];
		}

		static constexpr time update_interval = seconds(30.f);
	};

	auto begin_menu(
		system::resources& r,
		system_state& s,
		const std::string& name
	) -> bool;

	auto end_menu(
		system::resources& r,
		system_state& s
	) -> void;

	auto process_menu(
		system::resources& r,
		system_state& s,
		const input::state& input_state,
		const std::string& name,
		const std::function<void(builder&)>& build
	) -> void;
}
