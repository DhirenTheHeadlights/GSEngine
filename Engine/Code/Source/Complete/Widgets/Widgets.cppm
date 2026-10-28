export module gse.complete.widgets;

import std;

import gse.assets;
import gse.core;
import gse.graphics;
import gse.math;
import gse.os;

export namespace gse::gui {
	struct completion_item {
		std::string_view label;
		std::string_view detail;
		std::string_view badge;
		vec4f accent;
		std::uint64_t matched = 0;
	};

	struct completion_state {
		std::size_t selected = 0;
		scroll_state scroll;
		bool reveal = true;
	};

	enum class completion_action : std::uint8_t {
		none,
		accept,
		dismiss
	};

	struct completion_result {
		completion_action action = completion_action::none;
		std::size_t index = 0;
	};

	struct completion_popup {
		using result = completion_result;

		struct params {
			std::span<const completion_item> items;
			completion_state& state;
			vec2f anchor;
			float line_height = 0.f;
			rectf bounds;
			resource::handle<font> font{};
			std::size_t max_visible = 10;
		};

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> completion_result;
	};

	struct ghost_text {
		using result = void;

		struct params {
			std::string_view text;
			vec2f anchor;
			float line_height = 0.f;
			rectf clip;
			resource::handle<font> font{};
		};

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> void;
	};

	struct signature_strip {
		using result = void;

		struct params {
			std::string_view label;
			std::uint32_t active_start = 0;
			std::uint32_t active_length = 0;
			std::string_view counter;
			vec2f anchor;
			float line_height = 0.f;
			rectf bounds;
			resource::handle<font> font{};
		};

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> void;
	};

	struct hover_card {
		using result = void;

		struct params {
			std::string_view title;
			std::span<const text_span> title_spans;
			std::string_view detail;
			std::string_view badge;
			vec4f accent;
			vec2f anchor;
			float line_height = 0.f;
			rectf bounds;
			resource::handle<font> font{};
		};

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> void;
	};
}

namespace gse::gui {
	constexpr float completion_min_width = 260.f;
	constexpr float completion_max_width = 640.f;
	constexpr float completion_detail_gap = 24.f;

	auto completion_keys(
		const draw_context& ctx,
		const completion_popup::params& p
	) -> completion_result;

	auto draw_matched_label(
		const draw_context& ctx,
		const font& face,
		resource::handle<font> handle,
		const completion_item& item,
		vec2f origin,
		const rectf& clip
	) -> void;

	auto draw_badge(
		const draw_context& ctx,
		const font& face,
		resource::handle<font> handle,
		std::string_view badge,
		vec4f accent,
		const rectf& row,
		const rectf& clip
	) -> void;

	auto draw_spanned(
		const draw_context& ctx,
		const font& face,
		resource::handle<font> handle,
		std::string_view text,
		std::span<const text_span> spans,
		vec2f origin,
		const rectf& clip
	) -> void;
}

auto gse::gui::completion_keys(const draw_context& ctx, const completion_popup::params& p) -> completion_result {
	const std::size_t count = p.items.size();
	const std::size_t page = std::max<std::size_t>(1, p.max_visible - 1);
	completion_state& state = p.state;
	const std::size_t before = state.selected;
	const auto moved = [&] {
		state.reveal = state.reveal || state.selected != before;
	};

	if (ctx.key_pressed_for(key::escape)) {
		return { .action = completion_action::dismiss };
	}
	if (ctx.key_pressed_for(key::down)) {
		state.selected = (state.selected + 1) % count;
	}
	if (ctx.key_pressed_for(key::up)) {
		state.selected = (state.selected + count - 1) % count;
	}
	if (ctx.key_pressed_for(key::page_down)) {
		state.selected = std::min(count - 1, state.selected + page);
	}
	if (ctx.key_pressed_for(key::page_up)) {
		state.selected = state.selected > page ? state.selected - page : 0;
	}
	moved();
	if (ctx.key_pressed_for(key::tab) || ctx.key_pressed_for(key::enter)) {
		return {
			.action = completion_action::accept,
			.index = state.selected,
		};
	}
	return {};
}

