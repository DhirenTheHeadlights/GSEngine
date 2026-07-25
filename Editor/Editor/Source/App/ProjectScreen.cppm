export module gse.ide.app:project_screen;

import std;
import gse;

import gse.ide.project;

export namespace gse::ide {
	class project_screen : public gui::screen {
	public:
		project_screen();

		auto build(
			gui::builder& ui,
			gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto captures_input() const -> bool override;

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
			std::string label;
			bool current = false;
		};

		std::vector<entry> m_entries;
		bool m_dismiss = false;
	};
}

gse::ide::project_screen::project_screen() {
	const project::manifest& active = project::current();

	for (const std::filesystem::path& path : project::known()) {
		const project::manifest found = project::load(path);
		if (!found.valid) {
			continue;
		}
		m_entries.push_back({
			.manifest = path,
			.label = std::format("{}   {}", found.name, found.root.generic_display_string()),
			.current = active.valid && path == active.file
		});
	}
}

auto gse::ide::project_screen::title() const -> std::string_view {
	return "Switch Project";
}

auto gse::ide::project_screen::captures_input() const -> bool {
	return true;
}

auto gse::ide::project_screen::dismissable() const -> bool {
	return true;
}

auto gse::ide::project_screen::should_dismiss() const -> bool {
	return m_dismiss;
}

auto gse::ide::project_screen::body_rect(const gui::style& sty, const vec2f viewport_size) const -> rect_t<vec2f> {
	const float w = std::min(viewport_size.x() * 0.5f, 720.f * sty.scale_factor);
	const float h = std::min(viewport_size.y() * 0.5f, 420.f * sty.scale_factor);
	return rectf::from_position_size(
		{ (viewport_size.x() - w) * 0.5f, (viewport_size.y() + h) * 0.5f },
		{ w, h }
	);
}

auto gse::ide::project_screen::draw_backdrop(gui::draw_context& ctx, const vec2f viewport_size) const -> void {
	ctx.sprites.push_back({
		.rect = rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size),
		.color = { 0.f, 0.f, 0.f, 0.55f },
		.texture = ctx.blank_texture,
		.layer = render_layer::overlay,
	});
	const rectf card = body_rect(ctx.style, viewport_size);
	ctx.sprites.push_back({
		.rect = card,
		.color = { ctx.style.color_menu_body.x(), ctx.style.color_menu_body.y(), ctx.style.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.layer = render_layer::overlay,
		.corner_radius = ctx.style.corner_radius_menu,
	});
}

auto gse::ide::project_screen::build(gui::builder& ui, gui::nav&) -> void {
	if (m_entries.empty()) {
		ui.draw<gui::text>({
			.content = "No projects found.",
		});
		return;
	}

	for (const entry& current : m_entries) {
		if (!ui.draw<gui::selectable>({ .text = current.label, .selected = current.current })) {
			continue;
		}

		m_dismiss = true;
		if (current.current) {
			continue;
		}

		// The config table is immutable for the process lifetime, so switching
		// projects means starting over with the new manifest on the command line.
		gse::app::relaunch_on_exit(
			gse::config::executable_file(),
			current.manifest.parent_path(),
			{ current.manifest }
		);
		gse::shutdown();
	}
}
