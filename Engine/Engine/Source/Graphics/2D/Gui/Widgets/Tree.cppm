export module gse.graphics:tree_widget;

import std;

import gse.utility;
import gse.physics.math;
import gse.platform;

import :types;
import :ids;

export namespace gse::gui::draw {
	struct tree_options {
		float row_gap = 0.15f;
		float indent_per_level = 15.f;
		float extra_right_padding = 0.0f;
		bool toggle_on_row_click = true;
		bool multi_select = false;
	};

	struct tree_selection {
		std::unordered_set<std::uint64_t> keys;
	};

	template <typename T>
	struct tree_ops {
		std::function<std::span<const T>(
			const T&
		)> children;

		std::function<std::string_view(
			const T&
		)> label;

		std::function<std::uint64_t(
			const T&
		)> key;

		std::function<bool(
			const T&
		)> is_leaf;

		std::function<void(
			const T&,
			const draw_context&,
			const ui_rect&,
			bool,
			bool,
			int
		)> custom_draw = nullptr;
	};

	template <typename T>
	auto tree(
		const draw_context& ctx,
		std::span<const T> roots,
		const tree_ops<T>& fns,
		tree_options opt,
		tree_selection* sel,
		id& active_widget_id
	) -> bool;
}

namespace gse::gui::draw {
	struct expand_state {
		std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> open;
	};

	expand_state global_expand_state;

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
		id& active_widget_id
	) -> bool;
}

template <typename T>
auto gse::gui::draw::tree(const draw_context& ctx, std::span<const T> roots, const tree_ops<T>& fns, tree_options opt, tree_selection* sel, id& active_widget_id) -> bool {
	if (!ctx.current_menu || !ctx.font.valid()) {
		return false;
	}

	const std::uint64_t tree_scope = ids::current_seed();
	bool is_active = false;

	for (const T& r : roots) {
		is_active |= tree_node(ctx, r, fns, opt, sel, tree_scope, 0, active_widget_id);
	}

	return is_active;
}

template <typename T>
auto gse::gui::draw::tree_node_key(const T& t, const tree_ops<T>& ops, const std::uint64_t tree_scope) -> std::uint64_t {
	if (ops.key) {
		return ops.key(t);
	}

	const std::string_view lbl = ops.label ? ops.label(t) : std::string_view{};
	return ids::hash_combine_string(tree_scope, lbl);
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
auto gse::gui::draw::tree_node(const draw_context& ctx, const T& t, const tree_ops<T>& ops, const tree_options& opt, tree_selection* sel, std::uint64_t tree_scope, int level, id& active_widget_id) -> bool {
	const float row_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const float gap = row_height * opt.row_gap;
	const ui_rect context_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const float indent = std::max(0.f, opt.indent_per_level) * std::max(0, level);

	const ui_rect row_rect = ui_rect::from_position_size(
		{ context_rect.left() + indent, ctx.layout_cursor.y() },
		{ context_rect.width() - indent, row_height }
	);

	const std::uint64_t key = tree_node_key(t, ops, tree_scope);
	std::unordered_set<std::uint64_t>& open_set = global_expand_state.open[tree_scope];
	const bool leaf = tree_node_is_leaf(t, ops);
	bool is_open = open_set.contains(key);
	const bool hovered = row_rect.contains(ctx.input.mouse_position());
	const id row_widget_id = ids::make(std::format("tree_row##{}", key));

	bool self_is_active = hovered;

	bool selected = false;
	if (sel && sel->keys.contains(key)) {
		selected = true;
	}

	unitless::vec4 background = ctx.style.color_widget_background;

	if (selected) {
		background = ctx.style.color_widget_fill;
	} else if (hovered) {
		background = ctx.style.color_dock_widget;
		if (ctx.input.mouse_button_pressed(mouse_button::button_1)) {
			active_widget_id = row_widget_id;
		}
	}

	if (active_widget_id == row_widget_id) {
		self_is_active = true;
	}

	ctx.queue_sprite({
		.rect = row_rect,
		.color = background,
		.texture = ctx.blank_texture
	});

	const float arrow_w = ctx.style.font_size;
	const ui_rect arrow_rect = ui_rect::from_position_size(
		row_rect.top_left(),
		{ arrow_w, row_height }
	);

	if (!leaf) {
		ctx.queue_text({
			.font = ctx.font,
			.text = is_open ? "v" : ">",
			.position = {
				arrow_rect.center().x() - ctx.font->width("v", ctx.style.font_size) * 0.5f,
				arrow_rect.center().y() + ctx.style.font_size / 2.f
			},
			.scale = ctx.style.font_size,
			.clip_rect = row_rect
		});
	}

	const std::string_view lbl = ops.label ? ops.label(t) : std::string_view{};
	
	const float label_available_width = std::max(0.0f, row_rect.width() - arrow_w - ctx.style.padding * 0.5f - opt.extra_right_padding);
	
	const ui_rect label_rect = ui_rect::from_position_size(
		{ row_rect.left() + arrow_w + ctx.style.padding * 0.5f, row_rect.top() },
		{ label_available_width, row_height }
	);

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(lbl),
		.position = {
			label_rect.left(),
			label_rect.center().y() + ctx.style.font_size / 2.f
		},
		.scale = ctx.style.font_size,
		.clip_rect = label_rect
	});

	if (ops.custom_draw) {
		ops.custom_draw(t, ctx, row_rect, hovered, selected, level);
	}

	if (ctx.input.mouse_button_released(mouse_button::button_1)) {
		if (hovered) {
			if (const bool clicked_arrow = arrow_rect.contains(ctx.input.mouse_position()); !leaf && (opt.toggle_on_row_click || clicked_arrow)) {
				if (is_open) {
					open_set.erase(key);
				} else {
					open_set.insert(key);
				}
				is_open = !is_open;
			}

			if (sel) {
				if (const bool ctrl = ctx.input.key_held(key::left_control) || ctx.input.key_held(key::right_control); opt.multi_select || ctrl) {
					if (const auto it = sel->keys.find(key); it != sel->keys.end()) {
						sel->keys.erase(it);
					} else {
						sel->keys.insert(key);
					}
				} else {
					sel->keys.clear();
					sel->keys.insert(key);
				}
			}
		}

		active_widget_id.reset();
	}

	ctx.layout_cursor.y() -= (row_height + gap);

	bool children_are_active = false;

	if (is_open && !leaf && ops.children) {
		for (const std::span<const T> kids = ops.children(t); const T& ch : kids) {
			children_are_active |= tree_node(ctx, ch, ops, opt, sel, tree_scope, level + 1, active_widget_id);
		}
	}

	return self_is_active || children_are_active;
}