auto gse::gui::draw_matched_label(const draw_context& ctx, const font& face, const resource::handle<font> handle, const completion_item& item, const vec2f origin, const rectf& clip) -> void {
	const float scale = ctx.style.font_size;
	const std::string_view label = item.label;
	std::size_t start = 0;
	while (start < label.size()) {
		auto matched_at = [&](const std::size_t i) {
			return i < 64 && (item.matched >> i & 1) != 0;
		};
		const bool matched = matched_at(start);
		std::size_t end = start + 1;
		while (end < label.size() && matched_at(end) == matched) {
			++end;
		}
		ctx.queue_text({
			.font = handle,
			.text = label.substr(start, end - start),
			.position = { origin.x() + face.width(label.substr(0, start), scale), origin.y() },
			.scale = scale,
			.color = matched ? ctx.style.color_accent : ctx.style.color_text,
			.clip_rect = clip,
		});
		start = end;
	}
}

auto gse::gui::draw_badge(const draw_context& ctx, const font& face, const resource::handle<font> handle, const std::string_view badge, const vec4f accent, const rectf& row, const rectf& clip) -> void {
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = row.height();
	const rectf chip = rectf::from_position_size({ row.left() + pad * 0.5f, row.top() - row_h * 0.15f }, { row_h - pad * 0.5f, row_h * 0.7f });
	ctx.queue_sprite({
		.rect = chip,
		.color = { accent.x(), accent.y(), accent.z(), 0.22f },
		.texture = ctx.blank_texture,
		.clip_rect = clip,
		.corner_radius = 3.f,
	});
	ctx.queue_text({
		.font = handle,
		.text = badge,
		.position = { chip.center().x() - face.width(badge, scale) * 0.5f, row.center().y() + face.vertical_center_offset(scale) },
		.scale = scale,
		.color = accent,
		.clip_rect = clip,
	});
}

auto gse::gui::draw_spanned(const draw_context& ctx, const font& face, const resource::handle<font> handle, const std::string_view text, const std::span<const text_span> spans, const vec2f origin, const rectf& clip) -> void {
	const float scale = ctx.style.font_size;
	std::size_t at = 0;
	auto emit = [&](const std::size_t end, const vec4f color) {
		if (end <= at) {
			return;
		}
		ctx.queue_text({
			.font = handle,
			.text = text.substr(at, end - at),
			.position = { origin.x() + face.width(text.substr(0, at), scale), origin.y() },
			.scale = scale,
			.color = color,
			.clip_rect = clip,
		});
		at = end;
	};
	for (const text_span& span : spans) {
		emit(std::min<std::size_t>(span.start_col, text.size()), ctx.style.color_text);
		emit(std::min<std::size_t>(span.end_col, text.size()), span.color);
	}
	emit(text.size(), ctx.style.color_text);
}

