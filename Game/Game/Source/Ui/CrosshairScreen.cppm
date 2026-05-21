export module gs:crosshair_screen;

import std;
import gse;

import :crosshair_system;

export namespace gs {
	class crosshair_screen : public gse::gui::screen {
	public:
		crosshair_screen(
			const gse::save::registry& save_reg,
			const crosshair_system::data& crosshair,
			gse::channel_writer channels
		);

		auto build(gse::gui::builder& ui, gse::gui::nav& n) -> void override;

		auto title() const -> std::string_view override;

	private:
		auto draw_preview(const gse::gui::draw_context& ctx, const gse::gui::ui_rect& rect) const -> void;

		const gse::save::registry* m_save_reg;
		const crosshair_system::data* m_crosshair;
		gse::channel_writer m_channels;
		gse::settings::panel_state m_panel_state;
	};
}

gs::crosshair_screen::crosshair_screen(
	const gse::save::registry& save_reg,
	const crosshair_system::data& crosshair,
	gse::channel_writer channels
)
	: m_save_reg(&save_reg),
	  m_crosshair(&crosshair),
	  m_channels(std::move(channels)) {
}

auto gs::crosshair_screen::title() const -> std::string_view {
	return "Crosshair";
}

auto gs::crosshair_screen::draw_preview(
	const gse::gui::draw_context& ctx,
	const gse::gui::ui_rect& rect
) const -> void {
	ctx.queue_sprite({
		.rect = rect,
		.color = { 0.06f, 0.06f, 0.09f, 1.f },
		.texture = ctx.blank_texture,
	});

	constexpr float border = 1.f;
	const auto border_color = ctx.style.color_border;

	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size(rect.top_left(), { rect.width(), border }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size(
			{ rect.left(), rect.bottom() + border },
			{ rect.width(), border }
		),
		.color = border_color,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size(rect.top_left(), { border, rect.height() }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect =
			gse::gui::ui_rect::from_position_size({ rect.right() - border, rect.top() }, { border, rect.height() }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});

	const auto& c = *m_crosshair;
	const gse::vec2f center = rect.center();
	const gse::vec4f color = { c.color_r, c.color_g, c.color_b, c.opacity };
	const gse::vec4f outline_color = { 0.f, 0.f, 0.f, c.outline_opacity };

	const float arm = c.arm_length;
	const float thickness = c.arm_thickness;
	const float gap = c.gap;
	const float outline = c.outline_thickness;

	const auto push_arm = [&](const gse::gui::ui_rect& r) {
		if (outline > 0.f) {
			ctx.queue_sprite({
				.rect = r.inset({ -outline, -outline }),
				.color = outline_color,
				.texture = ctx.blank_texture,
			});
		}
		ctx.queue_sprite({
			.rect = r,
			.color = color,
			.texture = ctx.blank_texture,
		});
	};

	if (arm > 0.f) {
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - gap - arm, center.y() + thickness * 0.5f },
				{ arm, thickness }
			)
		);
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() + gap, center.y() + thickness * 0.5f },
				{ arm, thickness }
			)
		);
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - thickness * 0.5f, center.y() + gap + arm },
				{ thickness, arm }
			)
		);
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - thickness * 0.5f, center.y() - gap },
				{ thickness, arm }
			)
		);
	}

	if (c.show_dot) {
		const float dot = c.dot_size;
		push_arm(
			gse::gui::ui_rect::from_position_size({ center.x() - dot * 0.5f, center.y() + dot * 0.5f }, { dot, dot })
		);
	}
}

auto gs::crosshair_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	const auto& ctx = ui.ctx;
	const float pad = ctx.style.padding;
	const auto content = ctx.current_menu->rect.inset({ pad, pad });

	constexpr float preview_height = 160.f;
	const auto preview_rect =
		gse::gui::ui_rect::from_position_size(content.top_left(), { content.width(), preview_height });

	draw_preview(ctx, preview_rect);

	ctx.layout_cursor = { content.left(), content.top() - preview_height - pad * 2.f };

	ui.scroll_region(
		{
			.id = "crosshair.settings"
		},
		[this](gse::gui::builder& b) {
			gse::settings::panel(b, m_panel_state, m_channels, *m_save_reg, "Crosshair");
		}
	);
}
