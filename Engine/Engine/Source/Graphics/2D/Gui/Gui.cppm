export module gse.graphics:gui;

import std;

import gse.platform;
import gse.utility;
import gse.math;

import :types;
import :layout;
import :font;
import :ui_renderer;
import :texture;
import :cursor;
import :save;
import :value_widget;
import :text_widget;
import :input_widget;
import :slider_widget;
import :ids;
import :button_widget;
export import :tree_widget;
import :selectable_widget;
import :dropdown_widget;
import :scroll_widget;
import :input_layers;
import :menu_bar;
import :settings_panel;
import :styles;

namespace gse::gui {
	struct frame_state {
		const gpu::context* rctx = nullptr;
		style sty{};
		bool active = false;
	};

	auto handle_idle_state(
		struct system_state& s,
		const input::state& input_state,
		unitless::vec2 mouse_position,
		bool mouse_held,
		const style& style
	) -> gui::state;

	auto handle_dragging_state(
		system_state& s,
		const states::dragging& current,
		const window& window,
		unitless::vec2 mouse_position,
		bool mouse_held
	) -> gui::state;

	auto handle_resizing_state(
		system_state& s,
		const states::resizing& current,
		unitless::vec2 mouse_position,
		bool mouse_held,
		const style& style
	) -> gui::state;

	auto handle_resizing_divider_state(
		system_state& s,
		const states::resizing_divider& current,
		unitless::vec2 mouse_position,
		bool mouse_held,
		const style& style
	) -> gui::state;

	auto handle_pending_drag_state(
		system_state& s,
		const states::pending_drag& current,
		unitless::vec2 mouse_position,
		bool mouse_held
	) -> gui::state;

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
		const system_state& s
	) -> ui_rect;

	auto calculate_display_rect(
		const system_state& s,
		const menu& m
	) -> ui_rect;

	auto apply_scale(
		const system_state& s,
		style sty,
		float viewport_height
	) -> style;

	auto reload_font(
		system_state& s
	) -> void;

	auto begin_menu(
		system_state& s,
		const std::string& name
	) -> bool;

	auto end_menu(
		system_state& s
	) -> void;
}

export namespace gse::gui {
	struct system_state {
		gpu::context* rctx = nullptr;

		id_mapped_collection<menu> menus;
		menu* current_menu = nullptr;

		resource::handle<font> gui_font;
		resource::handle<texture> blank_texture;

		std::optional<dock::space> active_dock_space;
		gui::state current_state;

		std::filesystem::path file_path = "Misc/gui_layout.toml";
		clock save_clock;

		id hot_widget_id;
		id active_widget_id;
		id focus_widget_id;

		std::unique_ptr<ids::scope> current_scope;

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
		unitless::vec2 previous_viewport_size;

		tooltip_state tooltip;
		render_layer input_layer_render = render_layer::content;
		input_layer input_layers_data;

		static constexpr time update_interval = seconds(30.f);

		explicit system_state(gpu::context& c) : rctx(std::addressof(c)) {}
		system_state() = default;

		auto input_layers() -> input_layer& {
			return input_layers_data;
		}
	};

	struct system {
		static auto initialize(initialize_phase& phase, system_state& s) -> void;
		static auto update(update_phase& phase, system_state& s) -> void;
		static auto begin_frame(begin_frame_phase& phase, system_state& s) -> bool;
		static auto render(render_phase& phase, const system_state& s) -> void;
		static auto end_frame(end_frame_phase& phase, system_state& s) -> void;
		static auto shutdown(shutdown_phase& phase, system_state& s) -> void;

		static auto save(system_state& s) -> void;

		static auto start(
			system_state& s,
			const input::state& input_state,
			const std::string& name,
			const std::function<void()>& contents
		) -> void;

		static auto text(
			system_state& s,
			const std::string& text
		) -> void;

		static auto button(
			system_state& s,
			const std::string& text
		) -> bool;

		static auto input(
			system_state& s,
			const std::string& name,
			std::string& buffer
		) -> void;

		template <is_arithmetic T>
		static auto slider(
			system_state& s,
			const std::string& name,
			T& value,
			T min,
			T max
		) -> void;

		template <is_quantity T, auto Unit = typename T::default_unit{}>
		static auto slider(
			system_state& s,
			const std::string& name,
			T& value,
			T min,
			T max
		) -> void;

		template <typename T, int N>
		static auto slider(
			system_state& s,
			const std::string& name,
			unitless::vec_t<T, N>& vec,
			unitless::vec_t<T, N> min,
			unitless::vec_t<T, N> max
		) -> void;

		template <typename T, int N, auto Unit = typename T::default_unit{}>
		static auto slider(
			system_state& s,
			const std::string& name,
			vec_t<T, N>& vec,
			vec_t<T, N> min,
			vec_t<T, N> max
		) -> void;

		template <is_arithmetic T>
		static auto value(
			system_state& s,
			const std::string& name,
			T value
		) -> void;

		template <is_quantity T, auto Unit = typename T::default_unit{}>
		static auto value(
			system_state& s,
			const std::string& name,
			T value
		) -> void;

		template <typename T, int N>
		static auto vec(
			system_state& s,
			const std::string& name,
			unitless::vec_t<T, N> vec
		) -> void;

		template <typename T, int N, auto Unit = typename T::default_unit{}>
		static auto vec(
			system_state& s,
			const std::string& name,
			vec_t<T, N> vec
		) -> void;

		template <typename T>
		static auto tree(
			system_state& s,
			std::span<const T> roots,
			const draw::tree_ops<T>& fns,
			draw::tree_options opt = {},
			draw::tree_selection* sel = nullptr
		) -> bool;

		static auto selectable(
			system_state& s,
			const std::string& text,
			bool selected = false
		) -> bool;

		static auto profiler(
			system_state& s
		) -> void;
	};
}