auto gse::gui::completion_popup::draw(const draw_context& ctx, const params& p, id&, id&, id&) -> completion_result {
	const std::size_t count = p.items.size();
	if (count == 0) {
		return {};
	}
	completion_state& state = p.state;
	state.selected = std::min(state.selected, count - 1);

	completion_result result = completion_keys(ctx, p);
	if (result.action == completion_action::dismiss) {
		return result;
	}

	const auto fnt = ctx.fonts.face_or(p.font, text_face::code);
	const auto face = fnt.resolve();
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = p.line_height;
	const float badge_w = row_h;

	float label_w = 0.f;
	float detail_w = 0.f;
	for (const completion_item& item : p.items) {
		label_w = std::max(label_w, face->width(item.label, scale));
		detail_w = std::max(detail_w, face->width(item.detail, scale));
	}
	const float natural = badge_w + pad + label_w + (detail_w > 0.f ? completion_detail_gap + detail_w : 0.f) + pad * 2.f;
	const float width = std::clamp(natural, completion_min_width, std::min(completion_max_width, std::max(completion_min_width, p.bounds.width())));

	const std::size_t visible = std::min(count, p.max_visible);
	const float height = static_cast<float>(visible) * row_h;
	const float below_top = p.anchor.y() - row_h;
	const bool fits_below = below_top - height >= p.bounds.bottom();
	const float top = fits_below ? below_top : p.anchor.y() + height;
	const float left = std::clamp(p.anchor.x() - badge_w - pad, p.bounds.left(), std::max(p.bounds.left(), p.bounds.right() - width));
	const rectf list = rectf::from_position_size({ left, top }, { width, height });

	ctx.register_hit_region(render_layer::popup, list);
	const auto _ = ctx.scoped_layer(render_layer::popup);

	constexpr float border = 1.f;
	constexpr float shadow_offset = 4.f;
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ list.left() + shadow_offset, list.top() - shadow_offset }, list.size()),
		.color = ctx.style.modal.shadow.fill,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = list.inset({ -border, -border }),
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = list,
		.color = ctx.style.modal.panel.fill,
		.texture = ctx.blank_texture,
	});

	const float content_h = static_cast<float>(count) * row_h;
	const float selected_top = static_cast<float>(state.selected) * row_h;
	if (state.reveal && selected_top < state.scroll.y.offset) {
		state.scroll.y.offset = selected_top;
		state.scroll.y.target = selected_top;
	}
	else if (state.reveal && selected_top + row_h > state.scroll.y.offset + height) {
		state.scroll.y.offset = selected_top + row_h - height;
		state.scroll.y.target = state.scroll.y.offset;
	}
	state.reveal = false;

	const scroll_config cfg{
		.scrollbar_width = 6.f,
		.scroll_speed = row_h * 3.f,
		.smooth_factor = 0.2f,
		.auto_hide_scrollbar = true,
	};
	const float voff = scroll_area(ctx, state.scroll, list, { width, content_h }, cfg).y();
	const float center = face->vertical_center_offset(scale);

	const std::size_t first = std::min(count - 1, static_cast<std::size_t>(voff / row_h));
	const std::size_t last = std::min(count, first + visible + 1);
	for (std::size_t i = first; i < last; ++i) {
		const completion_item& item = p.items[i];
		const rectf row = rectf::from_position_size({ list.left(), list.top() - static_cast<float>(i) * row_h + voff }, { width, row_h });
		const bool hovered = ctx.hovers(row) && list.contains(ctx.mouse_position());

		if (i == state.selected || hovered) {
			ctx.queue_sprite({
				.rect = row,
				.color = i == state.selected ? ctx.style.dropdown.option.surfaces.selected.fill : ctx.style.dropdown.option.surfaces.hot.fill,
				.texture = ctx.blank_texture,
				.clip_rect = list,
			});
		}

		const float baseline = row.center().y() + center;
		draw_badge(ctx, *face, fnt, item.badge, item.accent, row, list);

		const float label_x = row.left() + badge_w + pad;
		draw_matched_label(ctx, *face, fnt, item, { label_x, baseline }, list);

		if (!item.detail.empty()) {
			const float detail_left = label_x + face->width(item.label, scale) + completion_detail_gap;
			const float detail_x = std::max(detail_left, row.right() - pad - face->width(item.detail, scale));
			ctx.queue_text({
				.font = fnt,
				.text = item.detail,
				.position = { detail_x, baseline },
				.scale = scale,
				.color = ctx.style.color_text_secondary,
				.clip_rect = rectf::from_position_size({ detail_left, row.top() }, { std::max(0.f, row.right() - pad - detail_left), row_h }).intersection(list),
			});
		}

		if (hovered && ctx.mouse_pressed_for(row)) {
			ctx.consume_press();
			state.selected = i;
			result = {
				.action = completion_action::accept,
				.index = i,
			};
		}
	}

	if (interaction::dismissed_by_outside_press(ctx, {
		.body = list,
		.suppressed = state.scroll.y.held,
	})) {
		return { .action = completion_action::dismiss };
	}
	return result;
}

auto gse::gui::ghost_text::draw(const draw_context& ctx, const params& p, id&, id&, id&) -> void {
	if (p.text.empty()) {
		return;
	}
	const auto fnt = ctx.fonts.face_or(p.font, text_face::code);
	const auto face = fnt.resolve();
	const float scale = ctx.style.font_size;
	const vec4f color = ctx.style.color_text_disabled;
	ctx.queue_text({
		.font = fnt,
		.text = p.text,
		.position = { p.anchor.x(), p.anchor.y() - p.line_height * 0.5f + face->vertical_center_offset(scale) },
		.scale = scale,
		.color = { color.x(), color.y(), color.z(), color.w() * 0.8f },
		.clip_rect = p.clip,
		.layer = render_layer::overlay,
	});
}

