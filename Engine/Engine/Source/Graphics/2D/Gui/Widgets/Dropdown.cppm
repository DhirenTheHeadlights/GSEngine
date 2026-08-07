export module gse.graphics:dropdown_widget;

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
import :render_layer;
import :interaction;
import :symbols;

export namespace gse::gui {
	struct dropdown_state {
		id open_dropdown_id;
		scroll_state scroll;
	};

	struct dropdown_result {
		bool changed = false;
		std::size_t new_index = 0;
	};

	struct dropdown_config {
		std::size_t max_visible_items = 8;
		float scrollbar_width = 6.f;
	};
}

export namespace gse::gui::draw {
	auto dropdown(
		const draw_context& ctx,
		std::string_view name,
		std::size_t current_index,
		std::span<const std::string_view> options,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {}
	) -> dropdown_result;

	auto dropdown(
		const draw_context& ctx,
		std::string_view name,
		std::size_t current_index,
		std::span<const std::string> options,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {},
		resource::handle<font> font = {}
	) -> dropdown_result;

	auto dropdown_in_rect(
		const draw_context& ctx,
		std::string_view name,
		std::size_t current_index,
		std::span<const std::string_view> options,
		dropdown_state& state,
		const rectf& header_rect,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {}
	) -> dropdown_result;

	auto dropdown_in_rect(
		const draw_context& ctx,
		std::string_view name,
		std::size_t current_index,
		std::span<const std::string> options,
		dropdown_state& state,
		const rectf& header_rect,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {}
	) -> dropdown_result;

	auto dropdown_in_rect_keyed(
		const draw_context& ctx,
		std::uint64_t widget_key,
		std::size_t current_index,
		std::span<const std::string> options,
		dropdown_state& state,
		const rectf& header_rect,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {}
	) -> dropdown_result;
}

export namespace gse::gui {
	struct dropdown {
		using result = dropdown_result;
		struct params {
			std::string_view name;
			std::size_t& current_index;
			std::span<const std::string> options;
			dropdown_state& state;
			dropdown_config config = {};
			resource::handle<font> font{};
		};
		static auto draw(const draw_context& ctx, const params& p, id& hot, id& active, id&) -> dropdown_result {
			auto r = draw::dropdown(ctx, p.name, p.current_index, p.options, p.state, hot,
									active,
									p.config, p.font);
			if (r.changed) {
				p.current_index = r.new_index;
			}
			return r;
		}
	};
}

namespace gse::gui::draw {
	auto dropdown_impl(
		const draw_context& ctx,
		std::string_view name,
		std::size_t current_index,
		const std::function<std::size_t()>& option_count,
		const std::function<std::string_view(std::size_t)>& get_option,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config,
		resource::handle<font> font = {}
	) -> dropdown_result;

	auto dropdown_impl_in_rect(
		const draw_context& ctx,
		id dropdown_id,
		std::size_t current_index,
		const std::function<std::size_t()>& option_count,
		const std::function<std::string_view(std::size_t)>& get_option,
		dropdown_state& state,
		const rectf& header_rect,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config,
		resource::handle<font> font = {}
	) -> dropdown_result;
}

auto gse::gui::draw::dropdown(const draw_context& ctx, const std::string_view name, const std::size_t current_index, const std::span<const std::string_view> options, dropdown_state& state, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	return dropdown_impl(
		ctx,
		name,
		current_index,
		[&]() {
			return options.size();
		},
		[&](const std::size_t i) {
			return options[i];
		},
		state,
		hot_widget_id,
		active_widget_id,
		config
	);
}

auto gse::gui::draw::dropdown(const draw_context& ctx, const std::string_view name, const std::size_t current_index, const std::span<const std::string> options, dropdown_state& state, id& hot_widget_id, id& active_widget_id, const dropdown_config& config, const resource::handle<font> font) -> dropdown_result {
	return dropdown_impl(
		ctx,
		name,
		current_index,
		[&] {
			return options.size();
		},
		[&](const std::size_t i) -> std::string_view {
			return options[i];
		},
		state,
		hot_widget_id,
		active_widget_id,
		config,
		font
	);
}

auto gse::gui::draw::dropdown_in_rect(const draw_context& ctx, const std::string_view name, const std::size_t current_index, const std::span<const std::string_view> options, dropdown_state& state, const rectf& header_rect, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	return dropdown_impl_in_rect(
		ctx,
		ids::make(name),
		current_index,
		[&] {
			return options.size();
		},
		[&](const std::size_t i) {
			return options[i];
		},
		state,
		header_rect,
		hot_widget_id,
		active_widget_id,
		config
	);
}