auto gse::gui::system::initialize(initialize_phase& phase, system_state& s) -> void {
	if (const auto* save_state = phase.try_state_of<save::state>()) {
		s.current_theme = static_cast<theme>(save_state->read("UI", "Theme", static_cast<int>(theme::dark)));
		s.ui_scale = save_state->read("UI", "Scale", 1.0f);
		s.font_index = save_state->read("UI", "Font", 0);
	}

	s.available_fonts = s.rctx->enumerate_resources("Fonts", ".gfont");

	if (s.available_fonts.empty()) {
		s.available_fonts.push_back("default");
	}

	if (s.font_index < 0 || s.font_index >= static_cast<int>(s.available_fonts.size())) {
		s.font_index = 0;
	}

	s.gui_font = s.rctx->get<font>("Fonts/" + s.available_fonts[s.font_index]);
	s.blank_texture = s.rctx->queue<texture>("blank", unitless::vec4(1, 1, 1, 1));
	s.menus = load(config::resource_path / s.file_path, s.menus);

	std::vector<std::pair<std::string, int>> font_options;
	for (auto [i, font] : s.available_fonts | std::views::enumerate) {
		font_options.emplace_back(font, static_cast<int>(i));
	}

	phase.channels.push(save::register_property{
		.category = "UI",
		.name = "Theme",
		.description = "UI color theme",
		.ref = reinterpret_cast<int*>(&s.current_theme),
		.type = typeid(int),
		.enum_options = {
			{"Dark", static_cast<int>(theme::dark)},
			{"Darker", static_cast<int>(theme::darker)},
			{"Light", static_cast<int>(theme::light)},
			{"High Contrast", static_cast<int>(theme::high_contrast)}
		}
	});

	phase.channels.push(save::register_property{
		.category = "UI",
		.name = "Scale",
		.description = "UI scale multiplier",
		.ref = &s.ui_scale,
		.type = typeid(float)
	});

	phase.channels.push(save::register_property{
		.category = "UI",
		.name = "Font",
		.description = "UI font",
		.ref = &s.font_index,
		.type = typeid(int),
		.enum_options = std::move(font_options)
	});

	auto calculate_group_bounds = [&s](const id root_id) -> ui_rect {
		const menu* root = s.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : s.menus.items()) {
				if (item.owner_id() == parent_id) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	const ui_rect screen_rect = usable_screen_rect(s);

	for (menu& m : s.menus.items()) {
		if (!m.owner_id().exists()) {
			if (m.docked_to != dock::location::none) {
				if (m.docked_to == dock::location::center) {
					m.rect = screen_rect;
				} else {
					m.rect = layout::dock_target_rect(screen_rect, m.docked_to, m.dock_split_ratio);
				}
			} else {
				m.rect = calculate_group_bounds(m.id());

				// Clamp size to fit within screen
				const float max_width = screen_rect.width();
				const float max_height = screen_rect.height();
				const float clamped_width = std::min(m.rect.width(), max_width);
				const float clamped_height = std::min(m.rect.height(), max_height);

				// Clamp position to keep window on screen
				float new_left = m.rect.left();
				float new_top = m.rect.top();

				if (new_left < 0.f) {
					new_left = 0.f;
				} else if (new_left + clamped_width > screen_rect.width()) {
					new_left = std::max(0.f, screen_rect.width() - clamped_width);
				}

				if (new_top > screen_rect.top()) {
					new_top = screen_rect.top();
				} else if (new_top - clamped_height < 0.f) {
					new_top = clamped_height;
				}

				m.rect = ui_rect::from_position_size(
					{ new_left, new_top },
					{ clamped_width, clamped_height }
				);
			}

			layout::update(s.menus, m.id());
		}
	}

	s.visible_menu_ids_last_frame.clear();
	s.visible_menu_ids_last_frame.reserve(s.menus.items().size());

	for (menu& m : s.menus.items()) {
		s.visible_menu_ids_last_frame.push_back(m.id());
	}

	s.previous_viewport_size = unitless::vec2(s.rctx->window().viewport());
}

auto gse::gui::system::update(update_phase& phase, system_state& s) -> void {
	for (const auto& changed : phase.read_channel<save::property_changed>()) {
		if (changed.category == "UI" && changed.name == "Font") {
			reload_font(s);
		}
	}

	const auto* input_state = phase.try_state_of<input::system_state>();
	if (!input_state) {
		return;
	}

	const auto viewport = s.rctx->window().viewport();
	const style sty = apply_scale(s, style::from_theme(s.current_theme), static_cast<float>(viewport.y()));

	const unitless::vec2 mouse_position = input_state->current_state().mouse_position();
	const bool mouse_held = input_state->current_state().mouse_button_held(mouse_button::button_1);

	match(s.current_state)
		.if_is([&](const states::idle&) {
			s.current_state = handle_idle_state(s, input_state->current_state(), mouse_position, mouse_held, sty);
		})
		.else_if_is([&](const states::dragging& st) {
			s.current_state = handle_dragging_state(s, st, s.rctx->window(), mouse_position, mouse_held);
		})
		.else_if_is([&](const states::resizing& st) {
			s.current_state = handle_resizing_state(s, st, mouse_position, mouse_held, sty);
		})
		.else_if_is([&](const states::resizing_divider& st) {
			s.current_state = handle_resizing_divider_state(s, st, mouse_position, mouse_held, sty);
		})
		.else_if_is([&](const states::pending_drag& st) {
			s.current_state = handle_pending_drag_state(s, st, mouse_position, mouse_held);
		})
		.otherwise([&] {
			s.current_state = states::idle{};
		});

	if (s.save_clock.elapsed() > system_state::update_interval) {
		gui::save(s.menus, config::resource_path / s.file_path);
		s.save_clock.reset();
	}
}

auto gse::gui::system::begin_frame(begin_frame_phase&, system_state& s) -> bool {
	const auto current_viewport_size = unitless::vec2(s.rctx->window().viewport());

	if (s.previous_viewport_size.x() > 0.f && s.previous_viewport_size.y() > 0.f) {
		if (current_viewport_size.x() <= 0.f || current_viewport_size.y() <= 0.f) {
			return true;
		}

		if (current_viewport_size.x() != s.previous_viewport_size.x() ||
		    current_viewport_size.y() != s.previous_viewport_size.y()) {

			const style old_sty = apply_scale(s, style::from_theme(s.current_theme), s.previous_viewport_size.y());
			const style new_sty = apply_scale(s, style::from_theme(s.current_theme), current_viewport_size.y());

			const float old_menu_bar_h = menu_bar::height(old_sty);
			const float new_menu_bar_h = menu_bar::height(new_sty);

			const float old_usable_height = s.previous_viewport_size.y() - old_menu_bar_h;
			const float new_usable_height = current_viewport_size.y() - new_menu_bar_h;

			const float scale_x = current_viewport_size.x() / s.previous_viewport_size.x();
			const float scale_y = new_usable_height / old_usable_height;

			const ui_rect new_screen_rect = ui_rect::from_position_size(
				{ 0.f, new_usable_height },
				{ current_viewport_size.x(), new_usable_height }
			);

			for (menu& m : s.menus.items()) {
				if (!m.owner_id().exists()) {
					if (m.docked_to != dock::location::none) {
						if (m.docked_to == dock::location::center) {
							m.rect = new_screen_rect;
						} else {
							m.rect = layout::dock_target_rect(new_screen_rect, m.docked_to, m.dock_split_ratio);
						}
					} else {
						const float ratio_x = m.rect.left() / s.previous_viewport_size.x();
						const float ratio_y = (s.previous_viewport_size.y() - m.rect.top()) / old_usable_height;

						const float new_left = ratio_x * current_viewport_size.x();
						const float new_top = current_viewport_size.y() - (ratio_y * new_usable_height);

						const float new_width = m.rect.width() * scale_x;
						const float new_height = m.rect.height() * scale_y;

						const float actual_width = std::min(new_width, current_viewport_size.x());
						const float actual_height = std::min(new_height, new_usable_height);

						const float clamped_left = std::clamp(new_left, 0.f, std::max(0.f, current_viewport_size.x() - actual_width));
						const float clamped_top = std::clamp(new_top, actual_height, new_usable_height);

						m.rect = ui_rect::from_position_size(
							{ clamped_left, clamped_top },
							{ actual_width, actual_height }
						);
					}

					layout::update(s.menus, m.id());
				}
			}

			s.previous_viewport_size = current_viewport_size;
		}
	} else {
		s.previous_viewport_size = current_viewport_size;
	}

	s.fstate = {};
	s.sprite_commands.clear();
	s.text_commands.clear();
	s.input_layers_data.begin_frame();

	const style sty = apply_scale(s, style::from_theme(s.current_theme), current_viewport_size.y());

	s.fstate = {
		.rctx = s.rctx,
		.sty = sty,
		.active = s.gui_font.valid()
	};

	s.hot_widget_id = {};

	s.input_layer_render = s.menu_bar_state.settings_open
		? render_layer::popup
		: render_layer::content;

	for (menu& m : s.menus.items()) {
		m.was_begun_this_frame = false;
		m.chrome_drawn_this_frame = false;
	}

	return true;
}

auto gse::gui::system::render(render_phase&, const system_state&) -> void {}

auto gse::gui::system::end_frame(end_frame_phase& phase, system_state& s) -> void {
	if (!s.fstate.active) {
		s.fstate = {};
		return;
	}

	const auto* input_state_ptr = phase.try_state_of<input::system_state>();
	if (!input_state_ptr) {
		s.fstate = {};
		return;
	}

	const input::state& input_st = input_state_ptr->current_state();
	const auto viewport_size = unitless::vec2(s.rctx->window().viewport());

	if (s.active_dock_space) {
		const auto [areas] = s.active_dock_space.value();
		const unitless::vec2 mouse_pos = input_st.mouse_position();

		for (const dock::area& area : areas) {
			if (area.rect.contains(mouse_pos)) {
				s.sprite_commands.push_back({
					.rect = area.target,
					.color = s.fstate.sty.color_dock_preview,
					.texture = s.blank_texture
				});
				break;
			}
		}

		for (const dock::area& area : areas) {
			s.sprite_commands.push_back({
				.rect = area.rect,
				.color = s.fstate.sty.color_dock_preview,
				.texture = s.blank_texture
			});
		}
	}

	const menu_bar::context mb_ctx{
		.font = s.gui_font,
		.blank_texture = s.blank_texture,
		.style = s.fstate.sty,
		.sprites = s.sprite_commands,
		.texts = s.text_commands
	};
	menu_bar::update(s.menu_bar_state, mb_ctx, input_st, viewport_size);

	const ui_rect settings_rect = settings_panel::default_panel_rect(s.fstate.sty, viewport_size);

	const auto* save_state = phase.try_state_of<save::state>();
	const auto* actions_state = phase.try_state_of<actions::system_state>();

	const settings_panel::context sp_ctx{
		.font = s.gui_font,
		.blank_texture = s.blank_texture,
		.style = s.fstate.sty,
		.sprites = s.sprite_commands,
		.texts = s.text_commands,
		.input = input_st,
		.publish_update = [&phase](save::update_request req) {
			phase.channels.push(std::move(req));
		},
		.request_save = [save_state] {
			if (save_state) {
				save_state->save();
			}
		},
		.tooltip = &s.tooltip,
		.input_layers = &s.input_layers_data,
		.all_bindings = [actions_state]() -> std::vector<actions::action_binding_info> {
			return actions_state ? actions_state->all_bindings() : std::vector<actions::action_binding_info>{};
		},
		.rebind = [&phase](std::string_view name, key k) {
			phase.channels.push(actions::rebind_request{
				.action_name = std::string(name),
				.new_key = k
			});
		},
		.pressed_key = [&input_st]() -> key {
			for (auto i : std::views::iota(32, 97)) {
				if (input_st.key_pressed(static_cast<key>(i))) {
					return static_cast<key>(i);
				}
			}
			for (auto i : std::views::iota(256, 349)) {
				if (input_st.key_pressed(static_cast<key>(i))) {
					return static_cast<key>(i);
				}
			}
			return key{};
		}
	};

	if (save_state && settings_panel::update(s.settings_panel_state, sp_ctx, settings_rect, s.menu_bar_state.settings_open, *save_state)) {
		s.menu_bar_state.settings_open = false;
	}

	if (s.menu_bar_state.settings_open && input_st.mouse_button_pressed(mouse_button::button_1)) {
		const unitless::vec2 mouse_pos = input_st.mouse_position();

		if (const ui_rect bar_rect = menu_bar::bar_rect(s.fstate.sty, viewport_size); !settings_rect.contains(mouse_pos) && !bar_rect.contains(mouse_pos)) {
			s.menu_bar_state.settings_open = false;
		}
	}

	if (s.tooltip.pending_widget_id.exists()) {
		if (s.tooltip.pending_widget_id == s.tooltip.widget_id) {
			s.tooltip.hover_time += system_clock::dt<time>();
		} else {
			s.tooltip.widget_id = s.tooltip.pending_widget_id;
			s.tooltip.hover_time = time{};
		}
	} else {
		s.tooltip.widget_id.reset();
		s.tooltip.hover_time = time{};
		s.tooltip.text.clear();
	}

	if (s.tooltip.widget_id.exists() && s.tooltip.hover_time >= tooltip_state::show_delay && !s.tooltip.text.empty() && s.gui_font.valid()) {
		const float padding = s.fstate.sty.padding;
		const float font_size = s.fstate.sty.font_size;
		const float text_width = s.gui_font->width(s.tooltip.text, font_size);
		const float text_height = s.gui_font->line_height(font_size);

		const float tooltip_width = text_width + padding * 2.f;
		const float tooltip_height = text_height + padding;

		unitless::vec2 tooltip_pos = s.tooltip.position + unitless::vec2(15.f, -15.f);

		if (tooltip_pos.x() + tooltip_width > viewport_size.x()) {
			tooltip_pos.x() = viewport_size.x() - tooltip_width;
		}
		if (tooltip_pos.y() - tooltip_height < 0.f) {
			tooltip_pos.y() = tooltip_height;
		}

		const ui_rect tooltip_rect = ui_rect::from_position_size(
			tooltip_pos,
			{ tooltip_width, tooltip_height }
		);

		s.sprite_commands.push_back({
			.rect = tooltip_rect,
			.color = s.fstate.sty.color_menu_body,
			.texture = s.blank_texture,
			.layer = render_layer::modal,
			.z_order = 100
		});

		s.sprite_commands.push_back({
			.rect = tooltip_rect.inset({ -1.f, -1.f }),
			.color = s.fstate.sty.color_border,
			.texture = s.blank_texture,
			.layer = render_layer::modal,
			.z_order = 99
		});

		s.text_commands.push_back({
			.font = s.gui_font,
			.text = s.tooltip.text,
			.position = {
				tooltip_rect.left() + padding,
				tooltip_rect.center().y() + font_size * 0.35f
			},
			.scale = font_size,
			.color = s.fstate.sty.color_text,
			.layer = render_layer::modal,
			.z_order = 100
		});
	}

	s.tooltip.pending_widget_id.reset();

	if (s.fstate.rctx && s.fstate.rctx->ui_focus()) {
		cursor::render_to(*s.fstate.rctx, s.sprite_commands, input_st.mouse_position());
	}

	s.visible_menu_ids_last_frame.clear();
	s.visible_menu_ids_last_frame.reserve(s.menus.items().size());
	for (menu& m : s.menus.items()) {
		m.was_visible_last_frame = m.was_begun_this_frame;
		if (m.was_begun_this_frame) {
			s.visible_menu_ids_last_frame.push_back(m.id());
		}
	}

	auto sort_by_layer = [](auto& commands) {
		std::ranges::stable_sort(commands, [](const auto& a, const auto& b) {
			if (a.layer != b.layer) {
				return static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer);
			}
			return a.z_order < b.z_order;
		});
	};

	sort_by_layer(s.sprite_commands);
	sort_by_layer(s.text_commands);

	std::vector<renderer::sprite_command> final_sprites;
	std::vector<renderer::text_command> final_texts;

	final_sprites.reserve(s.sprite_commands.size());
	final_texts.reserve(s.text_commands.size());

	for (std::uint8_t layer = 0; layer <= static_cast<std::uint8_t>(render_layer::cursor); ++layer) {
		const auto current_layer = static_cast<render_layer>(layer);

		for (auto& cmd : s.sprite_commands) {
			if (cmd.layer == current_layer) {
				final_sprites.push_back(std::move(cmd));
			}
		}

		for (auto& cmd : s.text_commands) {
			if (cmd.layer == current_layer) {
				final_texts.push_back(std::move(cmd));
			}
		}
	}

	for (auto& cmd : final_sprites) {
		phase.channels.push(std::move(cmd));
	}

	for (auto& cmd : final_texts) {
		phase.channels.push(std::move(cmd));
	}

	s.fstate = {};
}

