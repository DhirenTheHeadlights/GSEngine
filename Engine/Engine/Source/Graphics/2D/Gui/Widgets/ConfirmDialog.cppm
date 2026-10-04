export module gse.graphics:confirm_dialog;

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
import :types;

export namespace gse::gui {
	enum class confirm_result : std::uint8_t {
		pending,
		confirmed,
		cancelled,
	};

	struct confirm_params {
		rectf body;
		std::string_view title;
		std::string_view message;
		std::string_view confirm_label = "Confirm";
		std::string_view cancel_label = "Cancel";
		std::string_view key;
		bool danger = true;
	};
}

namespace gse::gui::draw {
	auto confirm_dialog(
		builder& ui,
		const confirm_params& params
	) -> confirm_result;
}

export namespace gse::gui {
	struct confirm_dialog {
		using result = confirm_result;
		using params = confirm_params;

		static auto draw(draw_context& ctx, const params& p, id& hot, id& active, id& focus) -> confirm_result {
			builder ui{
				.ctx = ctx,
				.hot_widget_id = hot,
				.active_widget_id = active,
				.focus_widget_id = focus,
			};
			return draw::confirm_dialog(ui, p);
		}
	};
}

auto gse::gui::draw::confirm_dialog(builder& ui, const confirm_params& params) -> confirm_result {
	const draw_context& ctx = ui.ctx;
	if (!ctx.fonts.text.valid()) {
		return confirm_result::pending;
	}

	const style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.25f;
	const float btn_h = text_view->line_height(fs) + pad;

	const float content_w = std::max({ text_view->width(params.title, fs), text_view->width(params.message, fs), 220.f });
	const float dialog_w = content_w + pad * 4.f;
	const float dialog_h = line_h * 2.f + btn_h + pad * 4.f;
	const vec2f center = params.body.center();
	const rectf dialog = rectf::from_position_size(
		{ center.x() - dialog_w * 0.5f, center.y() + dialog_h * 0.5f },
		{ dialog_w, dialog_h }
	);

	const auto _ = draw_modal_surface(ctx, {
		.host = params.body,
		.panel = dialog,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = params.title,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_h * 0.5f + text_view->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text,
		.clip_rect = dialog,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = params.message,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_h * 1.5f + text_view->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text_secondary,
		.clip_rect = dialog,
	});

	const float btn_w = (dialog_w - pad * 3.f) * 0.5f;
	const rectf cancel_btn = rectf::from_position_size({ dialog.left() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });
	const rectf confirm_btn = rectf::from_position_size({ cancel_btn.right() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });

	const std::string cancel_key = std::format("{}_cancel", params.key);
	const std::string confirm_key = std::format("{}_confirm", params.key);

	bool cancel = button_in_rect(ctx, {
		.rect = cancel_btn,
		.label = params.cancel_label,
		.key = cancel_key,
	}, ui.hot_widget_id, ui.active_widget_id);
	const bool confirm = button_in_rect(ctx, {
		.rect = confirm_btn,
		.label = params.confirm_label,
		.key = confirm_key,
		.role = params.danger ? button_role::danger : button_role::standard,
	}, ui.hot_widget_id, ui.active_widget_id);

	if (ctx.key_pressed_for(key::escape)) {
		ctx.consume_key_press(key::escape);
		cancel = true;
	}

	if (confirm) {
		return confirm_result::confirmed;
	}
	if (cancel) {
		return confirm_result::cancelled;
	}
	return confirm_result::pending;
}