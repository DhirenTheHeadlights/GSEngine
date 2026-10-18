export module gse.graphics:section_widget;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.time;
import std;

import :font;
import :ids;
import :interaction;
import :layout_ops;
import :styles;
import :surface_paint;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct section {
		using result = void;
		struct params {
			std::string_view title;
			std::string_view subtitle = {};
			std::string_view action_icon = {};
			std::function<void()> on_action = {};
			std::string_view secondary_action_icon = {};
			std::function<void()> on_secondary_action = {};
			resource::handle<font> font{};
		};
		static auto draw(
			const draw_context& ctx,
			params p,
			id& hot,
			id& active,
			id&
		) -> void;
	};
}

auto gse::gui::section::draw(const draw_context& ctx, params p, id& hot, id& active, id&) -> void {
	const auto fnt = ctx.fonts.face_or(p.font);
	if (!ctx.current_menu || !fnt.valid()) {
		return;
	}

	const auto fnt_view = fnt.resolve();

	const auto& sty = ctx.style;
	namespace lo = layout;

	const rectf content_rect = ctx.current_menu->rect.inset({ sty.padding, sty.padding });

	lo::skip(ctx, sty.section_spacing_above);

	const float header_size = sty.font_size * sty.section_header_size_mult;
	const rectf header_row = lo::reserve_row(ctx, fnt_view->line_height(header_size));
	const float title_top = header_row.top();
	const float bar_height = fnt_view->line_height(header_size);

	const section_skin& skin = sty.section;

	const rectf bar_rect = rectf::from_position_size(
		{ content_rect.left(), title_top },
		{ sty.accent_bar_width, bar_height }
	);

	draw::fill_surface(ctx, bar_rect, skin.accent_bar, {});

	const float text_left = content_rect.left() + sty.accent_bar_width + sty.padding * 0.6f;

	draw::label(
		ctx,
		rectf::from_position_size({ text_left, title_top }, { std::max(0.f, content_rect.right() - text_left), bar_height }),
		p.title,
		skin.title,
		{
			.color = skin.title_color,
			.font = p.font,
			.clip_rect = content_rect,
		}
	);

	const float action_height = bar_height;
	float action_cursor_x = content_rect.right();

	auto draw_action = [&](const std::string_view icon, const std::string_view key, const std::function<void()>& on_click) {
		if (!on_click || icon.empty()) {
			return;
		}
		const float icon_w = fnt_view->width(icon, sty.font_size) + sty.padding;
		const rectf action_rect = rectf::from_position_size(
			{ action_cursor_x - icon_w, title_top },
			{ icon_w, action_height }
		);

		const interaction::press button = interaction::press_in_rect(ctx, hot, active, ids::make(key), {
			.rect = action_rect,
		});

		draw::fill_surface(ctx, action_rect, button.select(skin.action.surfaces), {
			.anim = button.widget,
		});

		draw::label(ctx, action_rect, icon, skin.action.label, {
			.color = button.select(skin.action.label_colors),
			.font = p.font,
			.clip_rect = action_rect,
		});

		if (button.activated) {
			on_click();
		}

		action_cursor_x -= icon_w + sty.padding * 0.5f;
	};

	{
		const ids::scope section_scope{ p.title };
		draw_action(p.action_icon, "##section_action", p.on_action);
		draw_action(p.secondary_action_icon, "##section_action_secondary", p.on_secondary_action);
	}

	if (!p.subtitle.empty()) {
		lo::skip(ctx, sty.item_spacing);
		const rectf subtitle_row = lo::reserve_row(ctx, fnt_view->line_height(sty.font_size));
		draw::label(
			ctx,
			rectf::from_position_size({ text_left, subtitle_row.top() }, { std::max(0.f, content_rect.right() - text_left), subtitle_row.height() }),
			p.subtitle,
			skin.subtitle,
			{
				.color = skin.subtitle_color,
				.font = p.font,
				.clip_rect = content_rect,
			}
		);
	}

	lo::skip(ctx, sty.section_spacing_below);
}