auto gse::gui::system::shutdown(shutdown_phase&, system_state& s) -> void {
	gui::save(s.menus, config::resource_path / s.file_path);
}

auto gse::gui::system::save(system_state& s) -> void {
	gui::save(s.menus, config::resource_path / s.file_path);
}

auto gse::gui::system::start(system_state& s, const input::state& input_state, const std::string& name, const std::function<void()>& contents) -> void {
	if (!s.fstate.active) {
		return;
	}

	if (!begin_menu(s, name)) {
		return;
	}

	menu& current_menu = *s.current_menu;

	if (!current_menu.chrome_drawn_this_frame) {
		draw_menu_chrome(s, input_state, current_menu);
		current_menu.chrome_drawn_this_frame = true;
	}

	const auto it = std::ranges::find(current_menu.tab_contents, name);
	const bool is_active_tab = (it != current_menu.tab_contents.end()) &&
		(std::distance(current_menu.tab_contents.begin(), it) == static_cast<ptrdiff_t>(current_menu.active_tab_index));

	if (!is_active_tab) {
		end_menu(s);
		return;
	}

	const style& sty = s.fstate.sty;
	const ui_rect display_rect = calculate_display_rect(s, current_menu);

	const ui_rect body_rect = ui_rect::from_position_size(
		{ display_rect.left(), display_rect.top() - sty.title_bar_height },
		{ display_rect.width(), display_rect.height() - sty.title_bar_height }
	);

	s.sprite_commands.push_back({
		.rect = body_rect,
		.color = sty.color_menu_body,
		.texture = s.blank_texture
	});

	const ui_rect content_rect = body_rect.inset({ sty.padding, sty.padding });
	unitless::vec2 layout_cursor = content_rect.top_left();

	ids::scope menu_scope(current_menu.id().number());

	draw_context ctx{
		.current_menu = &current_menu,
		.style = sty,
		.input = input_state,
		.font = s.gui_font,
		.blank_texture = s.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = s.sprite_commands,
		.texts = s.text_commands,
		.input_layer = s.input_layer_render,
		.tooltip = &s.tooltip
	};

	s.hot_widget_id = {};
	s.context = &ctx;
	contents();
	s.context = nullptr;

	end_menu(s);
}

