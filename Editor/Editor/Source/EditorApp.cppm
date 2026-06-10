export module ide:editor_app;

import std;
import gse;

import :workspace;
import :syntax_producer;
import :lsp_client;
import :command_registry;
import :commands;
import :config_system;

namespace ide {
	constexpr std::string_view explorer_panel_name = "Explorer";

	class editor_screen : public gse::gui::screen {
	public:
		explicit editor_screen(
			gse::channel_writer channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto dismissable() const -> bool override;

		auto captures_input() const -> bool override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		auto chrome_button(
			gse::gui::builder& ui,
			const gse::gui::ui_rect& rect,
			const std::string& key,
			std::string_view glyph,
			gse::vec4f hover_color
		) -> bool;

		gse::channel_writer m_channels;
	};

	auto draw_explorer_panel(
		gse::gui::builder& ui,
		workspace::data& ws
	) -> void;

	auto draw_document_panel(
		gse::gui::builder& ui,
		workspace::data& ws,
		std::uint32_t document_id,
		gse::shared_view<config_system> config
	) -> void;
}

ide::editor_screen::editor_screen(gse::channel_writer channels)
	: m_channels(std::move(channels)) {
}

auto ide::editor_screen::title() const -> std::string_view {
	return "GSEditor";
}

auto ide::editor_screen::dismissable() const -> bool {
	return false;
}

auto ide::editor_screen::captures_input() const -> bool {
	return false;
}

auto ide::editor_screen::draw_backdrop(gse::gui::draw_context&, gse::vec2f) const -> void {
}

auto ide::editor_screen::chrome_button(gse::gui::builder& ui, const gse::gui::ui_rect& rect, const std::string& key, const std::string_view glyph, const gse::vec4f hover_color) -> bool {
	const auto& ctx = ui.ctx;
	const gse::id widget_id = gse::gui::ids::make(key);

	const bool hovered = rect.contains(ctx.input.mouse_position()) && ctx.input_available();
	const bool released = ctx.input.mouse_button_released(gse::mouse_button::button_1);

	gse::gui::interaction::mark_hot(ui.hot_widget_id, widget_id, hovered);
	gse::gui::interaction::grab_active(ui.active_widget_id, widget_id, ctx.mouse_pressed_for(rect));

	const bool engaged = ui.active_widget_id == widget_id || ui.hot_widget_id == widget_id;

	ctx.queue_sprite({
		.rect = rect,
		.color = engaged ? hover_color : ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	const std::string label(glyph);
	const float glyph_w = ctx.font->width(label, ctx.style.font_size);
	ctx.queue_text({
		.font = ctx.font,
		.text = label,
		.position = { rect.center().x() - glyph_w * 0.5f, rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = rect,
	});

	return gse::gui::interaction::release_active(ui.active_widget_id, widget_id, released) && hovered;
}

auto ide::editor_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	const auto& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}

	const gse::gui::ui_rect screen_rect = ctx.current_menu->rect;
	const float bar_height = ctx.style.title_bar_height;

	const gse::gui::ui_rect bar_rect = gse::gui::ui_rect::from_position_size(
		{ screen_rect.left(), screen_rect.top() },
		{ screen_rect.width(), bar_height }
	);

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(title()),
		.position = { bar_rect.left() + ctx.style.padding, bar_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = bar_rect,
	});

	const float button_w = bar_height * 1.5f;
	auto button_slot = [&](const int from_right) -> gse::gui::ui_rect {
		return gse::gui::ui_rect::from_position_size(
			{ bar_rect.right() - button_w * static_cast<float>(from_right + 1), bar_rect.top() },
			{ button_w, bar_height }
		);
	};

	if (chrome_button(ui, button_slot(0), "##chrome_close", "X", gse::vec4f{ 0.78f, 0.22f, 0.22f, 1.f })) {
		gse::shutdown();
	}
	if (chrome_button(ui, button_slot(1), "##chrome_max", "[ ]", ctx.style.color_widget_hovered)) {
		m_channels.push<gse::window_toggle_maximize_request>({});
	}
	if (chrome_button(ui, button_slot(2), "##chrome_min", "_", ctx.style.color_widget_hovered)) {
		m_channels.push<gse::window_minimize_request>({});
	}

	m_channels.push<gse::window_chrome_metrics_request>({
		.caption_height = static_cast<int>(bar_height),
		.controls_width = static_cast<int>(button_w * 3.f),
	});
}

auto ide::draw_explorer_panel(gse::gui::builder& ui, workspace::data& ws) -> void {
	if (!ws.fs_root.loaded) {
		workspace::load_children(ws.fs_root);
	}

	const gse::gui::draw::tree_ops<fs_node> ops{
		.children = [](const fs_node& n) -> std::span<const fs_node> {
			if (n.is_dir && !n.loaded) {
				workspace::load_children(n);
			}
			return n.children;
		},
		.label = [](const fs_node& n) -> std::string_view {
			return n.name;
		},
		.key = [](const fs_node& n) -> std::uint64_t {
			return n.key;
		},
		.is_leaf = [](const fs_node& n) -> bool {
			return !n.is_dir;
		},
	};

	ui.scroll_region({ .id = "explorer_tree" }, [&](gse::gui::builder& b) {
		b.draw<gse::gui::tree<fs_node>>({
			.roots = ws.fs_root.children,
			.ops = ops,
			.selection = &ws.explorer_selection,
		});
	});

	for (const std::uint64_t key : ws.explorer_selection.keys) {
		if (key != ws.last_opened_key) {
			if (const fs_node* node = workspace::find_node(ws.fs_root, key); node && !node->is_dir) {
				workspace::open_file(ws, node->path);
				ws.last_opened_key = key;
			}
		}
	}
}

auto ide::draw_document_panel(gse::gui::builder& ui, workspace::data& ws, const std::uint32_t document_id, const gse::shared_view<config_system> config) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}
	const gse::gui::ui_rect body = ctx.clip_stack.back();

	const auto it = ws.documents.find(document_id);
	if (it == ws.documents.end()) {
		return;
	}
	document& doc = it->second;

	const gse::id text_id = gse::gui::ids::make("##doc_text_" + std::to_string(document_id));
	const bool edited = gse::gui::draw::text_area_in_rect(
		ctx,
		text_id,
		doc.buffer,
		doc.view,
		{},
		body,
		false,
		config.show_line_numbers,
		config.caret_blink,
		ui.hot_widget_id,
		ui.focus_widget_id
	);
	if (edited) {
		doc.dirty = true;
	}

	if (ui.focus_widget_id == text_id) {
		ws.active_document_id = document_id;
		const bool ctrl = ctx.input.key_held(gse::key::left_control) || ctx.input.key_held(gse::key::right_control);
		if (ctrl && ctx.input.key_pressed(gse::key::s)) {
			workspace::save_document(ws, document_id);
		}
	}
}