auto gse::gui::draw::dropdown_in_rect(const draw_context& ctx, const std::string_view name, const std::size_t current_index, const std::span<const std::string> options, dropdown_state& state, const rectf& header_rect, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	return dropdown_impl_in_rect(
		ctx,
		ids::make(name),
		current_index,
		[&] {
			return options.size();
		},
		[&](const std::size_t i) -> std::string_view {
			return options[i];
		},
		state,
		header_rect,
		hot_widget_id,
		active_widget_id,
		config
	);
}

auto gse::gui::draw::dropdown_in_rect_keyed(const draw_context& ctx, const std::uint64_t widget_key, const std::size_t current_index, const std::span<const std::string> options, dropdown_state& state, const rectf& header_rect, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	return dropdown_impl_in_rect(
		ctx,
		ids::make_from_key(widget_key),
		current_index,
		[&] {
			return options.size();
		},
		[&](const std::size_t i) -> std::string_view {
			return options[i];
		},
		state,
		header_rect,
		hot_widget_id,
		active_widget_id,
		config
	);
}

auto gse::gui::draw::dropdown_impl(const draw_context& ctx, const std::string_view name, const std::size_t current_index, const std::function<std::size_t()>& option_count, const std::function<std::string_view(std::size_t)>& get_option, dropdown_state& state, id& hot_widget_id, id& active_widget_id, const dropdown_config& config, const resource::handle<font> font) -> dropdown_result {
	if (!ctx.current_menu) {
		return {};
	}

	const auto text_view = ctx.fonts.text.resolve();
	const float row_height = text_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const float label_width = content_rect.width() * 0.4f;

	const rectf label_rect =
		rectf::from_position_size(
			{ content_rect.left(), ctx.layout_cursor.y() },
			{ label_width, row_height }
		);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = name,
		.position = { label_rect.left(), label_rect.center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = label_rect,
	});

	const rectf header_rect = rectf::from_position_size(
		{ content_rect.left() + label_width, ctx.layout_cursor.y() },
		{ content_rect.width() - label_width, row_height }
	);

	const auto result = dropdown_impl_in_rect(
		ctx,
		ids::make(name),
		current_index,
		option_count,
		get_option,
		state,
		header_rect,
		hot_widget_id,
		active_widget_id,
		config,
		font
	);

	ctx.layout_cursor.y() -= row_height + ctx.style.padding;

	return result;
}