auto gse::gui::system::text(system_state& s, const std::string& text) -> void {
	if (!s.context) {
		return;
	}

	draw::text(*s.context, "", text);
}

auto gse::gui::system::button(system_state& s, const std::string& text) -> bool {
	if (!s.context) {
		return false;
	}

	return draw::button(*s.context, text, s.hot_widget_id, s.active_widget_id);
}

auto gse::gui::system::input(system_state& s, const std::string& name, std::string& buffer) -> void {
	if (!s.context) {
		return;
	}

	draw::input(*s.context, name, buffer, s.hot_widget_id, s.focus_widget_id);
}

template <gse::is_arithmetic T>
auto gse::gui::system::slider(system_state& s, const std::string& name, T& value, T min, T max) -> void {
	if (!s.context) {
		return;
	}

	draw::slider(*s.context, name, value, min, max, s.hot_widget_id, s.active_widget_id);
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::system::slider(system_state& s, const std::string& name, T& value, T min, T max) -> void {
	if (!s.context) {
		return;
	}

	draw::slider<T, Unit>(*s.context, name, value, min, max, s.hot_widget_id, s.active_widget_id);
}

template <typename T, int N>
auto gse::gui::system::slider(system_state& s, const std::string& name, unitless::vec_t<T, N>& vec, unitless::vec_t<T, N> min, unitless::vec_t<T, N> max) -> void {
	if (!s.context) {
		return;
	}

	draw::slider<T, N>(*s.context, name, vec, min, max, s.hot_widget_id, s.active_widget_id);
}

template <typename T, int N, auto Unit>
auto gse::gui::system::slider(system_state& s, const std::string& name, vec_t<T, N>& vec, vec_t<T, N> min, vec_t<T, N> max) -> void {
	if (!s.context) {
		return;
	}

	draw::slider<T, N, Unit>(*s.context, name, vec, min, max, s.hot_widget_id, s.active_widget_id);
}

template <gse::is_arithmetic T>
auto gse::gui::system::value(system_state& s, const std::string& name, T value) -> void {
	if (!s.context) {
		return;
	}

	draw::value(*s.context, name, value);
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::system::value(system_state& s, const std::string& name, T value) -> void {
	if (!s.context) {
		return;
	}

	draw::value<T, Unit>(*s.context, name, value);
}

template <typename T, int N>
auto gse::gui::system::vec(system_state& s, const std::string& name, unitless::vec_t<T, N> vec) -> void {
	if (!s.context) {
		return;
	}

	draw::vec(*s.context, name, vec);
}

template <typename T, int N, auto Unit>
auto gse::gui::system::vec(system_state& s, const std::string& name, vec_t<T, N> vec) -> void {
	if (!s.context) {
		return;
	}

	draw::vec<T, N, Unit>(*s.context, name, vec);
}

template <typename T>
auto gse::gui::system::tree(system_state& s, std::span<const T> roots, const draw::tree_ops<T>& fns, draw::tree_options opt, draw::tree_selection* sel) -> bool {
	if (!s.context) {
		return false;
	}

	return draw::tree(*s.context, roots, fns, opt, sel, s.active_widget_id);
}

auto gse::gui::system::selectable(system_state& s, const std::string& text, const bool selected) -> bool {
	if (!s.context) {
		return false;
	}

	return draw::selectable(*s.context, text, selected, s.hot_widget_id, s.active_widget_id);
}

auto gse::gui::system::profiler(system_state& s) -> void {
	trace::thread_pause pause;

	const std::span<const trace::node> roots = trace::view().roots;
	if (roots.empty()) {
		text(s, "No trace data");
		return;
	}

	if (!s.context || !s.current_menu) {
		return;
	}

	static float w_dur = 80.f;
	static float w_self = 80.f;
	static float w_frame = 60.f;

	static int resizing_col_idx = -1;

	const float pad = s.context->style.padding;
	const float font_sz = s.context->style.font_size;
	const float row_h = s.context->font->line_height(font_sz) + pad * 0.5f;
	const ui_rect menu_content = s.current_menu->rect.inset({ pad, pad });

	const bool mouse_held = s.context->input.mouse_button_held(mouse_button::button_1);
	const unitless::vec2 mouse_pos = s.context->input.mouse_position();

	if (!mouse_held) {
		resizing_col_idx = -1;
	}

	const float x_frame_right = menu_content.right();
	const float x_frame_left = x_frame_right - w_frame;

	const float x_self_right = x_frame_left - pad;
	const float x_self_left = x_self_right - w_self;

	const float x_dur_right = x_self_left - pad;
	const float x_dur_left = x_dur_right - w_dur;

	const float header_y = s.context->layout_cursor.y();
	constexpr float hit_w = 6.0f;

	auto handle_resize = [&](float& width, const float right_anchor_x, const float split_x, const int idx) {
		const ui_rect hit_rect = ui_rect::from_position_size(
			{ split_x - hit_w * 0.5f, header_y },
			{ hit_w, row_h }
		);

		const bool hovered = hit_rect.contains(mouse_pos);

		if (resizing_col_idx == idx) {
			width = std::max(20.f, right_anchor_x - mouse_pos.x());
			set_style(cursor::style::resize_ew);
		} else if (resizing_col_idx == -1 && hovered) {
			set_style(cursor::style::resize_ew);
			if (mouse_held) {
				resizing_col_idx = idx;
			}
		}
	};

	handle_resize(w_frame, x_frame_right, x_frame_left, 2);
	handle_resize(w_self, x_self_right, x_self_left, 1);
	handle_resize(w_dur, x_dur_right, x_dur_left, 0);

	const float draw_x_frame = menu_content.right() - w_frame;
	const float draw_x_self = draw_x_frame - pad - w_self;
	const float draw_x_dur = draw_x_self - pad - w_dur;
	const float total_cols_w = w_dur + w_self + w_frame + (pad * 3);

	auto draw_header_item = [&](const std::string& txt, float x, float w) {
		const ui_rect r = ui_rect::from_position_size({ x, header_y }, { w, row_h });
		s.context->queue_sprite({
			.rect = r,
			.color = s.context->style.color_title_bar,
			.texture = s.context->blank_texture
		});
		s.context->queue_text({
			.font = s.context->font,
			.text = txt,
			.position = { r.left() + pad * 0.5f, r.center().y() + font_sz * 0.5f },
			.scale = font_sz,
			.clip_rect = r
		});
	};

	draw_header_item("Duration", draw_x_dur, w_dur);
	draw_header_item("Self", draw_x_self, w_self);
	draw_header_item("% Frame", draw_x_frame, w_frame);

	const float tree_w = draw_x_dur - menu_content.left() - pad;
	draw_header_item("Node Name", menu_content.left(), std::max(0.f, tree_w));

	s.context->layout_cursor.y() -= (row_h + (row_h * 0.15f));

	static draw::tree_selection selection;
	static draw::tree_options options{
		.indent_per_level = 15.f,
		.extra_right_padding = total_cols_w,
		.toggle_on_row_click = true,
		.multi_select = false
	};
	options.extra_right_padding = total_cols_w;

	time_t<std::uint64_t> frame_start = roots[0].start;
	time_t<std::uint64_t> frame_end = roots[0].stop;

	for (const trace::node& r : roots) {
		if (r.start < frame_start) frame_start = r.start;
		if (r.stop > frame_end) frame_end = r.stop;
	}

	const double frame_ns = static_cast<double>((frame_end - frame_start).as<nanoseconds>());

	const draw::tree_ops<trace::node> ops{
		.children = [](const trace::node& n) -> std::span<const trace::node> {
			return { n.children_first, n.children_count };
		},
		.label = [](const trace::node& n) -> std::string_view {
			return tag(n.id);
		},
		.key = [](const trace::node& n) -> std::uint64_t {
			return n.id;
		},
		.is_leaf = [](const trace::node& n) -> bool {
			return n.children_count == 0;
		},
		.custom_draw = [=](const trace::node& n, const draw_context& ctx, const ui_rect& row, bool, bool, int) {
			const double dur_ns = static_cast<double>((n.stop - n.start).as<nanoseconds>());
			const double self_ns = static_cast<double>(n.self.as<nanoseconds>());
			const double pct_frame = frame_ns > 0.0 ? (dur_ns / frame_ns) * 100.0 : 0.0;

			auto to_fixed = [](const double v, char* buf, const std::size_t len, const int prec) -> std::string_view {
				auto [p, ec] = std::to_chars(buf, buf + len, v, std::chars_format::fixed, prec);
				return ec == std::errc{} ? std::string_view{ buf, static_cast<size_t>(p - buf) } : std::string_view{};
			};

			char buf[32];

			auto draw_col = [&](const std::string_view val, float x, float w) {
				const ui_rect box = ui_rect::from_position_size({ x, row.top() }, { w, row.height() });

				ctx.queue_sprite({
					.rect = box,
					.color = ctx.style.color_widget_background,
					.texture = ctx.blank_texture
				});

				ctx.queue_text({
					.font = ctx.font,
					.text = std::string(val),
					.position = { box.left() + pad * 0.5f, box.center().y() + font_sz * 0.5f },
					.scale = font_sz,
					.clip_rect = box
				});
			};

			draw_col(to_fixed(dur_ns / 1000.0, buf, 32, 1), draw_x_dur, w_dur);
			draw_col(to_fixed(self_ns / 1000.0, buf, 32, 1), draw_x_self, w_self);
			draw_col(to_fixed(pct_frame, buf, 32, 1), draw_x_frame, w_frame);
		}
	};

	ids::scope tree_scope("gui.tree.profiler");
	trace::set_finalize_paused(tree(s, roots, ops, options, &selection));
}

auto gse::gui::begin_menu(system_state& s, const std::string& name) -> bool {
	for (menu& m : s.menus.items()) {
		if (const auto it = std::ranges::find(m.tab_contents, name); it != m.tab_contents.end()) {
			s.current_menu = &m;
			s.current_menu->was_begun_this_frame = true;
			s.current_scope = std::make_unique<ids::scope>(s.current_menu->id().number());
			return true;
		}
	}

	for (menu& m : s.menus.items()) {
		if (m.id().tag() == name) {
			s.current_menu = &m;
			s.current_menu->was_begun_this_frame = true;
			s.current_scope = std::make_unique<ids::scope>(s.current_menu->id().number());
			return true;
		}
	}

	menu new_menu(
		name,
		menu_data{
			.rect = ui_rect({
				.min = { 100.f, 100.f },
				.max = { 400.f, 300.f }
			}),
			.parent_id = id()
		}
	);

	const id new_id = new_menu.id();
	s.menus.add(new_id, std::move(new_menu));

	if (menu* menu_ptr = s.menus.try_get(new_id)) {
		s.current_menu = menu_ptr;
		s.current_menu->was_begun_this_frame = true;
		s.current_scope = std::make_unique<ids::scope>(s.current_menu->id().number());
		return true;
	}

	return false;
}

auto gse::gui::end_menu(system_state& s) -> void {
	s.current_scope.reset();
	s.current_menu = nullptr;
}

auto gse::gui::usable_screen_rect(const system_state& s) -> ui_rect {
	const auto viewport_size = unitless::vec2(s.rctx->window().viewport());
	const style sty = apply_scale(s, style::from_theme(s.current_theme), viewport_size.y());
	const float usable_height = viewport_size.y() - menu_bar::height(sty);
	return ui_rect::from_position_size(
		{ 0.f, usable_height },
		{ viewport_size.x(), usable_height }
	);
}

auto gse::gui::calculate_display_rect(const system_state& s, const menu& m) -> ui_rect {
	ui_rect display_rect = m.rect;

	for (const menu& child : s.menus.items()) {
		if (child.owner_id() == m.id() && !child.was_begun_this_frame && child.was_visible_last_frame) {
			display_rect = ui_rect::bounding_box(display_rect, calculate_display_rect(s, child));
		}
	}

	return display_rect;
}

auto gse::gui::apply_scale(const system_state& s, style sty, const float viewport_height) -> style {
	constexpr float reference_height = 1080.f;
	const float base_scale = viewport_height / reference_height;
	const float final_scale = base_scale * s.ui_scale;

	sty.padding *= final_scale;
	sty.title_bar_height *= final_scale;
	sty.resize_border_thickness *= final_scale;
	sty.min_menu_size.x() *= final_scale;
	sty.min_menu_size.y() *= final_scale;
	sty.font_size *= final_scale;
	sty.menu_bar_height *= final_scale;
	return sty;
}

auto gse::gui::reload_font(system_state& s) -> void {
	if (s.font_index >= 0 && s.font_index < static_cast<int>(s.available_fonts.size())) {
		s.gui_font = s.rctx->get<font>("Fonts/" + s.available_fonts[s.font_index]);
	}
}

auto gse::gui::draw_menu_chrome(system_state& s, const input::state& input_state, menu& current_menu) -> void {
	const style& sty = s.fstate.sty;

	const ui_rect display_rect = calculate_display_rect(s, current_menu);

	const ui_rect title_bar_rect = ui_rect::from_position_size(
		display_rect.top_left(),
		{ display_rect.width(), sty.title_bar_height }
	);

	const ui_rect body_rect = ui_rect::from_position_size(
		{ display_rect.left(), display_rect.top() - sty.title_bar_height },
		{ display_rect.width(), display_rect.height() - sty.title_bar_height }
	);

	s.sprite_commands.push_back({
		.rect = body_rect,
		.color = sty.color_menu_body,
		.texture = s.blank_texture
	});

	if (current_menu.tab_contents.size() > 1) {
		draw_tab_bar(s, input_state, current_menu, title_bar_rect);
	} else {
		s.sprite_commands.push_back({
			.rect = title_bar_rect,
			.color = sty.color_title_bar,
			.texture = s.blank_texture
		});

		if (s.gui_font.valid() && !current_menu.tab_contents.empty()) {
			s.text_commands.push_back({
				.font = s.gui_font,
				.text = current_menu.tab_contents[0],
				.position = {
					title_bar_rect.left() + sty.padding,
					title_bar_rect.center().y() + sty.font_size * 0.35f
				},
				.scale = sty.font_size,
				.clip_rect = title_bar_rect
			});
		}
	}
}

auto gse::gui::draw_tab_bar(system_state& s, const input::state& input_state, menu& current_menu, const ui_rect& title_bar_rect) -> void {
	const style& sty = s.fstate.sty;
	const unitless::vec2 mouse_pos = input_state.mouse_position();
	const bool mouse_clicked = input_state.mouse_button_pressed(mouse_button::button_1);

	s.sprite_commands.push_back({
		.rect = title_bar_rect,
		.color = sty.color_title_bar,
		.texture = s.blank_texture
	});

	const std::size_t tab_count = current_menu.tab_contents.size();
	if (tab_count == 0) {
		return;
	}

	const float tab_height = sty.title_bar_height - 4.0f;
	const float tab_top = title_bar_rect.top() - 2.0f;
	const float tab_padding_h = sty.padding;
	constexpr float tab_gap = 2.0f;
	constexpr float min_tab_width = 60.0f;
	constexpr float max_tab_width = 200.0f;

	const float available_width = title_bar_rect.width() - sty.padding * 2.0f;
	const float total_gaps = tab_gap * static_cast<float>(tab_count - 1);
	const float width_per_tab = (available_width - total_gaps) / static_cast<float>(tab_count);
	const float tab_width = std::clamp(width_per_tab, min_tab_width, max_tab_width);

	float tab_x = title_bar_rect.left() + sty.padding;

	auto truncate_text = [&s, &sty](const std::string& text, const float max_width) -> std::string {
		if (!s.gui_font.valid()) {
			return text;
		}

		if (const float text_width = s.gui_font->width(text, sty.font_size); text_width <= max_width) {
			return text;
		}

		const std::string ellipsis = "...";
		const float ellipsis_width = s.gui_font->width(ellipsis, sty.font_size);
		const float target_width = max_width - ellipsis_width;

		if (target_width <= 0) {
			return ellipsis;
		}

		std::string truncated;
		for (const char c : text) {
			const std::string test = truncated + c;
			if (s.gui_font->width(test, sty.font_size) > target_width) {
				break;
			}
			truncated += c;
		}

		return truncated + ellipsis;
	};

	for (std::size_t i = 0; i < tab_count; ++i) {
		const std::string& tab_name = current_menu.tab_contents[i];
		const bool is_active = (i == current_menu.active_tab_index);

		const ui_rect tab_rect = ui_rect::from_position_size(
			{ tab_x, tab_top },
			{ tab_width, tab_height }
		);

		const bool is_hovered = tab_rect.contains(mouse_pos);

		if (is_hovered && mouse_clicked && !is_active) {
			current_menu.active_tab_index = static_cast<uint32_t>(i);
		}

		unitless::vec4 tab_color;
		if (is_active) {
			tab_color = sty.color_menu_body;
		} else if (is_hovered) {
			tab_color = unitless::vec4(
				sty.color_title_bar.x() * 1.2f,
				sty.color_title_bar.y() * 1.2f,
				sty.color_title_bar.z() * 1.2f,
				sty.color_title_bar.w()
			);
		} else {
			tab_color = sty.color_title_bar_inactive;
		}

		s.sprite_commands.push_back({
			.rect = tab_rect,
			.color = tab_color,
			.texture = s.blank_texture
		});

		if (is_active) {
			const ui_rect connector = ui_rect::from_position_size(
				{ tab_rect.left(), title_bar_rect.bottom() },
				{ tab_width, 2.0f }
			);
			s.sprite_commands.push_back({
				.rect = connector,
				.color = sty.color_menu_body,
				.texture = s.blank_texture
			});
		}

		if (s.gui_font.valid()) {
			const float text_max_width = tab_width - tab_padding_h * 2.0f;
			const std::string display_text = truncate_text(tab_name, text_max_width);

			s.text_commands.push_back({
				.font = s.gui_font,
				.text = display_text,
				.position = {
					tab_rect.left() + tab_padding_h,
					tab_rect.center().y() + sty.font_size * 0.35f
				},
				.scale = sty.font_size,
				.clip_rect = tab_rect
			});
		}

		tab_x += tab_width + tab_gap;
	}
}

auto gse::gui::handle_idle_state(system_state& s, const input::state& input_state, unitless::vec2 mouse_position, const bool mouse_held, const style& style) -> gui::state {
	struct interaction_candidate {
		std::variant<states::resizing, states::dragging, states::resizing_divider, states::pending_drag> future_state;
		cursor::style cursor;
	};

	struct resize_rule {
		std::function<bool(const ui_rect&, const unitless::vec2&)> condition;
		resize_handle handle;
		cursor::style cursor;
	};

	const std::array<resize_rule, 8> resize_rules = {{
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.left()) < t;
		}, resize_handle::top_left, cursor::style::resize_nw },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.right()) < t;
		}, resize_handle::top_right, cursor::style::resize_ne },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.left()) < t;
		}, resize_handle::bottom_left, cursor::style::resize_sw },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.right()) < t;
		}, resize_handle::bottom_right, cursor::style::resize_se },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.x() - r.left()) < t;
		}, resize_handle::left, cursor::style::resize_w },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.x() - r.right()) < t;
		}, resize_handle::right, cursor::style::resize_e },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.top()) < t;
		}, resize_handle::top, cursor::style::resize_n },
		{ [style](const ui_rect& r, const unitless::vec2& p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t;
		}, resize_handle::bottom, cursor::style::resize_s },
	}};

	auto calculate_group_bounds = [&s](const id root_id) -> ui_rect {
		const menu* root = s.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : s.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	std::vector<menu*> visible_menus;
	visible_menus.reserve(s.visible_menu_ids_last_frame.size());
	for (const id& mid : s.visible_menu_ids_last_frame) {
		if (menu* m = s.menus.try_get(mid)) {
			visible_menus.push_back(m);
		}
	}

	auto hot_item = [&]() -> std::optional<interaction_candidate> {
		for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
			menu& current_menu = **it;

			if (!current_menu.owner_id().exists()) {
				if (current_menu.docked_to == dock::location::none) {
					const ui_rect group_rect = calculate_group_bounds(current_menu.id());

					for (const auto& [condition, handle, cursor] : resize_rules) {
						if (condition(group_rect, mouse_position)) {
							return interaction_candidate{
								.future_state = states::resizing{
									.menu_id = current_menu.id(),
									.handle = handle
								},
								.cursor = cursor
							};
						}
					}
				} else {
					const ui_rect& rect = current_menu.rect;

					switch (current_menu.docked_to) {
					case dock::location::left:
						if (std::abs(mouse_position.x() - rect.right()) < style.resize_border_thickness) {
							return interaction_candidate{
								states::resizing{ current_menu.id(), resize_handle::right },
								cursor::style::resize_e
							};
						}
						break;
					case dock::location::right:
						if (std::abs(mouse_position.x() - rect.left()) < style.resize_border_thickness) {
							return interaction_candidate{
								states::resizing{ current_menu.id(), resize_handle::left },
								cursor::style::resize_w
							};
						}
						break;
					case dock::location::top:
						if (std::abs(mouse_position.y() - rect.bottom()) < style.resize_border_thickness) {
							return interaction_candidate{
								states::resizing{ current_menu.id(), resize_handle::bottom },
								cursor::style::resize_s
							};
						}
						break;
					case dock::location::bottom:
						if (std::abs(mouse_position.y() - rect.top()) < style.resize_border_thickness) {
							return interaction_candidate{
								states::resizing{ current_menu.id(), resize_handle::top },
								cursor::style::resize_n
							};
						}
						break;
					default:
						break;
					}
				}
			} else {
				if (const menu* parent = s.menus.try_get(current_menu.owner_id())) {
					bool hovering = false;
					auto new_cursor = cursor::style::arrow;
					const ui_rect& r = current_menu.rect;

					switch (current_menu.docked_to) {
					case dock::location::left:
						if (std::abs(mouse_position.x() - r.right()) < style.resize_border_thickness &&
							mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
							hovering = true;
							new_cursor = cursor::style::resize_e;
						}
						break;
					case dock::location::right:
						if (std::abs(mouse_position.x() - r.left()) < style.resize_border_thickness &&
							mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
							hovering = true;
							new_cursor = cursor::style::resize_w;
						}
						break;
					case dock::location::top:
						if (std::abs(mouse_position.y() - r.bottom()) < style.resize_border_thickness &&
							mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
							hovering = true;
							new_cursor = cursor::style::resize_s;
						}
						break;
					case dock::location::bottom:
						if (std::abs(mouse_position.y() - r.top()) < style.resize_border_thickness &&
							mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
							hovering = true;
							new_cursor = cursor::style::resize_n;
						}
						break;
					default:
						break;
					}

					if (hovering) {
						return interaction_candidate{
							.future_state = states::resizing_divider{
								.parent_id = parent->id(),
								.child_id = current_menu.id()
							},
							.cursor = new_cursor
						};
					}
				}
			}

			const ui_rect title_bar_rect = ui_rect::from_position_size(
				{ current_menu.rect.left(), current_menu.rect.top() },
				{ current_menu.rect.width(), style.title_bar_height }
			);

			if (title_bar_rect.contains(mouse_position)) {
				std::optional<std::uint32_t> clicked_tab;

				if (current_menu.tab_contents.size() > 1) {
					const float tab_height = style.title_bar_height - 4.0f;
					const float tab_top = title_bar_rect.top() - 2.0f;
					constexpr float tab_gap = 2.0f;
					constexpr float min_tab_width = 60.0f;
					constexpr float max_tab_width = 200.0f;

					const std::size_t tab_count = current_menu.tab_contents.size();
					const float available_width = title_bar_rect.width() - style.padding * 2.0f;
					const float total_gaps = tab_gap * static_cast<float>(tab_count - 1);
					const float width_per_tab = (available_width - total_gaps) / static_cast<float>(tab_count);
					const float tab_width = std::clamp(width_per_tab, min_tab_width, max_tab_width);

					float tab_x = title_bar_rect.left() + style.padding;

					for (std::size_t i = 0; i < tab_count; ++i) {
						const ui_rect tab_rect = ui_rect::from_position_size(
							{ tab_x, tab_top },
							{ tab_width, tab_height }
						);

						if (tab_rect.contains(mouse_position)) {
							clicked_tab = static_cast<std::uint32_t>(i);
							break;
						}

						tab_x += tab_width + tab_gap;
					}
				}

				return interaction_candidate{
					.future_state = states::pending_drag{
						.menu_id = current_menu.id(),
						.start_position = mouse_position,
						.offset = current_menu.rect.top_left() - mouse_position,
						.tab_index = clicked_tab
					},
					.cursor = cursor::style::arrow
				};
			}
		}

		return std::nullopt;
	}();

	if (hot_item) {
		set_style(hot_item->cursor);

		if (mouse_held) {
			if (std::holds_alternative<states::dragging>(hot_item->future_state)) {
				const auto& [menu_id, offset] = std::get<states::dragging>(hot_item->future_state);
				if (const menu* m = s.menus.try_get(menu_id); m && m->docked_to != dock::location::none) {
					layout::undock(s.menus, m->id());
				}
			}

			return std::visit([](auto&& arg) -> gui::state { return arg; }, hot_item->future_state);
		}
	} else {
		set_style(cursor::style::arrow);
	}

	return states::idle{};
}

