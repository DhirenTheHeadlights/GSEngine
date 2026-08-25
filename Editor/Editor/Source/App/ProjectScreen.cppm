export module gse.ide.app:project_screen;

import std;
import gse;

import gse.ide.git;
import gse.ide.project;

export namespace gse::ide {
	class project_screen : public gui::screen {
	public:
		explicit project_screen(
			gse::channel_write<window_launcher_mode_request, window_open_file_request> channels,
			bool hub = false
		);

		auto build(
			gui::builder& ui,
			gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto captures_input() const -> bool override;

		auto occludes() const -> bool override;

		auto wants_chrome() const -> bool override;

		auto on_pop() -> void override;

		auto dismissable() const -> bool override;

		auto should_dismiss() const -> bool override;

		auto body_rect(
			const gui::style& sty,
			vec2f viewport_size
		) const -> rect_t<vec2f> override;

		auto draw_backdrop(
			gui::draw_context& ctx,
			vec2f viewport_size
		) const -> void override;

	private:
		struct entry {
			std::filesystem::path manifest;
			std::string key;
			std::string name;
			std::string location;
			vec4f accent;
			bool current = false;
		};

		auto activate(
			const entry& item
		) -> void;

		auto open(
			const std::filesystem::path& manifest
		) -> void;

		auto build_list(
			gui::builder& ui
		) -> void;

		auto build_create(
			gui::builder& ui
		) -> void;

		struct engine_choice {
			project::engine_entry entry;
			std::string location;
			bool current = false;
		};

		auto build_rebind(
			gui::builder& ui
		) -> void;


		std::vector<entry> m_entries;
		int m_selected = 0;
		bool m_creating = false;
		bool m_init_git = true;
		int m_template = 0;
		std::string m_new_name;
		std::string m_error;
		std::string m_error_name;
		gui::text_input_state m_input;
		bool m_dismiss = false;
		bool m_hub = false;
		gse::channel_write<window_launcher_mode_request, window_open_file_request> m_channels;
		bool m_launcher_sent = false;
		int m_requested_height = 0;
		float m_content_height = 0.f;
		std::vector<engine_choice> m_engines;
		int m_engine_selected = 0;
		bool m_rebinding = false;
		bool m_forced = false;
		std::string m_engine_problem;
	};
}

gse::ide::project_screen::project_screen(gse::channel_write<window_launcher_mode_request, window_open_file_request> channels, const bool hub) : m_hub(hub), m_channels(std::move(channels)) {
	const project::manifest& active = project::current();

	for (const std::filesystem::path& path : project::known()) {
		const project::manifest found = project::load(path);
		if (!found.valid) {
			continue;
		}
		const bool current = project::opened() && path == active.file;
		if (current) {
			m_selected = static_cast<int>(m_entries.size());
		}
		m_entries.push_back({
			.manifest = path,
			.key = path.generic_display_string(),
			.name = found.name,
			.location = found.root.generic_display_string(),
			.accent = found.accent,
			.current = current
		});
	}

	for (project::engine_entry& candidate : project::engines()) {
		const bool current = active.valid && candidate.path == active.engine;
		if (current) {
			m_engine_selected = static_cast<int>(m_engines.size());
		}
		std::string location = candidate.path.generic_display_string();
		m_engines.push_back({
			.entry = std::move(candidate),
			.location = std::move(location),
			.current = current
		});
	}

	if (active.valid && !active.engine_problem.empty()) {
		m_engine_problem = active.engine_problem;
		m_rebinding = true;
		m_forced = true;
	}
}

auto gse::ide::project_screen::title() const -> std::string_view {
	return m_hub ? "GSEditor" : "Projects";
}

auto gse::ide::project_screen::captures_input() const -> bool {
	return true;
}

auto gse::ide::project_screen::occludes() const -> bool {
	return m_hub;
}

auto gse::ide::project_screen::wants_chrome() const -> bool {
	return m_hub;
}

auto gse::ide::project_screen::on_pop() -> void {
	if (m_launcher_sent) {
		m_channels.push<window_launcher_mode_request>({ .active = false });
	}
}

auto gse::ide::project_screen::dismissable() const -> bool {
	return !m_hub && !m_forced;
}

auto gse::ide::project_screen::should_dismiss() const -> bool {
	return m_dismiss;
}

auto gse::ide::project_screen::body_rect(const gui::style& sty, const vec2f viewport_size) const -> rect_t<vec2f> {
	if (m_hub) {
		return rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size);
	}

