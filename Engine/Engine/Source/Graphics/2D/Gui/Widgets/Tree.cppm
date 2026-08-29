export module gse.graphics:tree_widget;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.os;
import gse.assets;
import gse.gpu;

import :types;
import :font;
import :ids;
import :styles;
import :builder;
import :interaction;
import :symbols;

export namespace gse::gui::draw {
	struct tree_options {
		float row_gap = 0.15f;
		float indent_per_level = 15.f;
		float extra_right_padding = 0.0f;
		bool toggle_on_row_click = true;
		bool multi_select = false;
		std::span<const std::uint64_t> open_keys;
		std::uint64_t reveal_key = 0;
		float* reveal_offset = nullptr;
	};

	struct tree_selection {
		std::unordered_set<std::uint64_t> keys;
		std::uint64_t activated = 0;
	};

	template <typename T>
	struct tree_ops {
		std::function<std::span<const T>(const T&)> children;

		std::function<std::string_view(const T&)> label;

		std::function<std::uint64_t(const T&)> key;

		std::function<bool(const T&)> is_leaf;

		std::function<void(const T&, const draw_context&, const rectf&, bool, bool, int)> custom_draw = nullptr;

		std::function<void(const T&, const draw_context&, vec2f)> on_context = nullptr;

		std::function<std::span<const symbol::stroke>(const T&)> icon = nullptr;

		std::function<vec4f(const T&)> label_color = nullptr;
	};

	template <typename T>
	auto tree(
		const draw_context& ctx,
		std::span<const T> roots,
		const tree_ops<T>& fns,
		tree_options opt,
		tree_selection* sel,
		id& active_widget_id,
		resource::handle<font> font = {}
	) -> bool;
}

export namespace gse::gui {
	template <typename T>
	struct tree {
		using result = bool;
		struct params {
			std::span<const T> roots;
			const draw::tree_ops<T>& ops;
			draw::tree_options options = {};
			draw::tree_selection* selection = nullptr;
			resource::handle<font> font{};
		};
		static auto draw(draw_context& ctx, params p, id&, id& active, id&) -> bool {
			return draw::tree(ctx, p.roots, p.ops, p.options, p.selection, active, p.font);
		}
	};
}

namespace gse::gui::draw {
	template <typename T>
	auto tree_node_key(
		const T& t,
		const tree_ops<T>& ops,
		std::uint64_t tree_scope
	) -> std::uint64_t;

	template <typename T>
	auto tree_node_is_leaf(
		const T& t,
		const tree_ops<T>& ops
	) -> bool;

	template <typename T>
	auto tree_node(
		const draw_context& ctx,
		const T& t,
		const tree_ops<T>& ops,
		const tree_options& opt,
		tree_selection* sel,
		std::uint64_t tree_scope,
		int level,
		id& active_widget_id,
		const resource::handle<font>& fnt
	) -> bool;
}

template <typename T>
auto gse::gui::draw::tree(const draw_context& ctx, std::span<const T> roots, const tree_ops<T>& fns, tree_options opt, tree_selection* sel, id& active_widget_id, const resource::handle<font> font) -> bool {
	const auto fnt = font.valid() ? font : ctx.fonts.text;
	if (!ctx.current_menu || !fnt.valid()) {
		return false;
	}

	const std::uint64_t tree_scope = ids::current_seed();
	std::unordered_set<std::uint64_t>& open_set = ctx.widget_tree_open[tree_scope];
	for (const std::uint64_t key : opt.open_keys) {
		open_set.insert(key);
	}

	constexpr float missing_row = std::numeric_limits<float>::lowest();
	float reveal_row_top = missing_row;
	tree_options node_opt = opt;
	node_opt.reveal_offset = opt.reveal_offset && opt.reveal_key != 0 ? &reveal_row_top : nullptr;

	const float content_start = ctx.layout_cursor.y();
	bool is_active = false;

	for (const T& r : roots) {
		is_active |= tree_node(ctx, r, fns, node_opt, sel, tree_scope, 0, active_widget_id, fnt);
	}

	if (opt.reveal_offset && reveal_row_top != missing_row) {
		*opt.reveal_offset = content_start - reveal_row_top;
	}

	return is_active;
}

template <typename T>
auto gse::gui::draw::tree_node_key(const T& t, const tree_ops<T>& ops, const std::uint64_t tree_scope) -> std::uint64_t {
	if (ops.key) {
		return ops.key(t);
	}

	const std::string_view lbl = ops.label ? ops.label(t) : std::string_view{};
	return hash_combine(tree_scope, stable_id(lbl));
}

template <typename T>
auto gse::gui::draw::tree_node_is_leaf(const T& t, const tree_ops<T>& ops) -> bool {
	if (ops.is_leaf) {
		return ops.is_leaf(t);
	}

	if (ops.children) {
		return ops.children(t).empty();
	}

	return true;
}

