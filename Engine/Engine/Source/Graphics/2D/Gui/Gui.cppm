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
import :font;
import :ui_renderer;
import :texture;
import :save;
import :ids;
import :input_layers;
import :styles;
import :builder;
import :menu_stack;
import :render_layer;
import :text_select;

namespace gse::gui {
	struct frame_state {
		style sty{};
		bool active = false;
	};
}

export namespace gse::gui {
	struct viewport_chrome {
		float caption_height = 0.f;
		float controls_width = 0.f;
		int resize_exclude_y0 = 0;
		int resize_exclude_y1 = 0;
	};

	struct viewport_state {
		[[= shared]] menu_stack_state menu_stack;
		[[= shared]] id_mapped_collection<menu> menus;
		menu* current_menu = nullptr;
		std::unique_ptr<ids::scope> current_scope;
		std::uint32_t next_z_order = 1;
		std::vector<id> visible_menu_ids_last_frame;
		std::unordered_map<std::uint64_t, id> name_to_menu_id;
		std::unordered_set<std::uint64_t> suppressed_menus;
		bool adopts_unclaimed_content = false;

		std::optional<dock::space> active_dock_space;
		std::optional<drag_ghost> active_drag_ghost;
		state current_state{ states::idle{} };
		id hot_widget_id;
		id active_widget_id;
		id focus_widget_id;
		tooltip_state tooltip;
		render_layer input_layer_render = render_layer::content;
		bool input_suppressed = false;
		bool owns_cursor = true;
		bool owns_keyboard = true;
		input_layer input_layers_data;
		context_menu_state context_menu;
		text_selection_state text_selection;
		std::optional<text_edit_request> pending_text_edit;
		std::optional<menu> screen_surface;
		bool manual_cursor = false;
		std::vector<id> pending_popout_close_ids;
		std::optional<std::pair<id, std::uint32_t>> pending_tab_close;
		std::optional<caption_action> pending_caption_action;
		viewport_chrome chrome;

		frame_state fstate{};
		id window;
		rectf frame_rect;
		rectf rect;
		vec2f previous_viewport_size;
		float previous_scale_factor = 0.f;
		float display_scale = 1.f;
		std::string active_monitor_key;
	};

	struct [[= system_state<"Gui">{}, = settings::category<"UI">{}]] data {
		[[
			= settings::describe<"Color theme applied to all UI panels and widgets.">{}
		]]
		theme current_theme = theme::midnight;

		std::optional<style> style_override;

		[[
			= settings::
				describe<"Multiplier on UI element sizes and font metrics, on top of the display scale. Remembered per monitor.">{},
			= settings::range<0.5f, 2.0f>{}
		]]
		float ui_scale = 1.0f;

		[[
			= settings::describe<"Font used for UI text: labels, menus, and controls.">{}
		]]
		settings::choice ui_font;

		[[
			= settings::describe<"Font used for code, terminals, and numeric readouts.">{}
		]]
		settings::choice code_font;

		[[
			= settings::describe<"Show developer overlays (Test, Profiler, Physics Debug).">{},
			= shared
		]]
		bool show_dev_overlays = false;

		[[
			= settings::describe<"Derive UI scale from viewport height instead of the monitor display scale.">{},
			= settings::app_scope{}
		]]
		bool scale_with_resolution = true;

		std::unordered_map<std::string, float> ui_scale_by_monitor;

		[[
			= settings::describe<"Reserve a title-bar strip at the top of the viewport for application chrome.">{},
			= settings::app_scope{}
		]]
		bool reserve_top_bar = false;


		[[= shared]] font_set fonts;
		[[= shared]] resource::handle<texture> blank_texture;

		[[
			= settings::describe<"File the dock and window layout is saved to and restored from.">{},
			= settings::app_scope{}
		]]
		std::filesystem::path file_path = config::user_config_dir() / "gui_layout.ini";
		clock save_clock;

		draw_context* context = nullptr;

		std::string last_ui_font;
		std::string last_code_font;

		std::vector<renderer::sprite_command> sprite_commands;
		std::vector<renderer::text_command> text_commands;
		per_frame_resource<std::deque<std::string>> text_pools;
		std::size_t text_pool_slot = 0;
		std::size_t text_pool_used = 0;

		std::unordered_map<std::uint64_t, scroll_state> widget_scrolls;
		std::unordered_map<std::uint64_t, vec4f> widget_anim_colors;
		std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> widget_tree_open;

		[[= shared]] viewport_state primary{ .adopts_unclaimed_content = true };
		std::vector<std::unique_ptr<viewport_state>> secondaries;

		static constexpr time update_interval = seconds(30.f);
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<window::data> window_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<window::data> window_s,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<input::data> input_state,
		channel_read<push_screen_request, pop_screen_request, clear_screens_request, set_manual_cursor_request, menu_content, popout_closed, menu_migrate_request, window_opened, window_closed, window_resized> requests_in,
		channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> ui_out,
		data& d
	) -> async::task<>;

	auto shutdown(
		data& d
	) -> void;

	auto save(
		data& d
	) -> void;
}

namespace gse::gui {
	[[nodiscard]] auto is_popout(
		const viewport_state& vp
	) -> bool;
}