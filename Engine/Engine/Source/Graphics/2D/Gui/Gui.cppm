export module gse.graphics:gui;

import std;

import gse.platform;
import gse.utility;
import gse.physics.math;

import :types;
import :layout;
import :font;
import :rendering_context;
import :sprite_renderer;
import :texture;
import :text_renderer;
import :cursor;
import :save;
import :value_widget;
import :text_widget;
import :input_widget;
import :slider_widget;
import :ids;
import :button_widget;
import :tree_widget;

export namespace gse::gui {
	auto initialize(
		renderer::context& context
	) -> void;

	auto update(
		const window& window,
		const style& style = {}
	) -> void;

	auto frame(
		const renderer::context& context,
		renderer::sprite& sprite_renderer,
		renderer::text& text_renderer,
		const std::function<void()>& in_frame = {},
		const style& style = {}
	) -> void;

	auto save() -> void;

	auto start(
		const std::string& name, const std::function<void()>& contents
	) -> void;

	auto text(
		const std::string& text
	) -> void;

	auto button(
		const std::string& text
	) -> bool;

	auto input(
		const std::string& name,
		std::string& buffer
	) -> void;

	template <is_arithmetic T>
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void;

	template <is_quantity T, auto Unit = typename T::default_unit{} >
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void;

	template <typename T, int N>
	auto slider(
		const std::string& name,
		unitless::vec_t<T, N>& vec,
		unitless::vec_t<T, N> min,
		unitless::vec_t<T, N> max
	) -> void;

	template <typename T, int N, auto Unit = typename T::default_unit{}>
	auto slider(
		const std::string& name,
		vec_t<T, N>& vec,
		vec_t<T, N> min,
		vec_t<T, N> max
	) -> void;

	template <is_arithmetic T>
	auto value(
		const std::string& name,
		T value
	) -> void;

	template <is_quantity T, auto Unit = typename T::default_unit{}>
	auto value(
		const std::string& name,
		T value
	) -> void;

	template <typename T, int N>
	auto vec(
		const std::string& name,
		unitless::vec_t<T, N> vec
	) -> void;

	template <typename T, int N, auto Unit = typename T::default_unit{}>
	auto vec(
		const std::string& name,
		vec_t<T, N> vec
	) -> void;

	template <typename T>
	auto tree(
		std::span<const T> roots,
		const draw::tree_ops<T>& fns,
		draw::tree_options opt = {},
		draw::tree_selection* sel = nullptr
	) -> void;

	auto profiler(
	) -> void;
}

namespace gse::gui {
	id_mapped_collection<menu> menus;
	menu* current_menu = nullptr;
	resource::handle<font> font;
	resource::handle<texture> blank_texture;
	std::optional<dock::space> active_dock_space;
	state current_state;
	std::filesystem::path file_path = "Misc/gui_layout.ggui";
	clock save_clock;

	id hot_widget_id;
	id active_widget_id;
	id focus_widget_id;

	std::unique_ptr<ids::scope> current_scope;

	constexpr time update_interval = seconds(30.f);

	struct frame_bindings {
		const renderer::context* rctx = nullptr;
		renderer::sprite* sprite = nullptr;
		renderer::text* text = nullptr;
		style sty{};
		bool bound = false;
	};

	frame_bindings frame_bindings{};
	widget_context* context = nullptr;

	auto begin(const std::string& name) -> bool;
	auto end() -> void;

	auto handle_idle_state(unitless::vec2 mouse_position, bool mouse_held, const style& style) -> state;
	auto handle_dragging_state(const states::dragging& current, const window& window, unitless::vec2 mouse_position, bool mouse_held) -> state;
	auto handle_resizing_state(const states::resizing& current, unitless::vec2 mouse_position, bool mouse_held, const style& style) -> state;
	auto handle_resizing_divider_state(const states::resizing_divider& current, unitless::vec2 mouse_position, bool mouse_held, const style& style) -> state;
}

auto gse::gui::initialize(renderer::context& context) -> void {
	font = context.get<gse::font>("MonaspaceNeon-Regular");
	blank_texture = context.queue<texture>("blank", unitless::vec4(1, 1, 1, 1));
	menus = load(config::resource_path / file_path, menus);
}

