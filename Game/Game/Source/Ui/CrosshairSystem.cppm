export module gs:crosshair_system;

import std;
import gse;

export namespace gs {
	struct crosshair_system {
		struct data;

		static auto draw_settings_page(
			gse::gui::builder& b,
			gse::settings::panel_state& ps,
			const data& live,
			data& pending,
			gse::channel_writer& channels
		) -> void;

		static auto draw_preview(
			const gse::gui::draw_context& ctx,
			const gse::gui::ui_rect& rect,
			const data& crosshair
		) -> void;

		struct [[
			= gse::settings::category<"Crosshair">{},
			= gse::settings::page_drawer<&crosshair_system::draw_settings_page>{}
		]] data {
			[[= gse::settings::describe<"Show the crosshair while in-game.">{}]] bool show = true;

			[[
				= gse::settings::describe<"Length of each arm in pixels.">{},
				= gse::settings::range<0, 30>{}
			]]
			int arm_length = 8;

			[[
				= gse::settings::describe<"Thickness of each arm in pixels.">{},
				= gse::settings::range<1, 8>{}
			]]
			int arm_thickness = 2;

			[[
				= gse::settings::describe<"Gap between center and each arm.">{},
				= gse::settings::range<0, 20>{}
			]]
			int gap = 3;

			[[= gse::settings::describe<"Show the center dot.">{}]] bool show_dot = true;

			[[
				= gse::settings::describe<"Center dot size in pixels.">{},
				= gse::settings::range<1, 6>{}
			]]
			int dot_size = 1;

			[[
				= gse::settings::describe<"Red channel.">{},
				= gse::settings::range<0.f, 1.f>{}
			]]
			float color_r = 1.f;

			[[
				= gse::settings::describe<"Green channel.">{},
				= gse::settings::range<0.f, 1.f>{}
			]]
			float color_g = 1.f;

			[[
				= gse::settings::describe<"Blue channel.">{},
				= gse::settings::range<0.f, 1.f>{}
			]]
			float color_b = 1.f;

			[[
				= gse::settings::describe<"Opacity.">{},
				= gse::settings::range<0.f, 1.f>{}
			]]
			float opacity = 0.9f;

			[[
				= gse::settings::describe<"Outline thickness around each arm (0 = no outline).">{},
				= gse::settings::range<0, 3>{}
			]]
			int outline_thickness = 0;

			[[
				= gse::settings::describe<"Outline opacity.">{},
				= gse::settings::range<0.f, 1.f>{}
			]]
			float outline_opacity = 1.f;
		};
	};
}

auto gs::crosshair_system::draw_settings_page(gse::gui::builder& b, gse::settings::panel_state& ps, const data& live, data& pending, gse::channel_writer& channels) -> void {
	auto& ctx = b.ctx;
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	b.draw<gse::gui::section>({ .title = "Crosshair" });

	const gse::gui::ui_rect preview = lo::reserve_row(ctx, sty.preview_height, sty.padding);
	draw_preview(ctx, preview, pending);

	b.scroll_region(
		{ .id = "crosshair.fields" },
		[&](gse::gui::builder& sub) {
			gse::settings::draw_fields<crosshair_system>(sub, ps, live, channels);
		}
	);
}

auto gs::crosshair_system::draw_preview(const gse::gui::draw_context& ctx, const gse::gui::ui_rect& rect, const data& crosshair) -> void {
	const auto& sty = ctx.style;
	const float border = sty.separator_thickness;
	const gse::vec4f border_color = sty.color_border;

	ctx.queue_sprite({
		.rect = rect,
		.color = { 0.06f, 0.06f, 0.09f, 1.f },
		.texture = ctx.blank_texture,
	});

	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size(rect.top_left(), { rect.width(), border }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size({ rect.left(), rect.bottom() + border }, { rect.width(), border }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size(rect.top_left(), { border, rect.height() }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = gse::gui::ui_rect::from_position_size({ rect.right() - border, rect.top() }, { border, rect.height() }),
		.color = border_color,
		.texture = ctx.blank_texture,
	});

	const gse::vec2f center = rect.center();
	const gse::vec4f color = { crosshair.color_r, crosshair.color_g, crosshair.color_b, crosshair.opacity };
	const gse::vec4f outline_color = { 0.f, 0.f, 0.f, crosshair.outline_opacity };

	const float arm = crosshair.arm_length;
	const float thickness = crosshair.arm_thickness;
	const float gap = crosshair.gap;
	const float outline = crosshair.outline_thickness;

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
		push_arm(gse::gui::ui_rect::from_position_size(
			{ center.x() - gap - arm, center.y() + thickness * 0.5f },
			{ arm, thickness }
		));
		push_arm(gse::gui::ui_rect::from_position_size(
			{ center.x() + gap, center.y() + thickness * 0.5f },
			{ arm, thickness }
		));
		push_arm(gse::gui::ui_rect::from_position_size(
			{ center.x() - thickness * 0.5f, center.y() + gap + arm },
			{ thickness, arm }
		));
		push_arm(gse::gui::ui_rect::from_position_size(
			{ center.x() - thickness * 0.5f, center.y() - gap },
			{ thickness, arm }
		));
	}

	if (crosshair.show_dot) {
		const float dot = crosshair.dot_size;
		push_arm(gse::gui::ui_rect::from_position_size(
			{ center.x() - dot * 0.5f, center.y() + dot * 0.5f },
			{ dot, dot }
		));
	}
}