export namespace ide {
	struct editor_app {
		struct data {
			workspace::data ws;
			syntax_producer::data syntax;
			lsp::client::data lsp;
			command_registry commands;
			int boot_frames = 0;
			bool initialized = false;
		};

		static auto run(
			gse::context& ctx,
			data& d,
			gse::shared_view<config_system> config_d
		) -> gse::async::task<>;
	};
}

auto ide::editor_app::run(gse::context& ctx, data& d, const gse::shared_view<config_system> config_d) -> gse::async::task<> {
	if (!d.initialized) {
		ide::discover_commands<^^ide::commands>(d.commands);

		std::error_code ec;
		d.ws.root = workspace::find_repo_root(std::filesystem::current_path(ec));
		d.ws.fs_root.path = d.ws.root;
		d.ws.fs_root.name = d.ws.root.filename().string();
		d.ws.fs_root.is_dir = true;
		workspace::open_scratch(d.ws);

		ctx.channels.push<gse::gui::push_screen_request>({
			.factory = [channels = ctx.channels] {
				return std::make_unique<editor_screen>(channels);
			},
		});

		d.initialized = true;
	}
		workspace::data* ws = &d.ws;
		const auto config = config_d;

		ctx.channels.push<gse::gui::menu_content>({
			.menu = std::string(explorer_panel_name),
			.layer = gse::render_layer::content,
			.build = [ws](gse::gui::builder& b) {
				draw_explorer_panel(b, *ws);
			},
		});

		std::string host_name;
		if (const auto it = d.ws.documents.find(d.ws.primary_document_id); it != d.ws.documents.end()) {
			host_name = it->second.tab_name;
		}

		for (const auto& [document_id, doc] : d.ws.documents) {
			const std::uint32_t id = document_id;
			ctx.channels.push<gse::gui::menu_content>({
				.menu = doc.tab_name,
				.layer = gse::render_layer::content,
				.build = [ws, id, config](gse::gui::builder& b) {
					draw_document_panel(b, *ws, id, config);
				},
			});
		}

		if (d.boot_frames < 120 && !host_name.empty()) {
			++d.boot_frames;

			ctx.channels.push<gse::settings::change_request<gse::gui::system>>({
				.apply = [primary_name = host_name](gse::gui::system::data& s) {
					const gse::id primary_id = gse::find_or_generate_id(primary_name);
					const gse::id explorer_id = gse::find_or_generate_id(std::string(explorer_panel_name));
					gse::gui::menu* primary = s.menus.try_get(primary_id);
					gse::gui::menu* explorer = s.menus.try_get(explorer_id);
					if (!primary || !explorer || primary == explorer) {
						return;
					}
					if (explorer->docked_to != gse::gui::dock::location::none) {
						return;
					}

					const gse::vec2f vp = s.previous_viewport_size;
					const float inset = s.reserve_top_bar ? s.fstate.sty.title_bar_height : 0.f;
					const gse::gui::ui_rect screen = gse::gui::ui_rect::from_position_size(
						{ 0.f, vp.y() - inset },
						{ vp.x(), vp.y() - inset }
					);

					primary->rect = screen;
					primary->docked_to = gse::gui::dock::location::center;
					primary->swap_parent(gse::id());

					gse::gui::layout::dock(s.menus, explorer_id, primary_id, gse::gui::dock::location::left);
					explorer->dock_split_ratio = 0.2f;
					gse::gui::layout::update(s.menus, primary_id);
				},
			});
		}

	return {};
}