auto gse::gui::update(const window& window, const style& style) -> void {
	const auto mouse_position = mouse::position();
	const bool mouse_held = mouse::held(mouse_button::button_1);

	match(current_state)
		.if_is<states::idle>([&](const auto&) {
			current_state = handle_idle_state(mouse_position, mouse_held, style);
		})
		.else_if_is<states::dragging>([&](const auto& state) {
			current_state = handle_dragging_state(state, window, mouse_position, mouse_held);
		})
		.else_if_is<states::resizing>([&](const auto& state) {
			current_state = handle_resizing_state(state, mouse_position, mouse_held, style);
		})
		.else_if_is<states::resizing_divider>([&](const auto& state) {
			current_state = handle_resizing_divider_state(state, mouse_position, mouse_held, style);
		})
		.otherwise([&] {
			current_state = states::idle{};
		});

	if (save_clock.elapsed() > update_interval) {
		save(menus, config::resource_path / file_path);
		save_clock.reset();
	}
}

auto gse::gui::frame(const renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer, const std::function<void()>& in_frame, const style& style) -> void {
	if (!font.valid()) return;

	frame_bindings = { &context, &sprite_renderer, &text_renderer, style, true };
	hot_widget_id = {};
	for (auto& m : menus.items()) {
		m.was_begun_this_frame = false;
	}

	in_frame();

	if (!frame_bindings.bound) {
		frame_bindings = { &context, &sprite_renderer, &text_renderer, style, true };
		return;
	}

	if (active_dock_space) {
		const auto mouse_pos = mouse::position();
		for (const auto& area : active_dock_space->areas) {
			if (area.rect.contains(mouse_pos)) {
				frame_bindings.sprite->queue({
					.rect = area.target,
					.color = frame_bindings.sty.color_dock_preview,
					.texture = blank_texture
				});
				break;
			}
		}
		for (const auto& area : active_dock_space->areas) {
			frame_bindings.sprite->queue({
				.rect = area.rect,
				.color = frame_bindings.sty.color_dock_widget,
				.texture = blank_texture
			});
		}
	}

	if (frame_bindings.rctx && frame_bindings.sprite && frame_bindings.rctx->ui_focus()) {
		cursor::render(*frame_bindings.rctx, *frame_bindings.sprite);
	}

	frame_bindings = {};
}

auto gse::gui::save() -> void {
	save(menus, config::resource_path / file_path);
}

auto gse::gui::start(const std::string& name, const std::function<void()>& contents) -> void {
	if (!frame_bindings.bound) return;
	if (!begin(name)) return;

	auto& menu = *current_menu;
	const auto& sty = frame_bindings.sty;

	const auto title_bar_rect = ui_rect::from_position_size(
		menu.rect.top_left(),
		{ menu.rect.width(), sty.title_bar_height }
	);

	const auto body_rect = ui_rect::from_position_size(
		{ menu.rect.left(), menu.rect.top() - sty.title_bar_height },
		{ menu.rect.width(), menu.rect.height() - sty.title_bar_height }
	);

	frame_bindings.sprite->queue({
		.rect = body_rect,
		.color = sty.color_menu_body,
		.texture = blank_texture
	});

	frame_bindings.sprite->queue({
		.rect = title_bar_rect,
		.color = sty.color_title_bar,
		.texture = blank_texture
	});

	if (font.valid()) {
		frame_bindings.text->draw_text({
			.font = font,
			.text = menu.tab_contents[menu.active_tab_index],
			.position = {
				title_bar_rect.left() + sty.padding,
				title_bar_rect.center().y() + sty.font_size / 2.f
			},
			.scale = sty.font_size,
			.clip_rect = title_bar_rect
		});
	}

	const auto content_rect = body_rect.inset({ sty.padding, sty.padding });
	unitless::vec2 layout_cursor = content_rect.top_left();

	ids::scope menu_scope(menu.id().number());

	widget_context ctx{
		.current_menu = &menu,
		.style = sty,
		.sprite_renderer = *frame_bindings.sprite,
		.text_renderer = *frame_bindings.text,
		.font = font,
		.blank_texture = blank_texture,
		.layout_cursor = layout_cursor
	};

	hot_widget_id = {};
	context = &ctx;
	contents();
	context = nullptr;

	end();
}

