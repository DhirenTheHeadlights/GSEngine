export module gse.graphics:image_attachments;

import gse.assert;
import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.os;
import gse.time;
import std;

import :builder;
import :button_widget;
import :font;
import :ids;
import :interaction;
import :modal_surface;
import :render_layer;
import :styles;
import :symbols;
import :texture;
import :types;

export namespace gse::gui {
	struct image_strip_params {
		image_attachments& attachments;
		rectf strip;
		rectf viewer_host;
	};

	[[nodiscard]] auto image_strip_extent(
		const draw_context& ctx,
		const image_attachments& images
	) -> float;
}

namespace gse::gui {
	inline std::uint32_t pasted_image_count = 0;

	auto draw_image_attachments(
		const draw_context& ctx,
		id& hot_widget_id,
		id& active_widget_id,
		const image_strip_params& strip,
		id widget_id
	) -> void;

	auto resolve_image_paste(
		image_paste_state& paste,
		shared_view<asset::data> assets
	) -> void;

	constexpr float min_thumbnail_scale = 0.5f;
	constexpr float max_thumbnail_scale = 2.5f;

	[[nodiscard]] auto draw_image_viewer(
		const draw_context& ctx,
		id& hot_widget_id,
		id& active_widget_id,
		image_attachments& images,
		const rectf& host,
		id widget_id
	) -> std::optional<std::size_t>;

	[[nodiscard]] auto thumbnail_extent(
		const rectf& area,
		float pad
	) -> float;

	[[nodiscard]] auto thumbnail_width(
		float extent,
		vec2u size,
		float max_scale
	) -> float;

	[[nodiscard]] auto thumbnail_rect(
		const rectf& area,
		float left,
		vec2u size,
		float pad,
		float max_scale
	) -> rectf;

	[[nodiscard]] auto thumbnail_row_width(
		float extent,
		const image_attachments& images,
		float pad,
		float max_scale
	) -> float;

	[[nodiscard]] auto fitted_row_scale(
		const rectf& area,
		const image_attachments& images,
		float pad
	) -> float;

	[[nodiscard]] auto fitted_rect(
		const rectf& area,
		vec2u size
	) -> rectf;

	[[nodiscard]] auto close_button_rect(
		const rectf& frame,
		const style& sty
	) -> rectf;

	[[nodiscard]] auto encoding_for(
		const std::filesystem::path& path
	) -> std::optional<image_encoding>;
}

auto gse::gui::image_strip_extent(const draw_context& ctx, const image_attachments& images) -> float {
	if (images.items.empty()) {
		return 0.f;
	}
	return ctx.style.font_size * 3.f + ctx.style.padding * 2.f;
}

auto gse::gui::thumbnail_extent(const rectf& area, const float pad) -> float {
	return std::max(0.f, area.height() - pad * 2.f);
}

auto gse::gui::thumbnail_width(const float extent, const vec2u size, const float max_scale) -> float {
	const float aspect = static_cast<float>(size.x()) / static_cast<float>(size.y());
	return std::clamp(extent * aspect, extent * min_thumbnail_scale, extent * max_scale);
}

auto gse::gui::thumbnail_rect(const rectf& area, const float left, const vec2u size, const float pad, const float max_scale) -> rectf {
	const float extent = thumbnail_extent(area, pad);
	return rectf::from_position_size({ left, area.top() - pad }, { thumbnail_width(extent, size, max_scale), extent });
}

auto gse::gui::thumbnail_row_width(const float extent, const image_attachments& images, const float pad, const float max_scale) -> float {
	float total = pad;
	for (const image_attachment& item : images.items) {
		total += thumbnail_width(extent, item.size, max_scale) + pad;
	}
	return total;
}

auto gse::gui::fitted_row_scale(const rectf& area, const image_attachments& images, const float pad) -> float {
	const float extent = thumbnail_extent(area, pad);
	if (thumbnail_row_width(extent, images, pad, max_thumbnail_scale) <= area.width()) {
		return max_thumbnail_scale;
	}

	float lo = min_thumbnail_scale;
	float hi = max_thumbnail_scale;
	for (int step = 0; step < 12; ++step) {
		const float mid = (lo + hi) * 0.5f;
		if (thumbnail_row_width(extent, images, pad, mid) <= area.width()) {
			lo = mid;
		}
		else {
			hi = mid;
		}
	}
	return lo;
}

