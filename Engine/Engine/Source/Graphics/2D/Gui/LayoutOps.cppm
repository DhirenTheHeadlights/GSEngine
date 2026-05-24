export module gse.graphics:layout_ops;

import std;

import gse.math;
import gse.core;

import :types;
import :styles;

export namespace gse::gui::layout {
	enum class halign {
		start,
		center,
		end,
	};

	enum class valign {
		top,
		center,
		bottom,
	};

	enum class size_kind {
		fixed,
		flex,
		ratio,
	};

	struct size_spec {
		size_kind kind = size_kind::flex;
		float value = 1.f;

		[[nodiscard]] static constexpr auto px(
			float v
		) -> size_spec;

		[[nodiscard]] static constexpr auto px(
			dp v,
			const style& sty
		) -> size_spec;

		[[nodiscard]] static constexpr auto flex(
			float weight = 1.f
		) -> size_spec;

		[[nodiscard]] static constexpr auto ratio(
			float fraction
		) -> size_spec;
	};

	template <std::size_t N>
	[[nodiscard]] auto split_horizontal(
		const ui_rect& parent,
		const std::array<size_spec, N>& specs,
		float gap = 0.f
	) -> std::array<ui_rect, N>;

	template <std::size_t N>
	[[nodiscard]] auto split_vertical(
		const ui_rect& parent,
		const std::array<size_spec, N>& specs,
		float gap = 0.f
	) -> std::array<ui_rect, N>;

	[[nodiscard]] auto align_in(
		const ui_rect& parent,
		vec2f child_size,
		halign h = halign::center,
		valign v = valign::center
	) -> ui_rect;

	[[nodiscard]] auto inset_per_side(
		const ui_rect& parent,
		float top,
		float right,
		float bottom,
		float left
	) -> ui_rect;

	[[nodiscard]] auto centered(
		const ui_rect& parent,
		vec2f child_size
	) -> ui_rect;

	[[nodiscard]] auto fit_card(
		vec2f viewport_size,
		vec2f min_size,
		vec2f max_size,
		vec2f margin
	) -> ui_rect;

	auto skip(
		const draw_context& ctx,
		float amount
	) -> void;

	[[nodiscard]] auto reserve_row(
		const draw_context& ctx,
		float height,
		float trailing_gap = 0.f
	) -> ui_rect;

	class within_scope {
	public:
		within_scope(
			draw_context& ctx,
			const ui_rect& sub_rect
		);

		within_scope(
			const within_scope&
		) = delete;

		auto operator=(
			const within_scope&
		) -> within_scope& = delete;

		within_scope(
			within_scope&&
		) = delete;

		auto operator=(
			within_scope&&
		) -> within_scope& = delete;

		~within_scope();

	private:
		draw_context* m_ctx = nullptr;
		ui_rect m_saved_rect;
		vec2f m_saved_cursor;
	};

	[[nodiscard]] auto within(
		draw_context& ctx,
		const ui_rect& sub_rect
	) -> within_scope;
}

namespace gse::gui::layout {
	template <std::size_t N>
	[[nodiscard]] auto resolve_lengths(
		float available,
		const std::array<size_spec, N>& specs,
		float gap
	) -> std::array<float, N>;
}

constexpr auto gse::gui::layout::size_spec::px(const float v) -> size_spec {
	return { size_kind::fixed, v };
}

constexpr auto gse::gui::layout::size_spec::px(const dp v, const style& sty) -> size_spec {
	return { size_kind::fixed, v.px(sty) };
}

constexpr auto gse::gui::layout::size_spec::flex(const float weight) -> size_spec {
	return { size_kind::flex, weight };
}

constexpr auto gse::gui::layout::size_spec::ratio(const float fraction) -> size_spec {
	return { size_kind::ratio, fraction };
}

template <std::size_t N>
auto gse::gui::layout::resolve_lengths(const float available, const std::array<size_spec, N>& specs, const float gap) -> std::array<float, N> {
	std::array<float, N> out{};

	float fixed_total = 0.f;
	float ratio_total = 0.f;
	float flex_total = 0.f;
	for (const auto& s : specs) {
		switch (s.kind) {
			case size_kind::fixed:
				fixed_total += s.value;
				break;
			case size_kind::ratio:
				ratio_total += s.value * available;
				break;
			case size_kind::flex:
				flex_total += std::max(0.f, s.value);
				break;
		}
	}

	const float gap_total = N > 0 ? gap * static_cast<float>(N - 1) : 0.f;
	const float remaining = std::max(0.f, available - fixed_total - ratio_total - gap_total);

	for (std::size_t i = 0; i < N; ++i) {
		switch (specs[i].kind) {
			case size_kind::fixed:
				out[i] = specs[i].value;
				break;
			case size_kind::ratio:
				out[i] = specs[i].value * available;
				break;
			case size_kind::flex:
				out[i] = flex_total > 0.f ? remaining * (specs[i].value / flex_total) : 0.f;
				break;
		}
	}

	return out;
}

