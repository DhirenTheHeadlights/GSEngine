export module gse.graphics:prompt_dialog;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.os;
import gse.time;
import std;

import :builder;
import :button_widget;
import :font;
import :modal_surface;
import :render_layer;
import :styles;
import :surface_paint;
import :text_input_widget;
import :types;
import :widget_skin;

export namespace gse::gui {
	enum class prompt_result : std::uint8_t {
		pending,
		submitted,
		cancelled,
	};

	struct prompt_params {
		rectf body;
		std::string_view title;
		std::string_view message;
		std::string& value;
		text_input_state& state;
		std::string_view submit_label = "Create";
		std::string_view cancel_label = "Cancel";
		std::string_view key;
	};
}

namespace gse::gui::draw {
	auto prompt_dialog(
		builder& ui,
		const prompt_params& params
	) -> prompt_result;
}

export namespace gse::gui {
	struct prompt_dialog {
		using result = prompt_result;
		using params = prompt_params;

		static auto draw(
			draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> prompt_result;
	};
}

auto gse::gui::prompt_dialog::draw(draw_context& ctx, const params& p, id& hot, id& active, id& focus) -> prompt_result {
	builder ui{
		.ctx = ctx,
		.hot_widget_id = hot,
		.active_widget_id = active,
		.focus_widget_id = focus,
	};
	return draw::prompt_dialog(ui, p);
}

auto gse::gui::draw::prompt_dialog(builder& ui, const prompt_params& params) -> prompt_result {
	const draw_context& ctx = ui.ctx;
	if (!ctx.fonts.text.valid()) {
		return prompt_result::pending;
	}

	const style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.25f;
	const float btn_h = text_view->line_height(fs) + pad;
	const float field_h = text_view->line_height(fs) + pad;

	const float content_w = std::max({ text_view->width(params.title, fs), text_view->width(params.message, fs), 280.f });
	const float dialog_w = content_w + pad * 4.f;
	const float dialog_h = line_h * 2.f + field_h + btn_h + pad * 5.f;
	const vec2f center = params.body.center();
	const rectf dialog = rectf::from_position_size(
		{ center.x() - dialog_w * 0.5f, center.y() + dialog_h * 0.5f },
		{ dialog_w, dialog_h }
	);

	const auto _ = draw_modal_surface(ctx, {
		.host = params.body,
		.panel = dialog,
	});

	const modal_skin& skin = sty.modal;
	const float text_left = dialog.left() + pad * 2.f;
	const float text_width = std::max(0.f, dialog.right() - pad * 2.f - text_left);

	draw::label(
		ctx,
		rectf::from_position_size({ text_left, dialog.top() - pad * 2.f }, { text_width, line_h }),
		params.title,
		skin.title,
		{
			.color = skin.title_color,
			.clip_rect = dialog,
		}
	);

	draw::label(
		ctx,
		rectf::from_position_size({ text_left, dialog.top() - pad * 2.f - line_h }, { text_width, line_h }),
		params.message,
		skin.message,
		{
			.color = skin.message_color,
			.clip_rect = dialog,
		}
	);

	const id field_id = ids::make(std::format("{}_field", params.key));
	const rectf field = rectf::from_position_size(
		{ dialog.left() + pad * 2.f, dialog.top() - pad * 3.f - line_h * 2.f },
		{ dialog_w - pad * 4.f, field_h }
	);
	text_input_in_rect(ctx, field_id, params.value, params.state, field, ui.hot_widget_id, ui.focus_widget_id);
	if (ui.focus_widget_id != field_id) {
		ui.focus_widget_id = field_id;
	}

	const float btn_w = (dialog_w - pad * 3.f) * 0.5f;
	const rectf cancel_btn = rectf::from_position_size({ dialog.left() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });
	const rectf submit_btn = rectf::from_position_size({ cancel_btn.right() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });

	const std::string cancel_key = std::format("{}_cancel", params.key);
	const std::string submit_key = std::format("{}_submit", params.key);
	const bool named = !params.value.empty();

	bool cancel = button_in_rect(ctx, {
		.rect = cancel_btn,
		.label = params.cancel_label,
		.key = cancel_key,
	}, ui.hot_widget_id, ui.active_widget_id);
	bool submit = button_in_rect(ctx, {
		.rect = submit_btn,
		.label = params.submit_label,
		.key = submit_key,
		.enabled = named,
	}, ui.hot_widget_id, ui.active_widget_id);

	if (ctx.key_pressed_for(key::escape)) {
		ctx.consume_key_press(key::escape);
		cancel = true;
	}
	if (ctx.key_pressed_for(key::enter) && named) {
		ctx.consume_key_press(key::enter);
		submit = true;
	}

	if (submit) {
		return prompt_result::submitted;
	}
	if (cancel) {
		return prompt_result::cancelled;
	}
	return prompt_result::pending;
}
