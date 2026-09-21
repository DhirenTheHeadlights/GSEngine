export module gse.ide.agent:layout;

import std;
import gse;
import gse.ide.highlight;

import :model;

namespace gse::ide::agent {
	constexpr float chat_wrap_fraction = 0.72f;
	constexpr std::size_t diff_indent = 4;
	constexpr std::size_t diff_gap = 2;
	constexpr std::size_t diff_min_side_columns = 40;
	constexpr std::size_t diff_min_columns = 16;
	constexpr std::size_t transcript_tab_width = 4;
	constexpr std::uint32_t row_preview_lines = 8;
	constexpr std::uint32_t group_detail_rows = 12;
	constexpr std::string_view mcp_tool_prefix = "mcp__";

	struct transcript_metrics {
		const font& face;
		const font& body;
		float width = 0.f;
		float scale = 0.f;
	};

	struct diff_layout {
		std::size_t gutter = 0;
		std::size_t width = 0;
		std::size_t offset = 0;
	};

	struct markup_run {
		std::size_t start = 0;
		std::size_t end = 0;
		vec4f color;
		gui::text_face face = gui::text_face::inherit;
	};

	struct markup_line {
		std::string text;
		std::span<const markup_run> runs{};
		markdown::display_style base;
	};

	struct transcript_cursor {
		std::string_view prefix;
		std::string indent;
		vec4f prefix_color;
		float available = 0.f;
		std::uint32_t row = 0;
		bool first = true;
		bool wrap = true;
	};

	struct transcript_line {
		std::string_view prefix;
		std::string_view text;
		vec4f color;
		gui::text_face face = gui::text_face::inherit;
		std::uint32_t row = 0;
		float wrap_width = 0.f;
		bool markdown = false;
	};

	struct diff_cell {
		std::uint32_t number = 0;
		std::string_view text;
		vec4f color;
	};

	auto style_of(
		row_kind kind
	) -> row_style;

	auto family_of(
		gui::text_face face
	) -> markdown::family;

	auto verbatim_detail(
		const transcript_row& row
	) -> bool;

	auto line_base_style(
		const gui::style& sty,
		const markdown::line_info& info,
		const vec4f& fallback
	) -> markdown::display_style;

	auto table_extent(
		std::span<const std::string> lines,
		std::size_t first
	) -> std::size_t;

	auto align_table(
		std::span<const std::string> rows
	) -> std::vector<std::string>;

	auto trailing_blank(
		const session& s
	) -> bool;

	auto push_gap(
		session& s,
		const transcript_cursor& cursor
	) -> void;

	auto code_fence_language(
		std::string_view line
	) -> std::string_view;

	auto highlighted_language(
		std::string_view tag
	) -> bool;

	auto style_runs(
		std::span<const markdown::rendered_run> runs,
		const gui::style& sty,
		const markdown::display_style& base,
		std::vector<markup_run>& out
	) -> void;

	auto highlight_runs(
		std::span<const gui::text_span> spans,
		std::uint32_t line,
		std::vector<markup_run>& out
	) -> void;

	auto push_markup_line(
		session& s,
		const markup_line& parsed,
		const transcript_metrics& metrics,
		transcript_cursor& cursor
	) -> void;

	auto push_code_block(
		session& s,
		std::span<const std::string> body,
		const markdown::display_style& base,
		const transcript_metrics& metrics,
		transcript_cursor& cursor
	) -> void;

	auto push_transcript_line(
		session& s,
		const gui::style& sty,
		const transcript_line& line,
		const transcript_metrics& metrics
	) -> void;

	auto expand_tabs(
		std::string_view text
	) -> std::string;

	auto clip_columns(
		std::string_view text,
		std::size_t offset,
		std::size_t width
	) -> std::string_view;

	auto column_overflow(
		std::string_view text,
		std::size_t width
	) -> std::size_t;

	auto hunk_start_line(
		const transcript_row& row
	) -> std::uint32_t;

	auto diff_view_for(
		session& s,
		std::uint32_t row
	) -> diff_view&;

	auto relayout_from(
		session& s,
		std::uint32_t row
	) -> void;

	auto draw_diff_bars(
		const gui::draw_context& ctx,
		session& s,
		const rectf& area,
		float advance
	) -> void;

	auto push_diff_line(
		session& s,
		const gui::style& sty,
		std::uint32_t index,
		const diff_layout& layout,
		std::span<const diff_cell> cells
	) -> void;

	auto push_diff_side(
		session& s,
		const gui::style& sty,
		diff_view& view,
		std::span<const std::string> lines,
		std::uint32_t start,
		std::uint32_t index,
		const diff_layout& layout,
		const vec4f& color
	) -> void;

	auto push_diff(
		session& s,
		const gui::style& sty,
		transcript_row& row,
		std::uint32_t index,
		const transcript_metrics& metrics
	) -> void;

	auto quiet_tool(
		const transcript_row& row
	) -> bool;

	auto grouped_row(
		const transcript_row& row
	) -> bool;

	auto group_summary(
		const group_marker& group
	) -> std::string;

	auto group_detail(
		const session& s,
		const group_marker& group
	) -> std::string;

	auto group_at(
		const session& s,
		std::uint32_t line
	) -> const group_marker*;

	auto chat_row(
		const transcript_row& row
	) -> bool;

	auto after_bubble(
		const session& s
	) -> bool;

	auto chat_bubble(
		const gui::style& sty,
		const transcript_row& row,
		std::uint32_t first_line,
		std::uint32_t last_line
	) -> gui::text_block;

	auto link_at(
		const session& s,
		std::uint32_t line
	) -> const link_marker*;

	auto group_expanded(
		const session& s,
		std::uint32_t row
	) -> bool;

	auto toggle_marker(
		session& s,
		const group_marker& marker
	) -> void;

	auto marker_at(
		const session& s,
		std::uint32_t line
	) -> const group_marker*;

	auto truncate_transcript(
		session& s,
		std::uint32_t line
	) -> void;

	auto push_row(
		session& s,
		const gui::style& sty,
		transcript_row& row,
		std::uint32_t index,
		const transcript_metrics& metrics
	) -> void;

	auto sync_transcript(
		session& s,
		const gui::style& sty,
		const transcript_metrics& metrics
	) -> void;
}
