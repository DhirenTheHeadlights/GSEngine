export module gs:main_menu_screen;

import std;
import gse;

import :crosshair_screen;
import :crosshair_system;
import :network_screen;
import :settings_screen;

export namespace gs {
	class main_menu_screen : public gse::gui::screen {
	public:
		main_menu_screen(
			const gse::world_system::data& world,
			const gse::network::data& net,
			const gse::save::registry& save_reg,
			const crosshair_system::data& crosshair,
			gse::channel_writer channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title(
		) const -> std::string_view override;

		auto on_push(
		) -> void override;

		auto dismissable(
		) const -> bool override;

		auto body_rect(
			const gse::gui::style& sty,
			gse::vec2f viewport_size
		) const -> gse::gui::ui_rect override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;
	private:
		[[nodiscard]] auto eased_progress(
		) const -> float;

		[[nodiscard]] auto target_width(
			gse::vec2f viewport_size
		) const -> float;

		const gse::world_system::data* m_world;
		const gse::network::data* m_net;
		const gse::save::registry* m_save_reg;
		const crosshair_system::data* m_crosshair;
		gse::channel_writer m_channels;
		gse::clock m_opened_at;

		static constexpr gse::time slide_duration = gse::milliseconds(220.f);
		static constexpr float dim_max_alpha = 0.55f;
		static constexpr float panel_max_width = 320.f;
	};
}

gs::main_menu_screen::main_menu_screen(const gse::world_system::data& world, const gse::network::data& net, const gse::save::registry& save_reg, const crosshair_system::data& crosshair, gse::channel_writer channels)
	: m_world(&world), m_net(&net), m_save_reg(&save_reg), m_crosshair(&crosshair), m_channels(std::move(channels)) {}

auto gs::main_menu_screen::on_push() -> void {
	m_opened_at.reset();
}

auto gs::main_menu_screen::title() const -> std::string_view {
	return "Menu";
}

auto gs::main_menu_screen::dismissable() const -> bool {
	return m_world->active_scene.has_value();
}

auto gs::main_menu_screen::eased_progress() const -> float {
	const float elapsed_s = m_opened_at.elapsed<float>().as<gse::seconds>();
	const float duration_s = slide_duration.as<gse::seconds>();
	const float t = std::clamp(elapsed_s / duration_s, 0.f, 1.f);
	const float inv = 1.0f - t;
	return 1.0f - inv * inv * inv;
}

auto gs::main_menu_screen::target_width(const gse::vec2f viewport_size) const -> float {
	return std::min(panel_max_width, viewport_size.x() * 0.35f);
}

auto gs::main_menu_screen::body_rect(const gse::gui::style&, const gse::vec2f viewport_size) const -> gse::gui::ui_rect {
	const float width = target_width(viewport_size);
	const float eased = eased_progress();
	const float left = -width * (1.0f - eased);
	return gse::gui::ui_rect::from_position_size(
		{ left, viewport_size.y() },
		{ width, viewport_size.y() }
	);
}

auto gs::main_menu_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	const float eased = eased_progress();

	const gse::gui::ui_rect full = gse::gui::ui_rect::from_position_size(
		{ 0.f, viewport_size.y() },
		{ viewport_size.x(), viewport_size.y() }
	);

	ctx.sprites.push_back({
		.rect = full,
		.color = { 0.f, 0.f, 0.f, dim_max_alpha * eased },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
	});

	const gse::gui::ui_rect panel = body_rect(ctx.style, viewport_size);
	const gse::vec4f panel_color = {
		ctx.style.color_menu_body.x() * 1.05f,
		ctx.style.color_menu_body.y() * 1.05f,
		ctx.style.color_menu_body.z() * 1.05f,
		1.0f,
	};

	ctx.sprites.push_back({
		.rect = panel,
		.color = panel_color,
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
	});

	const gse::gui::ui_rect border = gse::gui::ui_rect::from_position_size(
		{ panel.right() - 1.f, panel.top() },
		{ 1.f, panel.height() }
	);
	ctx.sprites.push_back({
		.rect = border,
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
	});
}

auto gs::main_menu_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	const auto& world = *m_world;
	const bool scene_active = world.active_scene.has_value();

	if (scene_active) {
		if (ui.draw<gse::gui::button>({ .text = "Resume" })) {
			m_channels.push<gse::gui::pop_screen_request>({});
		}
	}

	for (const auto& [scene_id, scene_ptr] : world.scenes) {
		const std::string scene_name(scene_ptr->id().tag());
		if (ui.draw<gse::gui::button>({ .text = scene_name })) {
			if (scene_active) {
				m_channels.push<gse::deactivate_active_scene_request>({});
			}
			m_channels.push<gse::activate_scene_request>({ .scene_id = scene_id });
			m_channels.push<gse::gui::pop_screen_request>({});
		}
	}

	if (ui.draw<gse::gui::button>({ .text = "Network" })) {
		m_channels.push<gse::gui::push_screen_request>({
			.factory = [net = m_net, channels = m_channels] {
				return std::make_unique<network_screen>(*net, channels);
			},
		});
	}

	if (ui.draw<gse::gui::button>({ .text = "Crosshair" })) {
		m_channels.push<gse::gui::push_screen_request>({
			.factory = [save_reg = m_save_reg, crosshair = m_crosshair, channels = m_channels] {
				return std::make_unique<crosshair_screen>(*save_reg, *crosshair, channels);
			},
		});
	}

	if (ui.draw<gse::gui::button>({ .text = "Settings" })) {
		m_channels.push<gse::gui::push_screen_request>({
			.factory = [save_reg = m_save_reg, channels = m_channels] {
				return std::make_unique<settings_screen>(*save_reg, channels);
			},
		});
	}

	if (scene_active) {
		if (ui.draw<gse::gui::button>({ .text = "Exit to Main Menu" })) {
			m_channels.push<gse::deactivate_active_scene_request>({});
		}
	}

	if (ui.draw<gse::gui::button>({ .text = "Quit" })) {
		gse::shutdown();
	}
}