template <std::size_t N>
auto gse::gui::layout::split_horizontal(const ui_rect& parent, const std::array<size_spec, N>& specs, const float gap) -> std::array<ui_rect, N> {
	const auto widths = resolve_lengths<N>(parent.width(), specs, gap);

	std::array<ui_rect, N> out{};
	float x = parent.left();
	for (std::size_t i = 0; i < N; ++i) {
		out[i] = ui_rect::from_position_size(
			{ x, parent.top() },
			{ widths[i], parent.height() }
		);
		x += widths[i] + gap;
	}
	return out;
}

template <std::size_t N>
auto gse::gui::layout::split_vertical(const ui_rect& parent, const std::array<size_spec, N>& specs, const float gap) -> std::array<ui_rect, N> {
	const auto heights = resolve_lengths<N>(parent.height(), specs, gap);

	std::array<ui_rect, N> out{};
	float y = parent.top();
	for (std::size_t i = 0; i < N; ++i) {
		out[i] = ui_rect::from_position_size(
			{ parent.left(), y },
			{ parent.width(), heights[i] }
		);
		y -= heights[i] + gap;
	}
	return out;
}

auto gse::gui::layout::align_in(const ui_rect& parent, const vec2f child_size, const halign h, const valign v) -> ui_rect {
	float x = parent.left();
	switch (h) {
		case halign::start:
			x = parent.left();
			break;
		case halign::center:
			x = parent.center().x() - child_size.x() * 0.5f;
			break;
		case halign::end:
			x = parent.right() - child_size.x();
			break;
	}

	float y = parent.top();
	switch (v) {
		case valign::top:
			y = parent.top();
			break;
		case valign::center:
			y = parent.center().y() + child_size.y() * 0.5f;
			break;
		case valign::bottom:
			y = parent.bottom() + child_size.y();
			break;
	}

	return ui_rect::from_position_size({ x, y }, child_size);
}

auto gse::gui::layout::inset_per_side(const ui_rect& parent, const float top, const float right, const float bottom, const float left) -> ui_rect {
	return ui_rect({
		.min = { parent.left() + left, parent.bottom() + bottom },
		.max = { parent.right() - right, parent.top() - top }
	});
}

auto gse::gui::layout::centered(const ui_rect& parent, const vec2f child_size) -> ui_rect {
	return align_in(parent, child_size, halign::center, valign::center);
}

auto gse::gui::layout::fit_card(const vec2f viewport_size, const vec2f min_size, const vec2f max_size, const vec2f margin) -> ui_rect {
	const float w = std::clamp(viewport_size.x() - margin.x() * 2.f, min_size.x(), max_size.x());
	const float h = std::clamp(viewport_size.y() - margin.y() * 2.f, min_size.y(), max_size.y());
	const float left = (viewport_size.x() - w) * 0.5f;
	const float top = (viewport_size.y() + h) * 0.5f;
	return ui_rect::from_position_size({ left, top }, { w, h });
}

auto gse::gui::layout::skip(const draw_context& ctx, const float amount) -> void {
	ctx.layout_cursor.y() -= amount;
}

auto gse::gui::layout::reserve_row(const draw_context& ctx, const float height, const float trailing_gap) -> ui_rect {
	if (!ctx.current_menu) {
		return ui_rect::from_position_size(ctx.layout_cursor, { 0.f, height });
	}

	const ui_rect content = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const ui_rect row = ui_rect::from_position_size(
		{ content.left(), ctx.layout_cursor.y() },
		{ content.width(), height }
	);

	ctx.layout_cursor.y() -= height + trailing_gap;
	return row;
}

gse::gui::layout::within_scope::within_scope(draw_context& ctx, const ui_rect& sub_rect)
	: m_ctx(&ctx),
	  m_saved_rect(ctx.current_menu ? ctx.current_menu->rect : ui_rect{}),
	  m_saved_cursor(ctx.layout_cursor) {
	if (ctx.current_menu) {
		ctx.current_menu->rect = sub_rect;
	}
	ctx.layout_cursor = sub_rect.top_left();
}

gse::gui::layout::within_scope::~within_scope() {
	if (m_ctx == nullptr) {
		return;
	}
	if (m_ctx->current_menu) {
		m_ctx->current_menu->rect = m_saved_rect;
	}
	m_ctx->layout_cursor = m_saved_cursor;
}

auto gse::gui::layout::within(draw_context& ctx, const ui_rect& sub_rect) -> within_scope {
	return within_scope(ctx, sub_rect);
}