auto gse::gui::handle_dragging_state(system_state& s, const states::dragging& current, const window& window, const unitless::vec2 mouse_position, const bool mouse_held) -> gui::state {
	menu* m = s.menus.try_get(current.menu_id);
	if (!m) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	std::vector<menu*> visible_menus;
	visible_menus.reserve(s.visible_menu_ids_last_frame.size());
	for (const id& mid : s.visible_menu_ids_last_frame) {
		if (menu* vm = s.menus.try_get(mid)) {
			visible_menus.push_back(vm);
		}
	}

	if (!mouse_held) {
		if (s.active_dock_space) {
			id potential_dock_parent_id;

			for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
				if (const menu& other_menu = **it; other_menu.id() != current.menu_id && other_menu.rect.contains(mouse_position)) {
					potential_dock_parent_id = other_menu.id();
					break;
				}
			}

			for (const dock::area& area : s.active_dock_space->areas) {
				if (area.rect.contains(mouse_position)) {
					if (potential_dock_parent_id.exists()) {
						if (area.dock_location == dock::location::center) {
							if (menu* parent = s.menus.try_get(potential_dock_parent_id)) {
								parent->tab_contents.insert(
									parent->tab_contents.end(),
									std::make_move_iterator(m->tab_contents.begin()),
									std::make_move_iterator(m->tab_contents.end())
								);
								m->tab_contents.clear();
								parent->active_tab_index = static_cast<uint32_t>(parent->tab_contents.size() - 1);
								s.menus.remove(current.menu_id);
							}
						} else {
							layout::dock(s.menus, current.menu_id, potential_dock_parent_id, area.dock_location);
							layout::update(s.menus, m->id());
						}
					} else {
						const ui_rect screen_rect = usable_screen_rect(s);

						if (area.dock_location == dock::location::center) {
							m->rect = screen_rect;
							m->docked_to = dock::location::center;
							m->swap_parent(id());
							layout::update(s.menus, m->id());
						} else {
							m->rect = layout::dock_target_rect(screen_rect, area.dock_location, 0.5f);
							m->docked_to = area.dock_location;
							m->swap_parent(id());
							layout::update(s.menus, m->id());
						}
					}

					break;
				}
			}
		}

		s.active_dock_space.reset();
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	set_style(cursor::style::omni_move);

	const ui_rect screen_rect = usable_screen_rect(s);
	const unitless::vec2 old_top_left = m->rect.top_left();
	unitless::vec2 new_top_left = mouse_position + current.offset;

	const float max_x = std::max(0.f, screen_rect.width() - m->rect.width());
	new_top_left.x() = std::clamp(new_top_left.x(), 0.f, max_x);

	const float min_y = std::min(m->rect.height(), screen_rect.top());
	new_top_left.y() = std::clamp(new_top_left.y(), min_y, screen_rect.top());

	if (const unitless::vec2 delta = new_top_left - old_top_left; delta.x() != 0 || delta.y() != 0) {
		std::function<void(id)> move_group = [&](const id current_id) {
			if (menu* item = s.menus.try_get(current_id)) {
				item->rect = ui_rect::from_position_size(item->rect.top_left() + delta, item->rect.size());

				for (menu& potential_child : s.menus.items()) {
					if (potential_child.owner_id() == current_id) {
						move_group(potential_child.id());
					}
				}
			}
		};

		move_group(current.menu_id);
	}

	s.active_dock_space.reset();
	bool found_parent_menu = false;

	for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
		menu& other_menu = **it;

		if (other_menu.id() == current.menu_id) {
			continue;
		}

		if (other_menu.rect.contains(mouse_position)) {
			s.active_dock_space = layout::dock_space(other_menu.rect);
			found_parent_menu = true;
			break;
		}
	}

	if (!found_parent_menu) {
		s.active_dock_space = layout::dock_space(screen_rect);
	}

	return current;
}

