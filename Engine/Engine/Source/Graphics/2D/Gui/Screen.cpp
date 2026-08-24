module gse.graphics:gui_screen_impl;

import std;

import gse.os;
import gse.config;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.save;

import :gui;
import :gui_screen;
import :gui_chrome;

import :types;
import :layout;
import :font;
import :ui_renderer;
import :texture;
import :cursor;
import :save;
import :ids;
import :input_layers;
import :settings;
import :styles;
import :builder;
import :menu_stack;
import :render_layer;
import :interaction;
import :symbols;
import :tab_strip;
import :widget_context;

auto gse::gui::draw_screen_caption(builder& b, screen& top, const rectf& bar_rect, const rectf& full_rect, const channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request, window_panel_drag_request> channels) -> void {
	draw_context& ctx = b.ctx;
	const style& sty = ctx.style;

	ctx.clip_stack.push_back(bar_rect);

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const float button_w = bar_rect.height() * 1.5f;
	const rectf close_rect = rectf::from_position_size({ bar_rect.right() - button_w, bar_rect.top() }, { button_w, bar_rect.height() });
	const rectf max_rect = rectf::from_position_size({ bar_rect.right() - button_w * 2.f, bar_rect.top() }, { button_w, bar_rect.height() });
	const rectf min_rect = rectf::from_position_size({ bar_rect.right() - button_w * 3.f, bar_rect.top() }, { button_w, bar_rect.height() });

	const rectf content_rect = rectf::from_position_size(
		{ bar_rect.left(), bar_rect.top() },
		{ std::max(0.f, min_rect.left() - bar_rect.left()), bar_rect.height() }
	);

	const float screen_controls_width = top.draw_caption(b, content_rect);

	if (caption_button(b, close_rect, "##screen_caption_close", symbol::close(), vec4f{ 0.78f, 0.22f, 0.22f, 1.f })) {
		channels.push<window_close_request>({});
	}
	if (caption_button(b, max_rect, "##screen_caption_max", symbol::maximize(), sty.color_widget_hovered)) {
		channels.push<window_toggle_maximize_request>({});
	}
	if (caption_button(b, min_rect, "##screen_caption_min", symbol::minimize(), sty.color_widget_hovered)) {
		channels.push<window_minimize_request>({});
	}

	const caption_exclusion exclusion = top.caption_exclusion_range(ctx, full_rect);

	channels.push<window_chrome_metrics_request>({
		.caption_height = static_cast<int>(bar_rect.height()),
		.controls_width = static_cast<int>(button_w * 3.f + screen_controls_width),
		.resize_exclude_y0 = exclusion.y0,
		.resize_exclude_y1 = exclusion.y1,
	});

	ctx.clip_stack.pop_back();
}

auto gse::gui::process_screen(data& d, viewport_state& vp, const gse::input::state& input_state, const vec2f viewport_size, const channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request, window_panel_drag_request> channels) -> void {
	if (!vp.fstate.active) {
		return;
	}

	auto* top = vp.menu_stack.top();
	if (top == nullptr) {
		return;
	}

	const style& sty = vp.fstate.sty;
	const rectf full_rect = top->body_rect(sty, viewport_size);
	const bool wants_caption = top->wants_chrome();
	const float caption_height = wants_caption ? sty.title_bar_height : 0.f;
	const rectf caption_rect = rectf::from_position_size(
		{ full_rect.left(), full_rect.top() },
		{ full_rect.width(), caption_height }
	);
	const rectf body_rect = wants_caption
		? rectf::from_position_size(
			{ full_rect.left(), full_rect.top() - caption_height },
			{ full_rect.width(), std::max(0.f, full_rect.height() - caption_height) }
		)
		: full_rect;

	if (!vp.screen_surface) {
		vp.screen_surface.emplace(
			"__gui_screen__",
			menu_data{
				.rect = body_rect,
				.parent_id = id(),
			}
		);
	}
	vp.screen_surface->rect = body_rect;

	const rectf content_rect = body_rect.inset({ sty.padding, sty.padding });
	vec2f layout_cursor = content_rect.top_left();

	ids::scope screen_scope(vp.screen_surface->id().number());

	widget_context ctx{ {
		.current_menu = &*vp.screen_surface,
		.style = sty,
		.fonts = d.fonts,
		.blank_texture = d.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.text_pool = d.text_pools[d.text_pool_slot],
		.text_pool_used = d.text_pool_used,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.current_layer = render_layer::popup,
		.input_layer = vp.input_layer_render,
		.input_suppressed = vp.input_suppressed,
		.hit_regions = &vp.input_layers_data,
		.tooltip = &vp.tooltip,
		.context_menu = &vp.context_menu,
		.clip_stack = { body_rect },
	}, input_state };

	vp.hot_widget_id = {};
	d.context = &ctx;

	top->draw_backdrop(ctx, viewport_size);

	builder b{
		.ctx = ctx,
		.hot_widget_id = vp.hot_widget_id,
		.active_widget_id = vp.active_widget_id,
		.focus_widget_id = vp.focus_widget_id,
	};

	if (top->dismissable()
		&& input_state.mouse_button_pressed(mouse_button::button_1)
		&& !body_rect.contains(input_state.mouse_position())) {
		ctx.consume_press(mouse_button::button_1);
		vp.menu_stack.pop();
		d.context = nullptr;
		return;
	}

	if (wants_caption) {
		draw_screen_caption(b, *top, caption_rect, full_rect, channels);
	}

	vp.menu_stack.tick(b);

	d.context = nullptr;
}