auto gse::gui::text(const std::string& text) -> void {
	if (!context) return;
	draw::text(*context, "", text);
}

auto gse::gui::button(const std::string& text) -> bool {
	if (!context) return false;
	const auto& ctx = *context;

	const auto widget_id = ids::make(text);

	const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const auto content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const ui_rect button_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const bool hovered = button_rect.contains(mouse::position());

	if (hovered && mouse::pressed(mouse_button::button_1)) {
		active_widget_id = widget_id;
		if (focus_widget_id.exists()) focus_widget_id = {};
	}

	draw::button(ctx, text, hot_widget_id, active_widget_id);

	bool clicked = false;
	if (mouse::released(mouse_button::button_1) && active_widget_id == widget_id) {
		clicked = hovered;
		active_widget_id = {};
	}

	return clicked;
}

auto gse::gui::input(const std::string& name, std::string& buffer) -> void {
	if (!context) return;
	const auto& ctx = *context;

	const std::string id_label = name + "##Input";
	const auto widget_id = ids::make(id_label);

	const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const auto content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = content_rect.width() * 0.4f;
	const ui_rect box_rect = ui_rect::from_position_size(
		{ row_rect.left() + label_width, row_rect.top() },
		{ content_rect.width() - label_width, widget_height }
	);

	const bool hovered = box_rect.contains(mouse::position());

	if (mouse::pressed(mouse_button::button_1)) {
		if (hovered) {
			focus_widget_id = widget_id;
		} else if (focus_widget_id == widget_id) {
			focus_widget_id = {};
		}
	}

	draw::input(ctx, name, buffer, hot_widget_id, focus_widget_id);
}