	const float w = std::min(viewport_size.x(), 720.f * sty.scale_factor);
	const float desired = m_content_height > 0.f ? m_content_height : 420.f * sty.scale_factor;
	const float h = std::min(viewport_size.y(), desired);
	return rectf::from_position_size(
		{ (viewport_size.x() - w) * 0.5f, (viewport_size.y() + h) * 0.5f },
		{ w, h }
	);
}

auto gse::ide::project_screen::draw_backdrop(gui::draw_context& ctx, const vec2f viewport_size) const -> void {
	ctx.sprites.push_back({
		.rect = rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size),
		.color = m_hub ? vec4f{ vec3f(ctx.style.color_menu_body), 1.f } : vec4f{ 0.f, 0.f, 0.f, 0.55f },
		.texture = ctx.blank_texture,
		.layer = render_layer::overlay,
	});

	if (m_hub) {
		return;
	}
	const rectf card = body_rect(ctx.style, viewport_size);
	ctx.sprites.push_back({
		.rect = card,
		.color = { vec3f(ctx.style.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.layer = render_layer::overlay,
		.corner_radius = ctx.style.corner_radius_menu,
	});
}

auto gse::ide::project_screen::open(const std::filesystem::path& manifest) -> void {
	gse::app::relaunch_on_exit(
		gse::config::executable_file(),
		manifest.parent_path(),
		{ manifest }
	);
	gse::shutdown();
}

auto gse::ide::project_screen::activate(const entry& item) -> void {
	m_dismiss = true;
	if (item.current) {
		return;
	}
	open(item.manifest);
}

auto gse::ide::project_screen::build(gui::builder& ui, gui::nav&) -> void {
	auto& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	if (!ctx.current_menu) {
		return;
	}

	const auto scope = ctx.scoped_layer(render_layer::popup);
	const gui::style& sty = ctx.style;
	const rectf card = ctx.current_menu->rect;
	const float pad = sty.padding;

	const rectf header = rectf::from_position_size(
		{ card.left() + pad, card.top() - pad },
		{ card.width() - pad * 2.f, text_view->line_height(sty.font_size) + pad }
	);

	const float action_w = text_view->width("New Project", sty.font_size) + pad * 2.f;
	const float action_h = text_view->line_height(sty.font_size) + pad * 0.5f;
	const rectf action_rect = rectf::from_position_size(
		{ header.right() - action_w, (card.top() + header.bottom()) * 0.5f + action_h * 0.5f },
		{ action_w, action_h }
	);

	const bool listing = !m_creating && !m_rebinding;
	const float engine_w = text_view->width("Engine", sty.font_size) + pad * 2.f;
	const rectf engine_rect = rectf::from_position_size(
		{ action_rect.left() - pad - engine_w, action_rect.top() },
		{ engine_w, action_h }
	);

	const float open_w = text_view->width("Open", sty.font_size) + pad * 2.f;
	const rectf open_rect = rectf::from_position_size(
		{ engine_rect.left() - pad - open_w, engine_rect.top() },
		{ open_w, action_h }
	);

	const float title_limit = listing ? open_rect.left() : action_rect.left();

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = m_creating ? std::string_view("New Project") : m_rebinding ? std::string_view("Engine") : std::string_view("Projects"),
		.position = { header.left(), header.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = rectf::from_position_size(
			{ header.left(), header.top() },
			{ std::max(0.f, title_limit - pad - header.left()), header.height() }
		),
	});

	if (listing && gui::draw::button_in_rect(ctx, "Open", "##project_open", open_rect, ui.hot_widget_id, ui.active_widget_id)) {
		m_channels.push<window_open_file_request>({
			.title = "Open Project",
			.filter_name = "GSE Project",
			.filter_pattern = "*.gseproj",
		});
	}

	if (listing && project::current().valid && gui::draw::button_in_rect(ctx, "Engine", "##project_engine", engine_rect, ui.hot_widget_id, ui.active_widget_id)) {
		m_rebinding = true;
	}

	if (!m_creating && !m_rebinding && gui::draw::button_in_rect(ctx, "New Project", "##project_new", action_rect, ui.hot_widget_id, ui.active_widget_id)) {
		m_creating = true;
		m_new_name.clear();
		m_error.clear();
	}

	ctx.queue_sprite({
		.rect = rectf::from_position_size(
			{ header.left(), header.bottom() },
			{ header.width(), std::max(1.f, sty.scale_factor) }
		),
		.color = sty.color_accent_dim,
		.texture = ctx.blank_texture,
	});

	ctx.layout_cursor = { card.left() + pad, header.bottom() - pad };

	const float line = text_view->line_height(sty.font_size);
	const float row_stride = line + pad * 1.5f;

	float body = 0.f;
	if (m_creating) {
		body += line + pad;
		body += line + pad * 2.f;
		body += row_stride * static_cast<float>(project::templates().size());
		body += line + pad * sty.widget_height_padding + pad + sty.item_spacing;
		body += line + pad * 3.f;
	}
	else if (m_rebinding) {
		body += line + pad;
		body += row_stride * static_cast<float>(std::max<std::size_t>(m_engines.size(), 1u));
	}
	else {
		body += row_stride * static_cast<float>(std::max<std::size_t>(m_entries.size(), 1u));
	}

	m_content_height = (m_hub ? sty.title_bar_height : 0.f) + pad + header.height() + pad + body + pad;

	if (m_hub) {
		if (const int height = static_cast<int>(m_content_height + pad * 4.f); height != m_requested_height) {
			m_channels.push<window_launcher_mode_request>({
				.active = true,
				.width = static_cast<int>(720.f * sty.scale_factor + pad * 4.f),
				.height = height,
			});
			m_requested_height = height;
			m_launcher_sent = true;
		}
	}

	if (m_creating) {
		build_create(ui);
		return;
	}
	if (m_rebinding) {
		build_rebind(ui);
		return;
	}
	build_list(ui);
}