auto gse::gui::close_button_rect(const rectf& frame, const style& sty) -> rectf {
	const float inset = sty.padding * 0.5f;
	const float extent = std::min(sty.icon_extent, frame.height() * 0.5f);
	return rectf::from_position_size(
		{ frame.right() - extent - inset, frame.top() - inset },
		{ extent, extent }
	);
}

auto gse::gui::fitted_rect(const rectf& area, const vec2u size) -> rectf {
	const float aspect = static_cast<float>(size.x()) / static_cast<float>(size.y());
	const float width = std::min(area.width(), area.height() * aspect);
	const float height = width / aspect;
	const vec2f center = area.center();
	return rectf::from_position_size({ center.x() - width * 0.5f, center.y() + height * 0.5f }, { width, height });
}

auto gse::gui::draw_image_attachments(const draw_context& ctx, id& hot_widget_id, id& active_widget_id, const image_strip_params& strip, const id widget_id) -> void {
	image_attachments& images = strip.attachments;
	const rectf& area = strip.strip;
	const style& sty = ctx.style;
	const float pad = sty.padding;
	const auto text_view = ctx.fonts.text.resolve();

	assert(!images.viewing || *images.viewing < images.items.size(), "image_attachments::viewing is {} with {} items", images.viewing.value_or(0), images.items.size());

	if (images.items.empty()) {
		return;
	}

	const float scale = fitted_row_scale(area, images, pad);
	std::size_t removed = images.items.size();
	float left = area.left() + pad;

	for (std::size_t i = 0; i < images.items.size(); ++i) {
		const rectf frame = thumbnail_rect(area, left, images.items[i].size, pad, scale);

		if (frame.right() > area.right() - pad) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = ctx.intern(std::format("+{}", images.items.size() - i)),
				.position = { left, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.color = sty.color_text_secondary,
				.clip_rect = area,
			});
			break;
		}

		const id thumb_id = ids::make_from_key(hash_combine(widget_id.number(), static_cast<std::uint64_t>(i)));
		const id close_id = ids::make_from_key(hash_combine(thumb_id.number(), stable_id("close")));
		const rectf close = close_button_rect(frame, sty);

		const interaction::press discard = interaction::press_in_rect(ctx, hot_widget_id, active_widget_id, close_id, close);
		const interaction::press open = interaction::press_in_rect(ctx, hot_widget_id, active_widget_id, thumb_id, frame);

		ctx.queue_sprite({
			.rect = frame,
			.color = open.hovered ? sty.color_tab_hovered : sty.color_tab_background,
			.texture = ctx.blank_texture,
			.clip_rect = area,
			.corner_radius = sty.corner_radius,
		});
		ctx.queue_sprite({
			.rect = fitted_rect(frame, images.items[i].size),
			.texture = images.items[i].preview,
			.clip_rect = area,
			.corner_radius = sty.corner_radius,
		});

		draw_icon_button(ctx, {
			.rect = close,
			.glyph = symbol::close(),
			.background = ctx.animated_color(close_id, discard.color({
				.idle = sty.color_tab_background,
				.hot = sty.color_danger_hovered,
				.active = sty.color_danger_hovered,
				.disabled = sty.color_tab_background,
			})),
			.icon = discard.hovered ? sty.color_icon_hovered : sty.color_icon,
			.corner_radius = close.width() * 0.5f,
			.clip_rect = area,
		});

		if (discard.activated) {
			removed = i;
		}
		if (open.activated) {
			images.viewing = i;
		}

		left = frame.right() + pad;
	}

	if (images.viewing) {
		if (const std::optional<std::size_t> discarded = draw_image_viewer(ctx, hot_widget_id, active_widget_id, images, strip.viewer_host, widget_id)) {
			removed = *discarded;
		}
	}

	if (removed < images.items.size()) {
		images.remove(removed);
	}
}