template <gse::is_arithmetic T>
auto gse::gui::slider(const std::string& name, T& value, const T min, const T max) -> void {
	if (!context) return;

	draw::slider(*context, name, value, min, max, hot_widget_id, active_widget_id);

	if (mouse::pressed(mouse_button::button_1) && !active_widget_id.exists()) {
		active_widget_id = hot_widget_id;
	}
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::slider(const std::string& name, T& value, T min, T max) -> void {
	if (!context) return;

	draw::slider<T, Unit>(*context, name, value, min, max, hot_widget_id, active_widget_id);

	if (mouse::pressed(mouse_button::button_1) && !active_widget_id.exists()) {
		active_widget_id = hot_widget_id;
	}
}

template <typename T, int N>
auto gse::gui::slider(const std::string& name, unitless::vec_t<T, N>& vec, unitless::vec_t<T, N> min, unitless::vec_t<T, N> max) -> void {
	if (!context) return;

	draw::slider<T, N>(*context, name, vec, min, max, hot_widget_id, active_widget_id);

	if (mouse::pressed(mouse_button::button_1) && !active_widget_id.exists()) {
		active_widget_id = hot_widget_id;
	}
}

template <typename T, int N, auto Unit>
auto gse::gui::slider(const std::string& name, vec_t<T, N>& vec, vec_t<T, N> min, vec_t<T, N> max) -> void {
	if (!context) return;

	draw::slider<T, N, Unit>(*context, name, vec, min, max, hot_widget_id, active_widget_id);

	if (mouse::pressed(mouse_button::button_1) && !active_widget_id.exists()) {
		active_widget_id = hot_widget_id;
	}
}

template <gse::is_arithmetic T>
auto gse::gui::value(const std::string& name, T value) -> void {
	if (!context) return;
	draw::value(*context, name, value);
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::value(const std::string& name, T value) -> void {
	if (!context) return;
	draw::value<T, Unit>(*context, name, value);
}

template <typename T, int N>
auto gse::gui::vec(const std::string& name, unitless::vec_t<T, N> vec) -> void {
	if (!context) return;
	draw::vec(*context, name, vec);
}

template <typename T, int N, auto Unit>
auto gse::gui::vec(const std::string& name, vec_t<T, N> vec) -> void {
	if (!context) return;
	draw::vec<T, N, Unit>(*context, name, vec);
}

template <typename T>
auto gse::gui::tree(std::span<const T> roots, const draw::tree_ops<T>& fns, draw::tree_options opt, draw::tree_selection* sel) -> void {
	if (!context) return;
	draw::tree(*context, roots, fns, opt, sel, active_widget_id);
}

auto gse::gui::profiler() -> void {
	trace::thread_pause pause;

	const auto roots = trace::view().roots;
	if (roots.empty()) {
		text("No trace data");
		return;
	}

	static draw::tree_selection selection;
	static draw::tree_options options{
		.indent_per_level = 15.f,
		.toggle_on_row_click = true,
		.multi_select = false
	};

	ids::per_frame_key_cache cache;

	cache.begin(ids::stable_key("gui.tree.profiler"));

	const draw::tree_ops<trace::node> ops{
		.children = [](const trace::node& n) -> std::span<const trace::node> {
			return std::span{ n.children_first, n.children_count };
		},
		.label = [](const trace::node& n) -> std::string_view {
			return tag(n.id);
		},
		.key = [&cache](const trace::node& n) -> std::uint64_t {
			return cache.key(&n);
		},
		.is_leaf = [](const trace::node& n) -> bool {
			return n.children_count == 0;
		},
		.custom_draw = [](const trace::node& n, const widget_context& ctx, const ui_rect& row, bool /*hovered*/, bool /*selected*/, int /*level*/) {
			struct cand {
		        double v;
		        std::string_view name;
		    };

		    const auto span = (n.stop - n.start);
		    const auto self = n.self;

		    const cand dur_cands[] = {
		        { static_cast<double>(span.as<seconds>()),      decltype(seconds)::unit_name      },
		        { static_cast<double>(span.as<milliseconds>()), decltype(milliseconds)::unit_name },
		        { static_cast<double>(span.as<microseconds>()), decltype(microseconds)::unit_name },
		        { static_cast<double>(span.as<nanoseconds>()),  decltype(nanoseconds)::unit_name  },
		    };
		    const cand self_cands[] = {
		        { static_cast<double>(self.as<seconds>()),      decltype(seconds)::unit_name      },
		        { static_cast<double>(self.as<milliseconds>()), decltype(milliseconds)::unit_name },
		        { static_cast<double>(self.as<microseconds>()), decltype(microseconds)::unit_name },
		        { static_cast<double>(self.as<nanoseconds>()),  decltype(nanoseconds)::unit_name  },
		    };

		    auto choose = [](const cand* c) -> cand {
		        for (int i = 0; i < 3; ++i) if (c[i].v >= 1.0) return c[i];
		        return c[3];
		    };

		    const cand d = choose(dur_cands);
		    const cand s = choose(self_cands);
		    const double pct = (d.v > 0.0) ? (s.v / d.v) * 100.0 : 0.0;

		    auto to_fixed = [](const double v, char* buf, const std::size_t num, const int precision) -> std::string_view {
		        auto [p, ec] = std::to_chars(buf, buf + num, v, std::chars_format::fixed, precision);
		        if (ec != std::errc{}) return {};
		        return { buf, static_cast<size_t>(p - buf) };
		    };

		    char dur_buf[32], self_buf[32], pct_buf[32];
		    const std::string_view dur_num  = to_fixed(d.v,  dur_buf,  sizeof(dur_buf), 3);
		    const std::string_view self_num = to_fixed(s.v,  self_buf, sizeof(self_buf), 3);
		    const std::string_view pct_num  = to_fixed(pct,  pct_buf,  sizeof(pct_buf),  1);

		    const std::string_view dur_unit  = d.name;
		    const std::string_view self_unit = s.name;

		    static constexpr std::string_view cap_dur  = "Dur";
		    static constexpr std::string_view cap_self = "Self";
		    static constexpr std::string_view cap_pct  = "Percent";

		    auto width_sv = [&](const std::string_view str, const float scale) -> float {
		        return ctx.font->width(str, scale);
		    };

		    const float pad = ctx.style.padding;
		    const float widget_h = row.height();
		    const float cap_scale = ctx.style.font_size * 1.f;
		    const float num_scale = ctx.style.font_size;

		    constexpr int num_boxes = 3;
		    const float all_spacing = pad * static_cast<float>(num_boxes - 1);
		    const float max_total_w = std::min(row.width() * 0.55f, 1000.0f);
		    const float box_w = (max_total_w - all_spacing) / static_cast<float>(num_boxes);

		    unitless::vec2 pos = { row.right() - max_total_w, row.top() };

		    auto draw_box = [&](const std::string_view caption, const std::string_view num, const std::string_view unit) {
		         const ui_rect box = ui_rect::from_position_size(pos, { box_w, widget_h });

			    ctx.sprite_renderer.queue({
			        .rect = box,
			        .color = ctx.style.color_widget_background,
			        .texture = ctx.blank_texture
			    });

			    const float num_w   = width_sv(num,  num_scale);
			    const float unit_w  = width_sv(unit, num_scale);
			    const float cap_w   = width_sv(caption, cap_scale);
			    const float gap_nu  = std::max(2.0f, pad * 0.15f); 
			    const float gap_uc  = std::max(2.0f, pad * 0.25f);  

			    float x = box.right() - pad * 0.5f - (num_w + gap_nu + unit_w + gap_uc + cap_w);

			    ctx.text_renderer.draw_text({
			        .font = ctx.font,
			        .text = std::string(num),
			        .position = { x, box.center().y() + num_scale / 2.f },
			        .scale = num_scale,
			        .clip_rect = box
			    });

			    x += num_w + gap_nu;

			    ctx.text_renderer.draw_text({
			        .font = ctx.font,
			        .text = std::string(unit),
			        .position = { x, box.center().y() + num_scale / 2.f },
			        .scale = num_scale,
			        .clip_rect = box
			    });

			    x += unit_w + gap_uc;

			    ctx.text_renderer.draw_text({
			        .font = ctx.font,
			        .text = std::string(caption),
			        .position = { x, box.center().y() + cap_scale / 2.f },
			        .scale = cap_scale,
			        .clip_rect = box
			    });

			    pos.x() += box_w + pad;
		    };

		    draw_box(cap_dur,  dur_num,  dur_unit);
		    draw_box(cap_self, self_num, self_unit);
		    draw_box(cap_pct,  pct_num,  "");
		}
	};

	ids::scope tree_scope("gui.tree.profiler");
	gui::tree(roots, ops, options, &selection);
}

auto gse::gui::begin(const std::string& name) -> bool {
	menu* menu_ptr;

	if (!exists(name)) {
		menu new_menu(
			name,
			menu_data{
				.rect = ui_rect({
					.min = { 100.f, 100.f },
					.max = { 400.f, 300.f }
				})
			}
		);

		menus.add(new_menu.id(), std::move(new_menu));
		menu_ptr = menus.try_get(find(name));
	}
	else {
		menu_ptr = menus.try_get(find(name));
	}

	if (menu_ptr) {
		current_menu = menu_ptr;
		current_menu->was_begun_this_frame = true;
		current_scope = std::make_unique<ids::scope>(current_menu->id().number());
		return true;
	}

	return false;
}

auto gse::gui::end() -> void {
	current_scope.reset();
	current_menu = nullptr;
}

auto gse::gui::handle_idle_state(const unitless::vec2 mouse_position, const bool mouse_held, const style& style) -> state {
	struct interaction_candidate {
		std::variant<states::resizing, states::dragging, states::resizing_divider> future_state;
		cursor::style cursor;
	};

	struct resize_rule {
		std::function<bool(const ui_rect&, const unitless::vec2&)> condition;
		resize_handle handle;
		cursor::style cursor;
	};

	static const std::array<resize_rule, 8> resize_rules = { {
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.left()) < t;
		}, resize_handle::top_left, cursor::style::resize_nw },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.right()) < t;
		}, resize_handle::top_right, cursor::style::resize_ne },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.left()) < t;
		}, resize_handle::bottom_left, cursor::style::resize_sw },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.right()) < t;
		}, resize_handle::bottom_right, cursor::style::resize_se },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.x() - r.left()) < t;
		}, resize_handle::left, cursor::style::resize_w },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.x() - r.right()) < t;
		}, resize_handle::right, cursor::style::resize_e },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.top()) < t;
		}, resize_handle::top, cursor::style::resize_n },
		{ [style](auto r, auto p) {
			const float t = style.resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t;
		}, resize_handle::bottom, cursor::style::resize_s },
	} };

	auto calculate_group_bounds = [](
		const id& root_id
		) -> ui_rect {
			const menu* root = menus.try_get(root_id);
			if (!root) return {};
			ui_rect bounds = root->rect;
			std::function<void(const id&)> expand = [&](const id& parent_id) {
				for (const auto& item : menus.items()) {
					if (item.owner_id() == parent_id) {
						bounds = ui_rect::bounding_box(bounds, item.rect);
						expand(item.id());
					}
				}
			};
			expand(root_id);
			return bounds;
		};

	set_style(cursor::style::arrow);

	auto hot_item = [&](
		) -> std::optional<interaction_candidate> {
			for (auto& menu : std::views::reverse(menus.items()) | std::views::filter([](const gui::menu& m) { return m.was_begun_this_frame; })) {
				if (!menu.owner_id().exists()) {
					if (menu.docked_to == dock::location::none) {
						const ui_rect group_rect = calculate_group_bounds(menu.id());
						for (const auto& [condition, handle, cursor] : resize_rules) {
							if (condition(group_rect, mouse_position)) {
								return interaction_candidate{
									.future_state = { states::resizing{.menu_id = menu.id(), .handle = handle } },
									.cursor = cursor
								};
							}
						}
					}
					else {
						const ui_rect& rect = menu.rect;
						switch (menu.docked_to) {
							case dock::location::left:
								if (std::abs(mouse_position.x() - rect.right()) < style.resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::right }, cursor::style::resize_e };
								break;
							case dock::location::right:
								if (std::abs(mouse_position.x() - rect.left()) < style.resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::left }, cursor::style::resize_w };
								break;
							case dock::location::top:
								if (std::abs(mouse_position.y() - rect.bottom()) < style.resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::bottom }, cursor::style::resize_s };
								break;
							case dock::location::bottom:
								if (std::abs(mouse_position.y() - rect.top()) < style.resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::top }, cursor::style::resize_n };
								break;
							default: break;
						}
					}
				}
				else {
					if (const auto* parent = menus.try_get(menu.owner_id())) {
						bool hovering = false;
						auto new_cursor = cursor::style::arrow;
						const auto& r = menu.rect;

						switch (menu.docked_to) {
							case dock::location::left:
								if (std::abs(mouse_position.x() - r.right()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
									hovering = true; new_cursor = cursor::style::resize_e;
								} break;
							case dock::location::right:
								if (std::abs(mouse_position.x() - r.left()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
									hovering = true; new_cursor = cursor::style::resize_w;
								} break;
							case dock::location::top:
								if (std::abs(mouse_position.y() - r.bottom()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
									hovering = true; new_cursor = cursor::style::resize_s;
								} break;
							case dock::location::bottom:
								if (std::abs(mouse_position.y() - r.top()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
									hovering = true; new_cursor = cursor::style::resize_n;
								} break;
							default: break;
						}

						if (hovering) {
							return interaction_candidate{
								.future_state = {
									states::resizing_divider{
										.parent_id = parent->id(),
										.child_id = menu.id()
									}
								},
								.cursor = new_cursor
							};
						}
					}
				}

				if (const auto title_bar_rect = ui_rect::from_position_size(
					{ menu.rect.left(), menu.rect.top() }, { menu.rect.width(), style.title_bar_height }
				); title_bar_rect.contains(mouse_position)) {
					return interaction_candidate{
						.future_state = {
							states::dragging{
								.menu_id = menu.id(),
								.offset = menu.rect.top_left() - mouse_position
							}
						},
						.cursor = cursor::style::omni_move
					};
				}
			}
			return std::nullopt;
		}();

	if (hot_item) {
		set_style(hot_item->cursor);
		if (mouse_held) {
			if (std::holds_alternative<states::dragging>(hot_item->future_state)) {
				const auto& [menu_id, offset] = std::get<states::dragging>(hot_item->future_state);
				if (const menu* m = menus.try_get(menu_id); m && m->docked_to != dock::location::none) {
					layout::undock(menus, m->id());
				}
			}
			return std::visit(
				[](auto&& arg) -> state {
					return arg;
				},
				hot_item->future_state
			);
		}
	}

	return states::idle{};
}

auto gse::gui::handle_resizing_state(const states::resizing& current, const unitless::vec2 mouse_position, const bool mouse_held, const style& style) -> state {
	if (!mouse_held) {
		active_dock_space.reset();
		return states::idle{};
	}

	menu* m = menus.try_get(current.menu_id);
	if (!m || !m->was_begun_this_frame) {
		return states::idle{};
	}

	auto calculate_group_bounds = [&](const id& root_id) -> ui_rect {
		const menu* root = menus.try_get(root_id);
		if (!root) return {};
		ui_rect bounds = root->rect;
		std::function<void(const id&)> expand = [&](const id& parent_id) {
			for (const auto& item : menus.items()) {
				if (item.owner_id() == parent_id) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};
		expand(root_id);
		return bounds;
	};

	const ui_rect group_rect = calculate_group_bounds(m->id());
	auto min_corner = group_rect.min();
	auto max_corner = group_rect.max();

	switch (current.handle) {
		case resize_handle::left:         min_corner.x() = mouse_position.x(); break;
		case resize_handle::right:        max_corner.x() = mouse_position.x(); break;
		case resize_handle::bottom:       min_corner.y() = mouse_position.y(); break;
		case resize_handle::top:          max_corner.y() = mouse_position.y(); break;
		case resize_handle::bottom_left:  min_corner = mouse_position; break;
		case resize_handle::bottom_right: min_corner.y() = mouse_position.y(); max_corner.x() = mouse_position.x(); break;
		case resize_handle::top_left:     min_corner.x() = mouse_position.x(); max_corner.y() = mouse_position.y(); break;
		case resize_handle::top_right:    max_corner = mouse_position; break;
		default: break;
	}

	if (max_corner.x() - min_corner.x() < style.min_menu_size.x()) {
		if (current.handle == resize_handle::left || current.handle == resize_handle::top_left || current.handle == resize_handle::bottom_left) {
			min_corner.x() = max_corner.x() - style.min_menu_size.x();
		}
		else {
			max_corner.x() = min_corner.x() + style.min_menu_size.x();
		}
	}
	if (max_corner.y() - min_corner.y() < style.min_menu_size.y()) {
		if (current.handle == resize_handle::bottom || current.handle == resize_handle::bottom_left || current.handle == resize_handle::bottom_right) {
			min_corner.y() = max_corner.y() - style.min_menu_size.y();
		}
		else {
			max_corner.y() = min_corner.y() + style.min_menu_size.y();
		}
	}

	m->rect = ui_rect({ .min = min_corner, .max = max_corner });
	layout::update(menus, m->id());

	return current;
}

auto gse::gui::handle_dragging_state(const states::dragging& current, const window& window, const unitless::vec2 mouse_position, const bool mouse_held) -> state {
	menu* m = menus.try_get(current.menu_id);
	if (!m || !m->was_begun_this_frame) {
		return states::idle{};
	}

	auto visible_menus = menus.items() | std::views::filter([](const menu& menu) {
		return menu.was_begun_this_frame;
	});

	if (!mouse_held) {
		if (active_dock_space) {
			id potential_dock_parent_id;
			for (const auto& other_menu : std::views::reverse(visible_menus)) {
				if (other_menu.id() != current.menu_id && other_menu.rect.contains(mouse_position)) {
					potential_dock_parent_id = other_menu.id();
					break;
				}
			}

			for (const auto& area : active_dock_space->areas) {
				if (area.rect.contains(mouse_position)) {
					if (potential_dock_parent_id.exists()) {
						if (area.dock_location == dock::location::center) {
							if (menu* parent = menus.try_get(potential_dock_parent_id)) {
								parent->tab_contents.insert(
									parent->tab_contents.end(),
									std::make_move_iterator(m->tab_contents.begin()),
									std::make_move_iterator(m->tab_contents.end())
								);
								m->tab_contents.clear();
								parent->active_tab_index = static_cast<uint32_t>(parent->tab_contents.size() - 1);
								menus.remove(current.menu_id);
							}
						}
						else {
							layout::dock(menus, current.menu_id, potential_dock_parent_id, area.dock_location);
							layout::update(menus, m->id());
						}
					}
					else {
						const auto viewport_size = unitless::vec2(window.viewport());
						const auto screen_rect = ui_rect::from_position_size({ 0.f, viewport_size.y() }, viewport_size);

						if (area.dock_location == dock::location::center) {
							m->rect = screen_rect;
							m->docked_to = dock::location::center;
							m->swap(id());
							layout::update(menus, m->id());
						}
						else {
							m->rect = layout::dock_target_rect(screen_rect, area.dock_location, 0.5f);
							m->docked_to = area.dock_location;
							m->swap(id());
							layout::update(menus, m->id());
						}
					}

					break;
				}
			}
		}
		active_dock_space.reset();
		return states::idle{};
	}

	const auto old_top_left = m->rect.top_left();
	auto new_top_left = mouse_position + current.offset;
	const auto window_size = unitless::vec2(window.viewport());
	new_top_left.x() = std::clamp(new_top_left.x(), 0.f, window_size.x() - m->rect.width());
	new_top_left.y() = std::clamp(new_top_left.y(), m->rect.height(), window_size.y());

	if (const auto delta = new_top_left - old_top_left; delta.x() != 0 || delta.y() != 0) {
		std::function<void(const id&)> move_group = [&](const id& current_id) {
			if (menu* item = menus.try_get(current_id)) {
				item->rect = ui_rect::from_position_size(item->rect.top_left() + delta, item->rect.size());
				for (auto& potential_child : menus.items()) {
					if (potential_child.owner_id() == current_id) {
						move_group(potential_child.id());
					}
				}
			}
		};
		move_group(current.menu_id);
	}

	active_dock_space.reset();
	bool found_parent_menu = false;
	for (auto& other_menu : std::views::reverse(visible_menus)) {
		if (other_menu.id() == current.menu_id) continue;
		if (other_menu.rect.contains(mouse_position)) {
			active_dock_space = layout::dock_space(other_menu.rect);
			found_parent_menu = true;
			break;
		}
	}

	if (!found_parent_menu) {
		const auto screen_rect = ui_rect::from_position_size({ 0.f, window_size.y() }, window_size);
		active_dock_space = layout::dock_space(screen_rect);
	}

	return current;
}

auto gse::gui::handle_resizing_divider_state(const states::resizing_divider& current, const unitless::vec2 mouse_position, const bool mouse_held, const style& style) -> state {
	if (!mouse_held) {
		return states::idle{};
	}

	menu* parent = menus.try_get(current.parent_id);
	menu* child = menus.try_get(current.child_id);

	if (!parent || !child) {
		return states::idle{};
	}

	const auto location = child->docked_to;
	const ui_rect combined_rect = ui_rect::bounding_box(parent->rect, child->rect);

	switch (location) {
		case dock::location::left:
		case dock::location::right: {
			if (combined_rect.width() < style.min_menu_size.x() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.left() + style.min_menu_size.x();
			const float max_clamp = combined_rect.right() - style.min_menu_size.x();
			const float divider_x = std::clamp(mouse_position.x(), min_clamp, max_clamp);

			if (location == dock::location::left) {
				child->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				parent->rect = ui_rect({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}
			else {
				parent->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				child->rect = ui_rect({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}

			if (combined_rect.width() > 0.f) {
				const float child_width = (location == dock::location::left) ? (divider_x - combined_rect.left()) : (combined_rect.right() - divider_x);
				parent->dock_split_ratio = child_width / combined_rect.width();
			}
			break;
		}
		case dock::location::top:
		case dock::location::bottom: {
			if (combined_rect.height() < style.min_menu_size.y() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.bottom() + style.min_menu_size.y();
			const float max_clamp = combined_rect.top() - style.min_menu_size.y();
			const float divider_y = std::clamp(mouse_position.y(), min_clamp, max_clamp);

			if (location == dock::location::top) {
				child->rect = ui_rect({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				parent->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}
			else {
				parent->rect = ui_rect({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				child->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}

			if (combined_rect.height() > 0.f) {
				const float child_height = (location == dock::location::top) ? (combined_rect.top() - divider_y) : (divider_y - combined_rect.bottom());
				parent->dock_split_ratio = child_height / combined_rect.height();
			}
			break;
		}
		default:
			break;
	}

	layout::update(menus, child->id());

	return current;
}