auto gse::ide::project_screen::build_rebind(gui::builder& ui) -> void {
	auto& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const gui::style& sty = ctx.style;
	const rectf card = ctx.current_menu->rect;
	const float pad = sty.padding;

	const project::manifest& active = project::current();

	if (ctx.key_pressed(key::escape)) {
		m_rebinding = false;
		m_forced = false;
		return;
	}

	std::string formatted;
	std::string_view hint;
	if (!m_engine_problem.empty()) {
		hint = m_engine_problem;
	}
	else if (m_engines.empty()) {
		hint = "No engines registered yet.";
	}
	else {
		formatted = std::format("{} builds against this engine.", active.name);
		hint = formatted;
	}

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = hint,
		.position = { card.left() + pad, ctx.layout_cursor.y() - text_view->line_height(sty.font_size) * 0.5f },
		.scale = sty.font_size,
		.color = m_engine_problem.empty() ? sty.color_text_secondary : sty.color_error,
		.clip_rect = rectf::from_position_size(
			{ card.left() + pad, ctx.layout_cursor.y() },
			{ card.width() - pad * 2.f, text_view->line_height(sty.font_size) + pad }
		),
	});

	ctx.layout_cursor = { card.left() + pad, ctx.layout_cursor.y() - text_view->line_height(sty.font_size) - pad };

	if (!m_engines.empty()) {
		if (ctx.key_pressed(key::down)) {
			m_engine_selected = std::min(m_engine_selected + 1, static_cast<int>(m_engines.size()) - 1);
		}
		if (ctx.key_pressed(key::up)) {
			m_engine_selected = std::max(m_engine_selected - 1, 0);
		}
	}

	float widest_name = 0.f;
	for (const engine_choice& item : m_engines) {
		widest_name = std::max(widest_name, text_view->width(item.entry.name, sty.font_size));
	}
	const float detail_column = pad * 3.f + sty.accent_bar_width + widest_name;

	const bool submit = ctx.key_pressed(key::enter) || ctx.key_pressed(key::kp_enter);

	for (const auto& [index, item] : std::views::enumerate(m_engines)) {
		const bool chosen = ui.draw<gui::selectable>({
			.text = item.entry.name,
			.detail = item.location,
			.key = item.entry.name,
			.selected = m_engine_selected == static_cast<int>(index),
			.align = gui::selectable_align::left,
			.detail_column = detail_column,
		});
		if (chosen || (submit && m_engine_selected == static_cast<int>(index))) {
			if (!active.valid || item.current) {
				m_rebinding = false;
				return;
			}
			project::bind_engine(active.file, item.entry);
			m_dismiss = true;
			open(active.file);
			return;
		}
	}
}

auto gse::ide::project_screen::build_list(gui::builder& ui) -> void {
	auto& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();

	if (!m_hub && ctx.key_pressed(key::escape)) {
		m_dismiss = true;
		return;
	}
	if (!m_entries.empty()) {
		if (ctx.key_pressed(key::down)) {
			m_selected = std::min(m_selected + 1, static_cast<int>(m_entries.size()) - 1);
		}
		if (ctx.key_pressed(key::up)) {
			m_selected = std::max(m_selected - 1, 0);
		}
		if (ctx.key_pressed(key::enter) || ctx.key_pressed(key::kp_enter)) {
			activate(m_entries[static_cast<std::size_t>(m_selected)]);
			return;
		}
	}

	float widest_name = 0.f;
	for (const entry& item : m_entries) {
		widest_name = std::max(widest_name, text_view->width(item.name, ctx.style.font_size));
	}
	const float detail_column = ctx.style.padding * 3.f + ctx.style.accent_bar_width + widest_name;

	for (const auto& [index, item] : std::views::enumerate(m_entries)) {
		if (ui.draw<gui::selectable>({
			.text = item.name,
			.detail = item.location,
			.key = item.key,
			.selected = m_selected == static_cast<int>(index),
			.align = gui::selectable_align::left,
			.accent = item.accent,
			.detail_column = detail_column,
		})) {
			activate(item);
			return;
		}
	}

}

