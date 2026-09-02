export module gse.graphics:marquee_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import :types;
import :font;
import :ids;
import :styles;
import :builder;

export namespace gse::gui {
	struct marquee_info {
		std::string_view text;
		rectf area;
		vec4f color;
		float speed = 36.f;
		float gap = 40.f;
		bool scrolling = true;
		resource::handle<font> font{};
	};
}

namespace gse::gui::draw {
	auto marquee_text(
		const draw_context& ctx,
		const marquee_info& info
	) -> void;
}

export namespace gse::gui {
	struct marquee {
		using result = void;
		using params = marquee_info;

		static auto draw(const draw_context& ctx, const params& p, id&, id&, id&) -> void {
			draw::marquee_text(ctx, p);
		}
	};
}

auto gse::gui::draw::marquee_text(const draw_context& ctx, const marquee_info& info) -> void {
	const auto fnt = info.font.valid() ? info.font : ctx.fonts.text;
	const auto fnt_view = fnt.resolve();
	const float baseline = info.area.center().y() + fnt_view->vertical_center_offset(ctx.style.font_size);
	const float width = fnt_view->width(info.text, ctx.style.font_size);

	if (!info.scrolling || width <= info.area.width()) {
		ctx.queue_text({
			.font = fnt,
			.text = info.text,
			.position = { info.area.left(), baseline },
			.scale = ctx.style.font_size,
			.color = info.color,
			.clip_rect = info.area,
		});
		return;
	}

	const float span = width + info.gap;
	const double elapsed = system_clock::now<time_t<double>>().as<seconds>();
	const float offset = static_cast<float>(std::fmod(elapsed * info.speed, static_cast<double>(span)));

	for (const float shift : { 0.f, span }) {
		ctx.queue_text({
			.font = fnt,
			.text = info.text,
			.position = { info.area.left() - offset + shift, baseline },
			.scale = ctx.style.font_size,
			.color = info.color,
			.clip_rect = info.area,
		});
	}
}
