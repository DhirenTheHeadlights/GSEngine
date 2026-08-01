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
import :styles;
import :builder;
import :menu_stack;
import :render_layer;
import :symbols;

namespace gse::gui {
	struct frame_state {
		style sty{};
		bool active = false;
	};

	auto draw_screen_caption(
		builder& b,
		screen& top,
		const rectf& bar_rect,
		const rectf& full_rect,
		channel_writer channels
	) -> void;
}

export namespace gse::gui {
	struct [[= gse::system_state<"Gui">{}, = gse::settings::category<"UI">{}]] data {
		[[
			= gse::settings::describe<"Color theme applied to all UI panels and widgets.">{}
		]]
		theme current_theme = theme::midnight;

		[[
			= gse::settings::
				describe<"Multiplier on UI element sizes and font metrics. Useful for high-DPI displays.">{},
			= gse::settings::range<0.5f, 2.0f>{}
		]]
		float ui_scale = 1.0f;

		[[
			= gse::settings::describe<"Font used for UI text: labels, menus, and controls.">{}
		]]
		gse::settings::choice<int> ui_font{ .value = -1 };

		[[
			= gse::settings::describe<"Font used for code, terminals, and numeric readouts.">{}
		]]
		gse::settings::choice<int> code_font{ .value = -1 };

		[[
			= gse::settings::describe<"Show developer overlays (Test, Profiler, Physics Debug).">{},
			= gse::shared
		]]
		bool show_dev_overlays = false;

		bool scale_with_resolution = true;
		bool reserve_top_bar = false;
		std::uint32_t next_z_order = 1;

		[[= gse::shared]] id_mapped_collection<menu> menus;
		menu* current_menu = nullptr;

		[[= gse::shared]] font_set fonts;
		[[= gse::shared]] resource::handle<texture> blank_texture;

		std::optional<dock::space> active_dock_space;
		gui::state current_state{ states::idle{} };

		std::filesystem::path file_path = config::user_config_dir() / "gui_layout.ini";
		clock save_clock;

		id hot_widget_id;
		id active_widget_id;
		id focus_widget_id;

		frame_state fstate{};
		draw_context* context = nullptr;

		int last_ui_font_index = 0;
		int last_code_font_index = 0;

		std::vector<renderer::sprite_command> sprite_commands;
		std::vector<renderer::text_command> text_commands;
		per_frame_resource<std::deque<std::string>> text_pools;
		std::size_t text_pool_slot = 0;
		std::size_t text_pool_used = 0;

		std::vector<id> visible_menu_ids_last_frame;
		std::unordered_map<std::uint64_t, id> name_to_menu_id;
		vec2f previous_viewport_size;

		tooltip_state tooltip;
		render_layer input_layer_render = render_layer::content;
		input_layer input_layers_data;
		std::unordered_map<std::uint64_t, scroll_state> widget_scrolls;
		std::unordered_map<std::uint64_t, vec4f> widget_anim_colors;

		std::unique_ptr<ids::scope> current_scope;

		[[= gse::shared]] menu_stack_state menu_stack;
		std::optional<menu> screen_surface;
		bool manual_cursor = false;

		std::vector<id> pending_popout_close_ids;
		std::optional<std::pair<id, std::uint32_t>> pending_tab_close;
		context_menu_state context_menu;

		static constexpr time update_interval = seconds(30.f);
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<window::data> window_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<window::data> window_s,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<gse::input::data> input_state,
		const save::registry& save_reg,
		data& d
	) -> async::task<>;

	auto shutdown(
		data& d
	) -> void;

	auto save(
		data& d
	) -> void;

	auto clear_menu_interaction(
		data& d
	) -> void;

	auto init_body(
		context& ctx,
		shared_view<window::data> window_s,
		shared_view<asset::data> assets,
		data& d
	) -> async::task<>;

	auto update_body(
		context& ctx,
		shared_view<window::data> window_s,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<gse::input::data> input_state,
		const save::registry& save_reg,
		data& d
	) -> async::task<>;

	auto handle_idle_state(
		data& d,
		const input::state& input_state,
		vec2f mouse_position,
		bool mouse_held,
		const style& style
	) -> gui::state;

	auto handle_dragging_state(
		data& d,
		const states::dragging& current,
		shared_view<window::data> window_s,
		vec2f mouse_position,
		bool mouse_held
	) -> gui::state;

	auto handle_resizing_state(
		data& d,
		const states::resizing& current,
		vec2f mouse_position,
		bool mouse_held,
		const style& style,
		shared_view<window::data> window_s
	) -> gui::state;

	auto handle_resizing_divider_state(
		data& d,
		const states::resizing_divider& current,
		vec2f mouse_position,
		bool mouse_held,
		const style& style
	) -> gui::state;

	auto handle_pending_drag_state(
		data& d,
		const states::pending_drag& current,
		vec2f mouse_position,
		bool mouse_held
	) -> gui::state;

	auto draw_menu_chrome(
		data& d,
		const input::state& input_state,
		menu& current_menu,
		render_layer layer
	) -> void;

	auto draw_tab_bar(
		data& d,
		const input::state& input_state,
		menu& current_menu,
		const rectf& title_bar_rect,
		render_layer layer
	) -> void;

	auto process_context_menu(
		data& d,
		const input::state& input_state,
		vec2f viewport_size,
		channel_writer& channels
	) -> void;

	auto usable_screen_rect(
		data& d,
		shared_view<window::data> window_s
	) -> rectf;

	auto calculate_display_rect(
		data& d,
		const menu& m
	) -> rectf;

	auto apply_scale(
		const data& d,
		style sty,
		float viewport_height
	) -> style;

	auto reload_font(
		data& d,
		shared_view<asset::data> assets
	) -> void;

	auto begin_menu(
		data& d,
		const std::string& name
	) -> bool;

	auto end_menu(
		data& d
	) -> void;

	auto process_menu(
		data& d,
		const input::state& input_state,
		const std::string& name,
		render_layer layer,
		const std::function<void(builder&)>& build
	) -> void;

	auto caption_button(
		builder& b,
		const rectf& rect,
		const std::string& key,
		std::span<const symbol::stroke> glyph,
		vec4f hover_color
	) -> bool;

	auto process_screen(
		data& d,
		const input::state& input_state,
		vec2f viewport_size,
		channel_writer channels
	) -> void;
}
