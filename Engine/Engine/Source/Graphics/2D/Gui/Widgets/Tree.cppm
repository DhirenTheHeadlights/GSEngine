export module gse.graphics:tree_widget;

import std;

import gse.utility;  
import gse.physics.math;   
import gse.platform;

import :types;           
import :ids;             
import :text_renderer;     
import :sprite_renderer;

export namespace gse::gui::draw {
	struct tree_options {
		float row_gap = 0.15f;
		float indent_per_level = 15.f;
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
			widget_context&,
			const ui_rect&,
			bool,
			bool,
			int
		)> custom_draw = nullptr;
	};

	template <typename T>
	auto tree(
		widget_context& ctx,
		std::span<const T> roots,
		const tree_ops<T>& fns,
		tree_options opt,
		tree_selection* sel,
		id& active_widget_id
	) -> void;
}

namespace gse::gui::draw {
	struct expand_state {
		std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> open;
	};

	expand_state state;

	template <typename T>
	auto note_key(
		const T& t,
		const tree_ops<T>& ops,
		std::uint64_t tree_scope
	) -> std::uint64_t;

	template <typename T>
	auto is_leaf(
		const T& t,
		const tree_ops<T>& ops
	) -> bool;

	template <typename T>
	auto node(
		widget_context& ctx,
		const T& t,
		const tree_ops<T>& ops,
		const tree_options& opt,
		tree_selection* sel,
		std::uint64_t tree_scope,
		int level,
		id& active_widget_id
	) -> void;
	struct tree_debug_stats {
    std::size_t rows_drawn = 0;        // rows we actually emitted
    std::size_t nodes_visited = 0;     // times node() ran (i.e., candidates)
    std::size_t pruned_subtrees = 0;   // times we cut recursion because branch was closed
};

inline thread_local tree_debug_stats g_tree_stats;

inline void reset_tree_debug_stats() { g_tree_stats = {}; }
inline tree_debug_stats last_tree_debug_stats() { return g_tree_stats; }

}

template <typename T>
auto gse::gui::draw::tree(widget_context& ctx, std::span<const T> roots, const tree_ops<T>& fns, tree_options opt, tree_selection* sel, id& active_widget_id) -> void {
	if (!ctx.current_menu || !ctx.font.valid()) return;

	const auto tree_scope = ids::current_seed();

	for (const auto& r : roots) {
		node(ctx, r, fns, opt, sel, tree_scope, 0, active_widget_id);
	}
}

template <typename T>
auto gse::gui::draw::note_key(const T& t, const tree_ops<T>& ops, const std::uint64_t tree_scope) -> std::uint64_t {
	if (ops.key) {
		return ops.key(t);
	}
	const std::string_view lbl = ops.label ? ops.label(t) : std::string_view{};
    return ids::hash_combine_string(tree_scope, lbl);
}

template <typename T>
auto gse::gui::draw::is_leaf(const T& t, const tree_ops<T>& ops) -> bool {
	if (ops.is_leaf) {
		return ops.is_leaf(t);
	}

	if (ops.children) {
		return ops.children(t).empty();
	}

	return true;
}

template <typename T>
auto gse::gui::draw::node(widget_context& ctx, const T& t, const tree_ops<T>& ops, const tree_options& opt, tree_selection* sel, std::uint64_t tree_scope, int level, id& active_widget_id) -> void {
	g_tree_stats.nodes_visited++;

	const auto row_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const auto gap = row_height * opt.row_gap;

	const auto context_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const float indent = std::max(0.f, opt.indent_per_level) * std::max(0, level);

	const ui_rect row_rect = ui_rect::from_position_size(
		{ context_rect.left() + indent, ctx.layout_cursor.y() },
		{ context_rect.width() - indent, row_height }
	);
	const auto clip_rect = row_rect;

	const std::uint64_t key = note_key(t, ops, tree_scope);
	auto& open_set = state.open[tree_scope];
	const bool leaf = is_leaf(t, ops);
	bool is_open = open_set.contains(key);
	const bool hovered = row_rect.contains(mouse::position());
	const id row_widget_id = ids::make(std::format("tree_row##{}", key));

	bool selected = false;
	if (sel && sel->keys.contains(key)) {
		selected = true;
	}

	auto background = ctx.style.color_widget_background;

	if (selected) {
		background = ctx.style.color_widget_fill;
	}
	else if (hovered) {
		background = ctx.style.color_dock_widget;
		if (mouse::pressed(mouse_button::button_1) && !active_widget_id.exists()) {
			active_widget_id = row_widget_id;
		}
	}

	ctx.sprite_renderer.queue({
		.rect = row_rect,
		.color = background,
		.texture = ctx.blank_texture
	});
	g_tree_stats.rows_drawn++;

	const float arrow_w = ctx.style.font_size;
	const ui_rect arrow_rect = ui_rect::from_position_size(
		row_rect.top_left(),
		{ arrow_w, row_height }
	);

	if (!leaf) {
		ctx.text_renderer.draw_text({
			.font = ctx.font,
			.text = leaf ? "" : is_open ? "?" : ">",
			.position = {
				arrow_rect.center().x() - ctx.font->width("v", ctx.style.font_size) * 0.5f,
				arrow_rect.center().y() + ctx.style.font_size / 2.f
			},
			.scale = ctx.style.font_size,
			.clip_rect = clip_rect
		});
	}

	const std::string_view lbl = ops.label ? ops.label(t) : std::string_view{};
	const ui_rect label_rect = ui_rect::from_position_size(
		{ row_rect.left() + arrow_w + ctx.style.padding * 0.5f, row_rect.top() },
		{ row_rect.width() - arrow_w - ctx.style.padding * 0.5f, row_height }
	);

	ctx.text_renderer.draw_text({
		.font = ctx.font,
		.text = std::string(lbl),
		.position = {
			label_rect.left(),
			label_rect.center().y() + ctx.style.font_size / 2.f
		},
		.scale = ctx.style.font_size,
		.clip_rect = clip_rect
	});

	if (ops.custom_draw) {
		ops.custom_draw(t, ctx, row_rect, hovered, selected, level);
	}

	if (mouse::released(mouse_button::button_1) && active_widget_id == row_widget_id) {
		if (const bool clicked_arrow = arrow_rect.contains(mouse::position()); !leaf && (opt.toggle_on_row_click || clicked_arrow)) {
	        if (is_open) {
		        open_set.erase(key);
	        }
	        else {
		        open_set.insert(key);
	        }
	        is_open = !is_open;
	    }

	    if (sel) {
		    if (const bool ctrl = keyboard::pressed(key::left_control) || keyboard::pressed(key::right_control); opt.multi_select || ctrl) {
	            if (const auto it = sel->keys.find(key); it != sel->keys.end()) {
		            sel->keys.erase(it);
	            }
	            else {
		            sel->keys.insert(key);
	            }
	        } else {
	            sel->keys.clear();
	            sel->keys.insert(key);
	        }
	    }
	}

	ctx.layout_cursor.y() -= (row_height + gap);

	if (!is_open || leaf || !ops.children) {
		if (!leaf && ops.children && !is_open) {
        g_tree_stats.pruned_subtrees++;
    }
		return;
	}

	for (const auto kids = ops.children(t); const auto& ch : kids) {
		node(ctx, ch, ops, opt, sel, tree_scope, level + 1, active_widget_id);
	}
}



