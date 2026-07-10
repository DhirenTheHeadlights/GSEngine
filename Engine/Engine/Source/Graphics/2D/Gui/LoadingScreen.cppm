export module gse.graphics:loading_screen;

import std;

import gse.core;
import gse.math;
import gse.log;
import gse.time;

import :menu_stack;
import :builder;
import :types;
import :styles;
import :layout_ops;
import :text_widget;
import :loading;

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
	const ui_rect body = ctx.current_menu->rect.inset({ pad, pad });

	const float row_h = ctx.font->line_height(font_sz);
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

	const std::string label = phase.empty() ? std::string("Loading...") : phase;
	const float label_w = ctx.font->width(label, font_sz);

	ctx.queue_text({
		.font = ctx.font,
		.text = label,
		.position = { center.x() - label_w * 0.5f, center.y() + ctx.font->vertical_center_offset(font_sz) + row_h * 1.5f },
		.scale = font_sz,
		.color = ctx.style.color_text,
	});

	const float bar_width = std::min(ctx.style.progress_bar_max_width, body.width() * 0.6f);
	const float bar_height = ctx.style.progress_bar_height;
	const ui_rect bar_outline = layout::centered(
		body,
		{ bar_width, bar_height }
	);

	ctx.queue_sprite({
		.rect = bar_outline,
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
		.corner_radius = bar_height * 0.5f,
	});

	const float inset = ctx.style.separator_thickness * 2.f;
	const float fill_ratio = total == 0 ? 0.f : std::clamp(static_cast<float>(done) / static_cast<float>(total),
														   0.f,
														   1.f);
	const float fill_width = (bar_width - inset * 2.f) * fill_ratio;

	if (fill_width > 0.f) {
		const ui_rect fill_rect = ui_rect::from_position_size(
			{ bar_outline.left() + inset, bar_outline.top() - inset },
			{ fill_width, bar_height - inset * 2.f }
		);
		ctx.queue_sprite({
			.rect = fill_rect,
			.color = ctx.style.color_widget_active,
			.texture = ctx.blank_texture,
			.corner_radius = (bar_height - inset * 2.f) * 0.5f,
		});
	}

	if (total > 0) {
		const std::string detail = std::format("{} / {}", done, total);
		const float detail_w = ctx.font->width(detail, font_sz);
		ctx.queue_text({
			.font = ctx.font,
			.text = detail,
			.position = { center.x() - detail_w * 0.5f, center.y() - bar_height * 0.5f - pad },
			.scale = font_sz,
			.color = ctx.style.color_text,
		});
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