auto gse::gui::signature_strip::draw(const draw_context& ctx, const params& p, id&, id&, id&) -> void {
	if (p.label.empty()) {
		return;
	}
	const auto fnt = ctx.fonts.face_or(p.font, text_face::code);
	const auto face = fnt.resolve();
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = p.line_height;

	const float counter_w = p.counter.empty() ? 0.f : face->width(p.counter, scale) + pad;
	const float width = std::min(face->width(p.label, scale) + counter_w + pad * 2.f, std::max(0.f, p.bounds.width()));
	const float above_top = p.anchor.y() + row_h;
	const float top = above_top <= p.bounds.top() ? above_top : p.anchor.y() - row_h;
	const float left = std::clamp(p.anchor.x(), p.bounds.left(), std::max(p.bounds.left(), p.bounds.right() - width));
	const rectf strip = rectf::from_position_size({ left, top }, { width, row_h });

	const auto _ = ctx.scoped_layer(render_layer::popup);
	constexpr float border = 1.f;
	ctx.queue_sprite({
		.rect = strip.inset({ -border, -border }),
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = strip,
		.color = ctx.style.menu.title_bar.fill,
		.texture = ctx.blank_texture,
	});

	const float baseline = strip.center().y() + face->vertical_center_offset(scale);
	float x = strip.left() + pad;
	if (!p.counter.empty()) {
		ctx.queue_text({
			.font = fnt,
			.text = p.counter,
			.position = { x, baseline },
			.scale = scale,
			.color = ctx.style.color_text_disabled,
			.clip_rect = strip,
		});
		x += counter_w;
	}

	const std::size_t active_start = std::min<std::size_t>(p.active_start, p.label.size());
	const std::size_t active_end = std::min<std::size_t>(active_start + p.active_length, p.label.size());
	const std::array pieces{
		std::pair{ p.label.substr(0, active_start), ctx.style.color_text_secondary },
		std::pair{ p.label.substr(active_start, active_end - active_start), ctx.style.color_accent },
		std::pair{ p.label.substr(active_end), ctx.style.color_text_secondary },
	};
	for (const auto& [text, color] : pieces) {
		if (text.empty()) {
			continue;
		}
		ctx.queue_text({
			.font = fnt,
			.text = text,
			.position = { x, baseline },
			.scale = scale,
			.color = color,
			.clip_rect = strip,
		});
		x += face->width(text, scale);
	}
}

auto gse::gui::hover_card::draw(const draw_context& ctx, const params& p, id&, id&, id&) -> void {
	const auto fnt = ctx.fonts.face_or(p.font, text_face::code);
	const auto face = fnt.resolve();
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float row_h = p.line_height;
	const float badge_w = row_h;

	const float text_w = std::max(face->width(p.title, scale), face->width(p.detail, scale));
	const float width = std::min(badge_w + pad + text_w + pad * 2.f, std::min(completion_max_width, std::max(0.f, p.bounds.width())));
	const float height = row_h * (p.detail.empty() ? 1.f : 2.f);
	const float above_top = p.anchor.y() + height;
	const float top = above_top <= p.bounds.top() ? above_top : p.anchor.y() - row_h;
	const float left = std::clamp(p.anchor.x() - badge_w - pad, p.bounds.left(), std::max(p.bounds.left(), p.bounds.right() - width));
	const rectf card = rectf::from_position_size({ left, top }, { width, height });

	const auto _ = ctx.scoped_layer(render_layer::popup);
	constexpr float border = 1.f;
	ctx.queue_sprite({
		.rect = card.inset({ -border, -border }),
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = card,
		.color = ctx.style.menu.title_bar.fill,
		.texture = ctx.blank_texture,
	});

	const float center = face->vertical_center_offset(scale);
	const float text_x = card.left() + badge_w + pad;
	const rectf title_row = rectf::from_position_size({ card.left(), card.top() }, { width, row_h });
	draw_badge(ctx, *face, fnt, p.badge, p.accent, title_row, card);
	draw_spanned(ctx, *face, fnt, p.title, p.title_spans, { text_x, title_row.center().y() + center }, card);

	if (p.detail.empty()) {
		return;
	}
	ctx.queue_text({
		.font = fnt,
		.text = p.detail,
		.position = { text_x, title_row.center().y() - row_h + center },
		.scale = scale,
		.color = ctx.style.color_text_secondary,
		.clip_rect = card,
	});
}