auto gse::ide::project_screen::build_create(gui::builder& ui) -> void {
	auto& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const gui::style& sty = ctx.style;
	const rectf card = ctx.current_menu->rect;
	const float pad = sty.padding;

	if (ctx.key_pressed(key::escape)) {
		m_creating = false;
		return;
	}

	if (m_new_name != m_error_name) {
		m_error.clear();
	}

	const rectf input_rect = rectf::from_position_size(
		{ card.left() + pad, ctx.layout_cursor.y() },
		{ card.width() - pad * 2.f, text_view->line_height(sty.font_size) + pad }
	);

	const id input_id = gui::ids::make("##project_new_name");
	ui.focus_widget_id = input_id;
	gui::draw::text_input_in_rect(ctx, input_id, m_new_name, m_input, input_rect, ui.hot_widget_id, ui.focus_widget_id);

	if (m_new_name.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "Project name",
			.position = { input_rect.left() + pad, input_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = input_rect,
		});
	}

	const std::filesystem::path destination = gse::config::projects_root() / m_new_name;
	const std::string problem = project::validate_new(m_new_name, gse::config::projects_root());

	const std::string hint = !m_error.empty() ? m_error : problem.empty() ? destination.generic_display_string() : problem;

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = hint,
		.position = { input_rect.left(), input_rect.bottom() - pad - text_view->line_height(sty.font_size) * 0.5f },
		.scale = sty.font_size,
		.color = m_error.empty() && problem.empty() ? sty.color_text_secondary : sty.color_error,
		.clip_rect = rectf::from_position_size(
			{ input_rect.left(), input_rect.bottom() - pad },
			{ input_rect.width(), text_view->line_height(sty.font_size) + pad }
		),
	});

	ctx.layout_cursor = { card.left() + pad, input_rect.bottom() - pad * 2.f - text_view->line_height(sty.font_size) };

	for (const auto& [index, entry] : std::views::enumerate(project::templates())) {
		if (ui.draw<gui::selectable>({
			.text = entry.label,
			.detail = entry.detail,
			.key = entry.id,
			.selected = m_template == static_cast<int>(index),
			.align = gui::selectable_align::left,
			.detail_column = pad * 3.f + sty.accent_bar_width + text_view->width("Blank", sty.font_size),
		})) {
			m_template = static_cast<int>(index);
		}
	}

	ui.draw<gui::toggle>({
		.name = "Initialize Git Repository",
		.value = m_init_git,
	});

	const bool submit = ctx.key_pressed(key::enter) || ctx.key_pressed(key::kp_enter);

	const float action_h = text_view->line_height(sty.font_size) + pad;
	const float action_w = (card.width() - pad * 3.f) * 0.5f;
	const rectf create_rect = rectf::from_position_size(
		{ card.left() + pad, card.bottom() + pad + action_h },
		{ action_w, action_h }
	);
	const rectf cancel_rect = rectf::from_position_size(
		{ create_rect.right() + pad, create_rect.top() },
		{ action_w, action_h }
	);

	const bool valid = problem.empty();

	if ((gui::draw::button_in_rect(ctx, "Create", "##project_create_confirm", create_rect, ui.hot_widget_id, ui.active_widget_id, valid) || submit) && valid) {
		const std::expected<std::filesystem::path, std::string> created = project::create(
			m_new_name,
			gse::config::projects_root(),
			project::templates()[static_cast<std::size_t>(m_template)].id
		);
		if (!created) {
			m_error = created.error();
			m_error_name = m_new_name;
			return;
		}
		if (m_init_git) {
			if (const std::expected<void, std::string> repo = git::initialize(created->parent_path()); !repo) {
				log::println(log::level::error, log::category::general, "git init failed: {}", repo.error());
			}
		}
		m_dismiss = true;
		open(*created);
		return;
	}

	if (gui::draw::button_in_rect(ctx, "Cancel", "##project_create_cancel", cancel_rect, ui.hot_widget_id, ui.active_widget_id)) {
		m_creating = false;
	}
}