template <typename T>
auto gse::gui::draw::tree_node(const draw_context& ctx, const T& t, const tree_ops<T>& ops, const tree_options& opt, tree_selection* sel, std::uint64_t tree_scope, int level, id& active_widget_id, const resource::handle<font>& fnt) -> bool {
	const auto fnt_view = fnt.resolve();
	const float row_height = fnt_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const float gap = row_height * opt.row_gap;
	const rectf context_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const float indent = std::max(0.f, opt.indent_per_level) * std::max(0, level);
	const float row_x = std::min(context_rect.left() + indent, context_rect.right());
	const float row_width = std::max(0.f, context_rect.right() - row_x);

	const rectf row_rect = rectf::from_position_size(
		{ row_x, ctx.layout_cursor.y() },
		{ row_width, row_height }
	);

	const std::uint64_t key = tree_node_key(t, ops, tree_scope);
	if (opt.reveal_offset && opt.reveal_key == key) {
		*opt.reveal_offset = row_rect.top();
	}
	std::unordered_set<std::uint64_t>& open_set = ctx.widget_tree_open[tree_scope];
	const bool leaf = tree_node_is_leaf(t, ops);
	bool is_open = open_set.contains(key);

	const rectf effective_clip = ctx.current_clip().value_or(context_rect);
	const rectf visible = row_rect.intersection(effective_clip);
	const bool row_visible = visible.height() > 0.f;

	const vec2f mouse_pos = ctx.mouse_position();
	const bool hovered = row_visible && ctx.hovers(visible);
	const id row_widget_id = ids::make_from_key(hash_combine(stable_id("tree_row"), key));

	bool self_is_active = hovered;

	bool selected = false;
	if (sel && sel->keys.contains(key)) {
		selected = true;
	}

	const bool released = ctx.mouse_released();

	if (hovered && ops.on_context && ctx.mouse_pressed_for(visible, mouse_button::button_2)) {
		ops.on_context(t, ctx, mouse_pos);
	}

	const bool owns_active = active_widget_id == row_widget_id;
	const bool released_by_me = (row_visible || owns_active)
		&& interaction::activate_on_click(active_widget_id, row_widget_id, hovered, hovered && ctx.mouse_pressed_for(visible), released);

	if (active_widget_id == row_widget_id) {
		self_is_active = true;
	}

	if (row_visible) {
		vec4f background = ctx.style.color_widget_background;

		if (selected) {
			background = ctx.style.color_widget_selected;
		}
		else if (active_widget_id == row_widget_id) {
			background = ctx.style.color_widget_active;
		}
		else if (hovered) {
			background = ctx.style.color_widget_hovered;
		}

		ctx.queue_sprite({
			.rect = row_rect,
			.color = background,
			.texture = ctx.blank_texture,
			.corner_radius = ctx.style.corner_radius
		});

		const float arrow_w = ctx.style.font_size;
		const rectf arrow_rect = rectf::from_position_size(
			row_rect.top_left(),
			{ arrow_w, row_height }
		);

		if (!leaf) {
			symbol::draw(ctx, is_open ? symbol::chevron_down() : symbol::chevron_right(), arrow_rect, {
				.color = ctx.style.color_text,
				.extent = ctx.style.icon_extent,
			});
		}

		const float icon_w = ops.icon ? ctx.style.font_size : 0.f;
		if (ops.icon) {
			const rectf icon_rect = rectf::from_position_size(
				{ row_rect.left() + arrow_w, row_rect.top() },
				{ icon_w, row_height }
			);
			symbol::draw(ctx, ops.icon(t), icon_rect, {
				.color = leaf ? ctx.style.color_file : ctx.style.color_folder,
				.extent = ctx.style.icon_extent,
			});
		}

		const std::string_view lbl = ops.label ? ops.label(t) : std::string_view{};

		const float label_available_width =
			std::max(
				0.0f,
				row_rect.width() - arrow_w - icon_w - ctx.style.padding * 0.5f - opt.extra_right_padding
			);

		const rectf label_rect = rectf::from_position_size(
			{ row_rect.left() + arrow_w + icon_w + ctx.style.padding * 0.5f, row_rect.top() },
			{ label_available_width, row_height }
		);

		ctx.queue_text({
			.font = fnt,
			.text = lbl,
			.position = { label_rect.left(), label_rect.center().y() + fnt_view->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = ops.label_color ? ops.label_color(t) : ctx.style.color_text,
			.clip_rect = label_rect
		});

		if (ops.custom_draw) {
			ops.custom_draw(t, ctx, row_rect, hovered, selected, level);
		}
	}

	if (released_by_me && hovered) {
		const rectf arrow_rect = rectf::from_position_size(
			row_rect.top_left(),
			{ ctx.style.font_size, row_height }
		);

		if (const bool clicked_arrow = arrow_rect.contains(mouse_pos); !leaf && (opt.toggle_on_row_click || clicked_arrow)) {
			if (is_open) {
				open_set.erase(key);
			}
			else {
				open_set.insert(key);
			}
			is_open = !is_open;
		}

		if (sel) {
			sel->activated = key;

			if (const bool ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control); opt.multi_select || ctrl) {
				if (const auto it = sel->keys.find(key); it != sel->keys.end()) {
					sel->keys.erase(it);
				}
				else {
					sel->keys.insert(key);
				}
			}
			else {
				sel->keys.clear();
				sel->keys.insert(key);
			}
		}
	}

	ctx.layout_cursor.y() -= (row_height + gap);

	bool children_are_active = false;

	if (is_open && !leaf && ops.children) {
		for (const std::span<const T> kids = ops.children(t); const T& ch : kids) {
			children_are_active |= tree_node(ctx, ch, ops, opt, sel, tree_scope, level + 1, active_widget_id, fnt);
		}
	}

	return self_is_active || children_are_active;
}