auto gse::gui::handle_resizing_state(system_state& s, const states::resizing& current, const unitless::vec2 mouse_position, const bool mouse_held, const style& style) -> gui::state {
	if (!mouse_held) {
		s.active_dock_space.reset();
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	menu* m = s.menus.try_get(current.menu_id);
	if (!m) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	auto handle_to_cursor = [](const resize_handle h) -> cursor::style {
		switch (h) {
			case resize_handle::top_left:     return cursor::style::resize_nw;
			case resize_handle::top_right:    return cursor::style::resize_ne;
			case resize_handle::bottom_left:  return cursor::style::resize_sw;
			case resize_handle::bottom_right: return cursor::style::resize_se;
			case resize_handle::left:         return cursor::style::resize_w;
			case resize_handle::right:        return cursor::style::resize_e;
			case resize_handle::top:          return cursor::style::resize_n;
			case resize_handle::bottom:       return cursor::style::resize_s;
			default:                          return cursor::style::arrow;
		}
	};

	set_style(handle_to_cursor(current.handle));

	auto calculate_group_bounds = [&s](const id root_id) -> ui_rect {
		const menu* root = s.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : s.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	auto calculate_min_required_size = [&s, &style](const id root_id) -> unitless::vec2 {
		std::function<unitless::vec2(id)> rec = [&](const id node_id) -> unitless::vec2 {
			unitless::vec2 req = style.min_menu_size;

			for (const menu& child : s.menus.items()) {
				if (child.owner_id() != node_id || !child.was_visible_last_frame) {
					continue;
				}

				const unitless::vec2 c = rec(child.id());

				switch (child.docked_to) {
					case dock::location::left:
					case dock::location::right:
						req.x() += c.x();
						req.y() = std::max(req.y(), c.y());
						break;
					case dock::location::top:
					case dock::location::bottom:
						req.y() += c.y();
						req.x() = std::max(req.x(), c.x());
						break;
					default:
						req.x() = std::max(req.x(), c.x());
						req.y() = std::max(req.y(), c.y());
						break;
				}
			}

			return req;
		};

		return rec(root_id);
	};

	const ui_rect group_rect = calculate_group_bounds(m->id());
	unitless::vec2 min_corner = group_rect.min();
	unitless::vec2 max_corner = group_rect.max();

	const unitless::vec2 subtree_min = calculate_min_required_size(m->id());
	const float min_w = subtree_min.x();
	const float min_h = subtree_min.y();

	float opposing_left = 0.f;
	float opposing_right = std::numeric_limits<float>::max();
	float opposing_top = std::numeric_limits<float>::max();
	float opposing_bottom = 0.f;

	constexpr float dock_gap = 8.f;

	if (m->docked_to != dock::location::none) {
		for (const menu& other : s.menus.items()) {
			if (other.id() == m->id()) continue;
			if (other.owner_id() != m->owner_id()) continue;
			if (other.docked_to == dock::location::none) continue;
			if (!other.was_visible_last_frame) continue;

			const ui_rect other_bounds = calculate_group_bounds(other.id());

			switch (other.docked_to) {
				case dock::location::left:
					opposing_left = std::max(opposing_left, other_bounds.right() + dock_gap);
					break;
				case dock::location::right:
					opposing_right = std::min(opposing_right, other_bounds.left() - dock_gap);
					break;
				case dock::location::top:
					opposing_top = std::min(opposing_top, other_bounds.bottom() - dock_gap);
					break;
				case dock::location::bottom:
					opposing_bottom = std::max(opposing_bottom, other_bounds.top() + dock_gap);
					break;
				default:
					break;
			}
		}
	}

	auto clamp_left = [&](const float x) -> float {
		float result = std::min(x, max_corner.x() - min_w);
		result = std::max(result, opposing_left);
		return result;
	};

	auto clamp_right = [&](const float x) -> float {
		float result = std::max(x, min_corner.x() + min_w);
		result = std::min(result, opposing_right);
		return result;
	};

	auto clamp_bottom = [&](const float y) -> float {
		float result = std::min(y, max_corner.y() - min_h);
		result = std::max(result, opposing_bottom);
		return result;
	};

	auto clamp_top = [&](const float y) -> float {
		float result = std::max(y, min_corner.y() + min_h);
		result = std::min(result, opposing_top);
		return result;
	};

	switch (current.handle) {
		case resize_handle::left:
			min_corner.x() = clamp_left(mouse_position.x());
			break;
		case resize_handle::right:
			max_corner.x() = clamp_right(mouse_position.x());
			break;
		case resize_handle::bottom:
			min_corner.y() = clamp_bottom(mouse_position.y());
			break;
		case resize_handle::top:
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		case resize_handle::bottom_left:
			min_corner.x() = clamp_left(mouse_position.x());
			min_corner.y() = clamp_bottom(mouse_position.y());
			break;
		case resize_handle::bottom_right:
			min_corner.y() = clamp_bottom(mouse_position.y());
			max_corner.x() = clamp_right(mouse_position.x());
			break;
		case resize_handle::top_left:
			min_corner.x() = clamp_left(mouse_position.x());
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		case resize_handle::top_right:
			max_corner.x() = clamp_right(mouse_position.x());
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		default:
			break;
	}

	m->rect = ui_rect({ .min = min_corner, .max = max_corner });

	if (!m->owner_id().exists()) {
		const ui_rect screen_rect = usable_screen_rect(s);

		switch (m->docked_to) {
			case dock::location::left:
			case dock::location::right: {
				const float denom = screen_rect.width();
				const float ratio = denom > 0.f ? (m->rect.width() / denom) : 1.f;
				const float min_ratio = denom > 0.f ? std::min(1.f, min_w / denom) : 0.f;
				m->dock_split_ratio = std::clamp(ratio, min_ratio, 1.f);
				break;
			}
			case dock::location::top:
			case dock::location::bottom: {
				const float denom = screen_rect.height();
				const float ratio = denom > 0.f ? (m->rect.height() / denom) : 1.f;
				const float min_ratio = denom > 0.f ? std::min(1.f, min_h / denom) : 0.f;
				m->dock_split_ratio = std::clamp(ratio, min_ratio, 1.f);
				break;
			}
			default:
				break;
		}
	}

	layout::update(s.menus, m->id());

	return current;
}

auto gse::gui::handle_resizing_divider_state(system_state& s, const states::resizing_divider& current, const unitless::vec2 mouse_position, const bool mouse_held, const style& style) -> gui::state {
	menu* parent = s.menus.try_get(current.parent_id);
	menu* child = s.menus.try_get(current.child_id);

	if (!parent || !child) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	if (!mouse_held) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	const dock::location location = child->docked_to;

	auto location_to_cursor = [](const dock::location loc) -> cursor::style {
		switch (loc) {
			case dock::location::left:   return cursor::style::resize_e;
			case dock::location::right:  return cursor::style::resize_w;
			case dock::location::top:    return cursor::style::resize_s;
			case dock::location::bottom: return cursor::style::resize_n;
			default:                     return cursor::style::arrow;
		}
	};

	set_style(location_to_cursor(location));

	const ui_rect combined_rect = ui_rect::bounding_box(parent->rect, child->rect);

	switch (location) {
	case dock::location::left:
	case dock::location::right: {
		if (combined_rect.width() < style.min_menu_size.x() * 2.f) {
			return current;
		}

		const float min_clamp = combined_rect.left() + style.min_menu_size.x();
		const float max_clamp = combined_rect.right() - style.min_menu_size.x();
		const float divider_x = std::clamp(mouse_position.x(), min_clamp, max_clamp);

		if (location == dock::location::left) {
			child->rect = ui_rect({
				.min = { combined_rect.left(), combined_rect.bottom() },
				.max = { divider_x, combined_rect.top() }
			});
			parent->rect = ui_rect({
				.min = { divider_x, combined_rect.bottom() },
				.max = { combined_rect.right(), combined_rect.top() }
			});
		} else {
			parent->rect = ui_rect({
				.min = { combined_rect.left(), combined_rect.bottom() },
				.max = { divider_x, combined_rect.top() }
			});
			child->rect = ui_rect({
				.min = { divider_x, combined_rect.bottom() },
				.max = { combined_rect.right(), combined_rect.top() }
			});
		}

		if (combined_rect.width() > 0.f) {
			const float child_width = (location == dock::location::left)
				? (divider_x - combined_rect.left())
				: (combined_rect.right() - divider_x);
			child->dock_split_ratio = child_width / combined_rect.width();
		}
		break;
	}
	case dock::location::top:
	case dock::location::bottom: {
		if (combined_rect.height() < style.min_menu_size.y() * 2.f) {
			return current;
		}

		const float min_clamp = combined_rect.bottom() + style.min_menu_size.y();
		const float max_clamp = combined_rect.top() - style.min_menu_size.y();
		const float divider_y = std::clamp(mouse_position.y(), min_clamp, max_clamp);

		if (location == dock::location::top) {
			child->rect = ui_rect({
				.min = { combined_rect.left(), divider_y },
				.max = { combined_rect.right(), combined_rect.top() }
			});
			parent->rect = ui_rect({
				.min = { combined_rect.left(), combined_rect.bottom() },
				.max = { combined_rect.right(), divider_y }
			});
		} else {
			parent->rect = ui_rect({
				.min = { combined_rect.left(), divider_y },
				.max = { combined_rect.right(), combined_rect.top() }
			});
			child->rect = ui_rect({
				.min = { combined_rect.left(), combined_rect.bottom() },
				.max = { combined_rect.right(), divider_y }
			});
		}

		if (combined_rect.height() > 0.f) {
			const float child_height = (location == dock::location::top)
				? (combined_rect.top() - divider_y)
				: (divider_y - combined_rect.bottom());
			child->dock_split_ratio = child_height / combined_rect.height();
		}
		break;
	}
	default:
		break;
	}

	layout::update(s.menus, child->id());

	return current;
}

auto gse::gui::handle_pending_drag_state(system_state& s, const states::pending_drag& current, const unitless::vec2 mouse_position, const bool mouse_held) -> gui::state {
	if (!mouse_held) {
		return states::idle{};
	}

	const float distance = magnitude(mouse_position - current.start_position);

	if (constexpr float drag_threshold = 5.0f; distance > drag_threshold) {
		menu* m = s.menus.try_get(current.menu_id);
		if (!m) {
			return states::idle{};
		}

		id drag_menu_id = current.menu_id;
		unitless::vec2 drag_offset = current.offset;

		if (current.tab_index.has_value() && m->tab_contents.size() > 1) {
			if (const std::uint32_t tab_idx = current.tab_index.value(); tab_idx < m->tab_contents.size()) {
				std::string tab_name = m->tab_contents[tab_idx];

				m->tab_contents.erase(m->tab_contents.begin() + tab_idx);

				if (m->active_tab_index >= m->tab_contents.size()) {
					m->active_tab_index = static_cast<std::uint32_t>(m->tab_contents.size() - 1);
				}
				else if (m->active_tab_index > tab_idx) {
					m->active_tab_index--;
				}

				constexpr unitless::vec2 default_size = { 300.f, 200.f };

				const style sty = s.fstate.sty;
				const unitless::vec2 new_top_left = {
					mouse_position.x() - default_size.x() * 0.5f,
					mouse_position.y() + sty.title_bar_height * 0.5f
				};

				menu new_menu(
					tab_name,
					menu_data{
						.rect = ui_rect::from_position_size(new_top_left, default_size),
						.parent_id = id()
					}
				);

				const id new_id = new_menu.id();
				s.menus.add(new_id, std::move(new_menu));

				drag_menu_id = new_id;
				drag_offset = { -default_size.x() * 0.5f, sty.title_bar_height * 0.5f };
			}
		}
		else if (m->docked_to != dock::location::none) {
			layout::undock(s.menus, m->id());
		}

		set_style(cursor::style::omni_move);
		return states::dragging{
			.menu_id = drag_menu_id,
			.offset = drag_offset
		};
	}

	return current;
}