auto gse::gui::draw_image_viewer(const draw_context& ctx, id& hot_widget_id, id& active_widget_id, image_attachments& images, const rectf& host, const id widget_id) -> std::optional<std::size_t> {
	const style& sty = ctx.style;
	const std::size_t shown_index = *images.viewing;
	const image_attachment& shown = images.items[shown_index];
	const float pad = sty.padding;
	const auto text_view = ctx.fonts.text.resolve();

	const float header = text_view->line_height(sty.font_size) + pad * 2.f;
	const rectf body = host.inset({ host.width() * 0.08f, host.height() * 0.08f });
	const rectf content = rectf::from_position_size(
		{ body.left() + pad, body.top() - header },
		{ std::max(0.f, body.width() - pad * 2.f), std::max(0.f, body.height() - header - pad) }
	);

	const auto _ = draw_modal_surface(ctx, {
		.host = host,
		.panel = body,
	});
	ctx.queue_sprite({
		.rect = fitted_rect(content, shown.size),
		.texture = shown.preview,
		.clip_rect = content,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = ctx.intern(std::format("{}  {}", shown.path.filename().display_string(), shown.size)),
		.position = { body.left() + pad, body.top() - header * 0.5f + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = body,
	});

	const float chip_extent = header * 0.5f;
	const rectf close = rectf::from_position_size(
		{ body.right() - pad - chip_extent, body.top() - pad * 0.5f },
		{ chip_extent, chip_extent }
	);
	const rectf discard = rectf::from_position_size(
		{ close.left() - pad * 0.5f - chip_extent, close.top() },
		{ chip_extent, chip_extent }
	);

	const id close_id = ids::make_from_key(hash_combine(widget_id.number(), stable_id("viewer_close")));
	const id discard_id = ids::make_from_key(hash_combine(widget_id.number(), stable_id("viewer_discard")));

	const interaction::press close_press = interaction::press_in_rect(ctx, hot_widget_id, active_widget_id, close_id, close);
	const interaction::press discard_press = interaction::press_in_rect(ctx, hot_widget_id, active_widget_id, discard_id, discard);

	draw_icon_button(ctx, {
		.rect = close,
		.glyph = symbol::close(),
		.background = ctx.animated_color(close_id, close_press.color({
			.idle = sty.color_tab_background,
			.hot = sty.color_tab_hovered,
			.active = sty.color_tab_hovered,
			.disabled = sty.color_tab_background,
		})),
		.icon = close_press.hovered ? sty.color_icon_hovered : sty.color_icon,
		.corner_radius = close.width() * 0.5f,
		.clip_rect = body,
	});

	draw_icon_button(ctx, {
		.rect = discard,
		.glyph = symbol::trash(),
		.background = ctx.animated_color(discard_id, discard_press.color({
			.idle = sty.color_tab_background,
			.hot = sty.color_danger_hovered,
			.active = sty.color_danger_hovered,
			.disabled = sty.color_tab_background,
		})),
		.icon = discard_press.hovered ? sty.color_icon_hovered : sty.color_icon,
		.corner_radius = discard.width() * 0.5f,
		.clip_rect = body,
	});

	const bool dismissed = interaction::dismissed_by_outside_press(ctx, { .body = body });
	const bool escaped = ctx.key_pressed_for(key::escape);

	if (escaped || dismissed || close_press.activated) {
		images.viewing.reset();
	}

	return discard_press.activated ? std::optional{ shown_index } : std::nullopt;
}

auto gse::gui::encoding_for(const std::filesystem::path& path) -> std::optional<image_encoding> {
	std::string extension = path.extension().native_encoded_string();
	std::ranges::transform(extension, extension.begin(), [](const unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	if (extension == ".png") {
		return image_encoding::png;
	}
	if (extension == ".jpg" || extension == ".jpeg") {
		return image_encoding::jpeg;
	}
	if (extension == ".gif") {
		return image_encoding::gif;
	}
	if (extension == ".webp") {
		return image_encoding::webp;
	}
	return std::nullopt;
}
