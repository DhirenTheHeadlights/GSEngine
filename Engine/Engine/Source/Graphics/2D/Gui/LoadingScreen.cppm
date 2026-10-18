export module gse.graphics:loading_screen;

import gse.core;
import gse.log;
import gse.math;
import gse.time;
import std;

import :builder;
import :layout_ops;
import :loading;
import :menu_stack;
import :styles;
import :surface_paint;
import :types;
import :widget_skin;

export namespace gse::gui {
	class loading_screen : public screen {
	public:
		explicit loading_screen(
			const loading::state& state
		);

		auto build(
			builder& ui,
			nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto dismissable() const -> bool override;

		auto captures_input() const -> bool override;

		auto should_dismiss() const -> bool override;

	private:
		const loading::state* m_state;
		bool m_logged_first_build = false;
		interval_timer<float> m_log_timer{ milliseconds(250.f) };
	};
}

gse::gui::loading_screen::loading_screen(const loading::state& state) : m_state(&state) {
}

auto gse::gui::loading_screen::build(builder& ui, nav& n) -> void {
	if (!m_logged_first_build) {
		m_logged_first_build = true;
		log::println(log::category::runtime, "boot: loading_screen first build()");
	}

	const_cast<loading::state*>(m_state)->mark_rendered();

	const auto& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}

	const float pad = ctx.style.padding;
	const float font_sz = ctx.style.font_size;
	const rectf body = ctx.current_menu->rect.inset({ pad, pad });

	const auto text_view = ctx.fonts.text.resolve();
	const float row_h = text_view->line_height(font_sz);
	const vec2f center = body.center();

	const std::string phase = m_state->phase();
	const std::uint32_t done = m_state->done();
	const std::uint32_t total = m_state->total();

	if (m_log_timer.tick()) {
		log::println(
			log::category::runtime,
			"loading_screen::build read done={} total={} phase=\"{}\"",
			done,
			total,
			phase
		);
	}

	const std::string_view label = phase.empty() ? std::string_view("Loading...") : std::string_view(phase);

	draw::label(
		ctx,
		rectf::from_position_size({ body.left(), center.y() + row_h * 2.f }, { body.width(), row_h }),
		label,
		ctx.style.progress.label,
		{ .color = ctx.style.progress.label_color }
	);

	const float bar_width = std::min(ctx.style.progress_bar_max_width, body.width() * 0.6f);
	const float bar_height = ctx.style.progress_bar_height;
	const rectf bar_outline = layout::centered(
		body,
		{ bar_width, bar_height }
	);

	surface track = ctx.style.progress.track;
	track.radii = uniform_radii(bar_height * 0.5f);

	draw::fill_surface(ctx, bar_outline, track, {});

	const float inset = ctx.style.separator_thickness * 2.f;
	const float fill_ratio = total == 0 ? 0.f : std::clamp(static_cast<float>(done) / static_cast<float>(total),
		0.f,
		1.f);
	const float fill_width = (bar_width - inset * 2.f) * fill_ratio;

	if (fill_width > 0.f) {
		const rectf fill_rect = rectf::from_position_size(
			{ bar_outline.left() + inset, bar_outline.top() - inset },
			{ fill_width, bar_height - inset * 2.f }
		);
		surface bar_fill = ctx.style.progress.fill;
		bar_fill.radii = uniform_radii((bar_height - inset * 2.f) * 0.5f);

		draw::fill_surface(ctx, fill_rect, bar_fill, {});
	}

	if (total > 0) {
		draw::label(
			ctx,
			rectf::from_position_size(
				{ body.left(), center.y() - bar_height * 0.5f - pad + row_h * 0.5f },
				{ body.width(), row_h }
			),
			ctx.intern(std::format("{} / {}", done, total)),
			ctx.style.progress.label,
			{ .color = ctx.style.progress.label_color }
		);
	}
}

auto gse::gui::loading_screen::title() const -> std::string_view {
	return "Loading";
}

auto gse::gui::loading_screen::dismissable() const -> bool {
	return false;
}

auto gse::gui::loading_screen::captures_input() const -> bool {
	return true;
}

auto gse::gui::loading_screen::should_dismiss() const -> bool {
	return m_state->finished();
}