auto gse::gui::draw::dropdown_impl_in_rect(const draw_context& ctx, const id dropdown_id, const std::size_t current_index, const std::function<std::size_t()>& option_count, const std::function<std::string_view(std::size_t)>& get_option, dropdown_state& state, const rectf& header_rect, id& hot_widget_id, id& active_widget_id, const dropdown_config& config, const resource::handle<font> font) -> dropdown_result {
	const auto fnt = font.valid() ? font : ctx.fonts.text;
	const auto fnt_view = fnt.resolve();
	if (!ctx.current_menu) {
		return {};
	}

	const bool is_open = state.open_dropdown_id == dropdown_id;
	dropdown_result result;

	const float row_height = header_rect.height();
	const std::size_t count = option_count();
	const float max_option_width = header_rect.width();

	const std::size_t visible_count_early = std::min(count, config.max_visible_items);
	const float visible_height_early = static_cast<float>(visible_count_early) * row_height;
	const bool needs_scroll_early = count > config.max_visible_items;
	const float list_width_early = needs_scroll_early ? max_option_width + config.scrollbar_width : max_option_width;
	const rectf list_rect_early = rectf::from_position_size(
		{ header_rect.left(), header_rect.bottom() },
		{ list_width_early, visible_height_early }
	);

	if (is_open && count > 0) {
		ctx.register_hit_region(render_layer::modal, list_rect_early);
	}

	const bool released = ctx.mouse_released();
	const bool header_pressed_for_me = ctx.mouse_pressed_for(header_rect);
	const bool header_hovered = ctx.hovers(header_rect);

	interaction::mark_hot(hot_widget_id, dropdown_id, header_hovered);

	const bool released_by_me = interaction::activate_on_click(active_widget_id, dropdown_id, header_hovered, header_pressed_for_me, released);
	if (released_by_me && header_hovered) {
		if (is_open) {
			state.open_dropdown_id.reset();
		}
		else {
			state.open_dropdown_id = dropdown_id;
			state.scroll = {};
		}
	}

	vec4f header_bg = ctx.style.color_widget_background;
	if (is_open) {
		header_bg = ctx.style.color_widget_active;
	}
	else if (active_widget_id == dropdown_id) {
		header_bg = ctx.style.color_widget_active;
	}
	else if (header_hovered) {
		header_bg = ctx.style.color_widget_hovered;
	}

	ctx.queue_sprite({
		.rect = header_rect,
		.color = header_bg,
		.texture = ctx.blank_texture
	});

	const std::string_view current_label = current_index < count ? get_option(current_index) : std::string_view{};

	const float text_x = header_rect.left() + ctx.style.padding;

	ctx.queue_text({
		.font = fnt,
		.text = current_label,
		.position = { text_x, header_rect.center().y() + fnt_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = header_rect
	});

	const float arrow_size = ctx.style.font_size;
	const rectf arrow_rect = rectf::from_position_size(
		{ header_rect.right() - arrow_size - ctx.style.padding, header_rect.top() },
		{ arrow_size, header_rect.height() }
	);
	symbol::draw(ctx, is_open ? symbol::chevron_up() : symbol::chevron_down(), arrow_rect, {
		.color = ctx.style.color_text_secondary,
		.extent = ctx.style.icon_extent,
	});

	if (is_open && count > 0) {
		const auto modal_layer = ctx.scoped_layer(render_layer::modal);

		const std::size_t visible_count = std::min(count, config.max_visible_items);
		const float visible_height = static_cast<float>(visible_count) * row_height;
		const float total_content_height = static_cast<float>(count) * row_height;
		const bool needs_scroll = count > config.max_visible_items;

		const float list_width = needs_scroll ? max_option_width + config.scrollbar_width : max_option_width;

		const rectf list_rect =
			rectf::from_position_size(
				{ header_rect.left(), header_rect.bottom() },
				{ list_width, visible_height }
			);

		const rectf content_area = needs_scroll
			? rectf::from_position_size(
				  list_rect.top_left(),
				  { max_option_width, visible_height }
			  )
			: list_rect;

		constexpr float border = 1.f;
		ctx.queue_sprite({
			.rect = list_rect.inset({ -border, -border }),
			.color = ctx.style.color_border,
			.texture = ctx.blank_texture,
		});

		ctx.queue_sprite({
			.rect = list_rect,
			.color = ctx.style.color_panel_alt,
			.texture = ctx.blank_texture,
		});

		const scroll_config list_scroll_cfg{
			.scrollbar_width = config.scrollbar_width,
			.scroll_speed = row_height * 2.f,
			.smooth_factor = 0.2f,
			.auto_hide_scrollbar = false,
		};

		const float voff = scroll_area(ctx, state.scroll, list_rect, { list_rect.width(), total_content_height }, list_scroll_cfg).y();

		const vec2f mouse_pos = ctx.mouse_position();

		const std::size_t first_visible = static_cast<std::size_t>(voff / row_height);
		const std::size_t last_visible = std::min(count, first_visible + visible_count + 2);

		for (std::size_t i = first_visible; i < last_visible; ++i) {
			const float item_y = list_rect.top() - static_cast<float>(i) * row_height + voff;

			if (item_y < list_rect.bottom() - row_height || item_y > list_rect.top() + row_height) {
				continue;
			}

			const rectf option_rect =
				rectf::from_position_size(
					{ content_area.left(), item_y },
					{ content_area.width(), row_height }
				);

			const float clipped_top = std::min(option_rect.top(), content_area.top());
			const float clipped_bottom = std::max(option_rect.bottom(), content_area.bottom());
			const float clipped_height = clipped_top - clipped_bottom;

			if (clipped_height <= 0.f) {
				continue;
			}

			const rectf clipped_option = rectf::from_position_size(
				{ option_rect.left(), clipped_top },
				{ content_area.width(), clipped_height }
			);

			const bool option_hovered = content_area.contains(mouse_pos) && ctx.hovers(option_rect);

			vec4f option_bg{ 0.f, 0.f, 0.f, 0.f };
			if (i == current_index) {
				option_bg = ctx.style.color_accent_dim;
			}
			else if (option_hovered) {
				option_bg = ctx.style.color_widget_hovered;
			}

			ctx.queue_sprite({
				.rect = clipped_option,
				.color = option_bg,
				.texture = ctx.blank_texture,
			});

			ctx.queue_text({
				.font = fnt,
				.text = get_option(i),
				.position = { option_rect.left() + ctx.style.padding, option_rect.center().y() + fnt_view->vertical_center_offset(ctx.style.font_size) },
				.scale = ctx.style.font_size,
				.color = ctx.style.color_text,
				.clip_rect = content_area,
			});

			if (option_hovered && ctx.mouse_pressed_for(option_rect)) {
				result.changed = true;
				result.new_index = i;
				state.open_dropdown_id.reset();
			}
		}

		const bool still_open = state.open_dropdown_id == dropdown_id;
		const bool raw_press = ctx.mouse_pressed();
		if (still_open && !header_rect.contains(mouse_pos) && !list_rect.contains(mouse_pos) && !state.scroll.y.held && raw_press) {
			state.open_dropdown_id.reset();
		}
	}

	return result;
}
