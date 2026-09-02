export module gse.ide.app:code_panel;

import std;
import gse;
import gse.gpu;

import gse.ide.workspace;
import gse.ide.git;
import gse.ide.highlight;
import gse.format;
import gse.ide.build;
import gse.ide.analysis;
import gse.ide.diagnostic;
import gse.ide.lint;
import gse.ide.config;
import gse.ide.search;
import gse.ide.docs;
import gse.ide.navigation;

import :chrome;

namespace gse::ide {
	struct code_panel_inputs {
		shared_view<config_system::data> config;
		const search::index_state* index = nullptr;
		bool building = false;
	};

	auto draw_code_panel(
		gui::builder& ui,
		workspace::data& ws,
		channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, jump_to_request, set_cursor_shape_request, search::index_merge_request> channels,
		const code_panel_inputs& inputs
	) -> void;

	auto refresh_rendered_view(
		const gui::draw_context& ctx,
		document& doc,
		const rectf& rect
	) -> void;

	auto hover_wrap_lines(
		const gui::draw_context& ctx,
		std::string_view text,
		float max_width,
		float scale
	) -> std::vector<std::string>;

	auto draw_hover_panel(
		const gui::draw_context& ctx,
		const rectf& text_rect,
		hover_state& h,
		std::uint32_t z,
		bool input_owner
	) -> bool;

	auto draw_diagnostic_tooltip(
		const gui::draw_context& ctx,
		const rectf& text_rect,
		std::string_view label,
		std::string_view message,
		vec4f label_color,
		vec2f anchor
	) -> void;

	auto draw_game_placeholder(
		const gui::draw_context& ctx,
		const rectf& rect,
		std::string_view message
	) -> void;

	constexpr int quickfix_edit_kind = 0x5149;
	constexpr int format_edit_kind = 0x464d;

	struct quickfix_layout {
		rectf panel;
		rectf fix_row;
		rectf fixall_row;
	};

	auto draw_quickfix_tooltip(
		const gui::draw_context& ctx,
		const rectf& text_rect,
		std::string_view label,
		vec4f label_color,
		std::string_view message,
		std::string_view fix_title,
		int fixall_count,
		vec2f anchor,
		vec2f mouse
	) -> quickfix_layout;

	auto apply_quickfix(
		document& doc,
		std::span<const text_edit> edits
	) -> void;

	auto apply_lint_edits(
		workspace::data& ws,
		std::span<const lint_file_edits> files
	) -> std::size_t;

	auto adjust_after_format(
		gui::buffer_position& p,
		std::span<const format::line_edit> edits
	) -> void;

	auto apply_format(
		document& doc,
		const format::options& opts
	) -> void;

	auto mark_document_dirty(
		document& doc
	) -> void;

	struct format_save_request {
		id document_id;
		format::options format_options{};
		bool format_enabled = false;
		bool force_save = false;
	};

	auto format_and_save(
		workspace::data& ws,
		const format_save_request& request,
		channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> channels
	) -> document_save_result;

	auto draw_document_prompt(
		gui::builder& ui,
		workspace::data& ws,
		const rectf& body,
		channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> channels
	) -> void;

	auto apply_diagnostics(
		workspace::data& ws,
		const std::shared_ptr<analysis::diagnostics_check>& check,
		channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> channels
	) -> void;

	auto update_diagnostics(
		context& ctx,
		channel_read<analysis::diagnostics_completed, build_runner::build_finished> diag_in,
		channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> code_out,
		workspace::data& ws,
		shared_view<config_system::data> config,
		bool building
	) -> void;

	auto audit_semantic_coverage(
		const document& doc,
		const search::index_state* index
	) -> void;

	auto point_in_triangle(
		vec2f p,
		vec2f a,
		vec2f b,
		vec2f c
	) -> bool;

	auto hover_kept_alive(
		const hover_state& h,
		vec2f mouse
	) -> bool;

	struct panel_code_hit {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		std::string_view row;
		std::string_view ident;
		bool member_access = false;
	};

	auto hover_code_line_height(
		const gui::draw_context& ctx,
		float scale
	) -> float;

	auto hover_panel_code_hit(
		const gui::draw_context& ctx,
		const hover_state& h,
		vec2f mouse
	) -> std::optional<panel_code_hit>;

	auto highlight_code_body(
		const docs::doc_card& card,
		const search::index_state* index
	) -> std::vector<gui::text_span>;

	struct resolved_symbol {
		std::string qualified;
		std::string kind;
		std::string type;
		std::string value;
		std::optional<search::location> def;
	};

	auto resolve_hover_card(
		hover_state& h,
		std::string_view ident,
		std::string_view row,
		std::size_t ident_start,
		bool declaration_fallback,
		const search::index_state* index,
		docs::cppref_index& cppref,
		std::span<const search::lookup_error> issues,
		resolved_symbol resolved
	) -> void;

	struct module_link {
		std::string name;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
	};

	auto module_name_at(
		std::string_view row,
		std::uint32_t column
	) -> std::optional<module_link>;

	auto draw_code_line(
		const gui::draw_context& ctx,
		std::string_view text,
		std::span<const gui::text_span> spans,
		std::uint32_t line,
		vec2f origin,
		float scale,
		vec4f fallback,
		const rectf& clip,
		std::uint32_t z
	) -> void;

	auto diagnostic_targets_document(
		const diagnostic& diag,
		const std::filesystem::path& path
	) -> bool;
}

auto gse::ide::diagnostic_targets_document(const diagnostic& diag, const std::filesystem::path& path) -> bool {
	return diag.file.empty() || diag.file == path;
}

auto gse::ide::refresh_rendered_view(const gui::draw_context& ctx, document& doc, const rectf& rect) -> void {
	const float width = std::max(0.f, rect.width() - ctx.style.padding * 2.f);
	const std::uint64_t key = gui::style_key(ctx.style);
	if (doc.rendered.built
		&& doc.rendered.revision == doc.revision
		&& doc.rendered.width == width
		&& doc.rendered.font_size == ctx.style.font_size
		&& doc.rendered.style_key == key) {
		return;
	}

	doc.rendered.content = markdown::render_document(doc.buffer.text(), {
		.fonts = ctx.fonts,
		.sty = ctx.style,
		.font_size = ctx.style.font_size,
		.content_width = width,
	});
	doc.rendered.revision = doc.revision;
	doc.rendered.width = width;
	doc.rendered.font_size = ctx.style.font_size;
	doc.rendered.style_key = key;
	doc.rendered.built = true;
}

auto gse::ide::hover_wrap_lines(const gui::draw_context& ctx, const std::string_view text, const float max_width, const float scale) -> std::vector<std::string> {
	const auto text_view = ctx.fonts.text.resolve();
	std::vector<std::string> out;
	std::size_t para_start = 0;
	while (para_start <= text.size()) {
		const std::size_t nl = text.find('\n', para_start);
		const std::string_view para = text.substr(para_start, (nl == std::string_view::npos ? text.size() : nl) - para_start);
		std::string current;
		std::size_t i = 0;
		while (i < para.size()) {
			const std::size_t sp = para.find(' ', i);
			const std::string_view word = para.substr(i, (sp == std::string_view::npos ? para.size() : sp) - i);
			i = (sp == std::string_view::npos) ? para.size() : sp + 1;
			if (word.empty()) {
				continue;
			}
			std::string candidate = current.empty() ? std::string(word) : current + " " + std::string(word);
			if (!current.empty() && text_view->width(candidate, scale) > max_width) {
				out.push_back(std::move(current));
				current = std::string(word);
			}
			else {
				current = std::move(candidate);
			}
		}
		if (!current.empty()) {
			out.push_back(std::move(current));
		}
		if (nl == std::string_view::npos) {
			break;
		}
		para_start = nl + 1;
	}
	if (out.size() > 14) {
		out.resize(14);
		out.back() += " ...";
	}
	return out;
}

auto gse::ide::draw_code_line(const gui::draw_context& ctx, const std::string_view text, const std::span<const gui::text_span> spans, const std::uint32_t line, const vec2f origin, const float scale, const vec4f fallback, const rectf& clip, const std::uint32_t z) -> void {
	const auto code_view = ctx.fonts.code.resolve();
	const std::vector<float> offsets = code_view->caret_offsets(text, scale);
	const float baseline = origin.y() + code_view->vertical_center_offset(scale);
	const std::uint32_t len = static_cast<std::uint32_t>(text.size());
	std::uint32_t pos = 0;
	for (const gui::text_span& sp : spans) {
		if (sp.line != line) {
			continue;
		}
		const std::uint32_t a = std::min<std::uint32_t>(sp.start_col, len);
		const std::uint32_t b = std::min<std::uint32_t>(sp.end_col, len);
		if (a > pos) {
			ctx.queue_text({ .font = ctx.fonts.code, .text = text.substr(pos, a - pos), .position = { origin.x() + offsets[pos], baseline }, .scale = scale, .color = fallback, .clip_rect = clip, .z_order = z });
		}
		if (b > a) {
			ctx.queue_text({ .font = ctx.fonts.code, .text = text.substr(a, b - a), .position = { origin.x() + offsets[a], baseline }, .scale = scale, .color = sp.color, .clip_rect = clip, .z_order = z });
		}
		if (b > pos) {
			pos = b;
		}
	}
	if (pos < len) {
		ctx.queue_text({ .font = ctx.fonts.code, .text = text.substr(pos), .position = { origin.x() + offsets[pos], baseline }, .scale = scale, .color = fallback, .clip_rect = clip, .z_order = z });
	}
}

auto gse::ide::point_in_triangle(const vec2f p, const vec2f a, const vec2f b, const vec2f c) -> bool {
	const float d1 = (p.x() - b.x()) * (a.y() - b.y()) - (a.x() - b.x()) * (p.y() - b.y());
	const float d2 = (p.x() - c.x()) * (b.y() - c.y()) - (b.x() - c.x()) * (p.y() - c.y());
	const float d3 = (p.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (p.y() - a.y());
	const bool neg = d1 < 0.f || d2 < 0.f || d3 < 0.f;
	const bool pos = d1 > 0.f || d2 > 0.f || d3 > 0.f;
	return !(neg && pos);
}

auto gse::ide::hover_kept_alive(const hover_state& h, const vec2f mouse) -> bool {
	if (h.panel.width() <= 0.f) {
		return false;
	}
	if (h.x_axis.held || h.y_axis.held) {
		return true;
	}
	constexpr float grace = 16.f;
	if (mouse.x() >= h.panel.left() - grace && mouse.x() <= h.panel.right() + grace
		&& mouse.y() >= h.panel.bottom() - grace && mouse.y() <= h.panel.top() + grace) {
		return true;
	}
	const bool left = h.anchor.x() <= h.panel.left();
	const bool right = h.anchor.x() >= h.panel.right();
	const bool above = h.anchor.y() >= h.panel.top();
	const bool below = h.anchor.y() <= h.panel.bottom();
	vec2f c0;
	vec2f c1;
	if ((left && above) || (right && below)) {
		c0 = h.panel.top_right();
		c1 = h.panel.bottom_left();
	}
	else if ((right && above) || (left && below)) {
		c0 = h.panel.top_left();
		c1 = h.panel.bottom_right();
	}
	else if (left) {
		c0 = h.panel.top_left();
		c1 = h.panel.bottom_left();
	}
	else if (right) {
		c0 = h.panel.top_right();
		c1 = h.panel.bottom_right();
	}
	else if (above) {
		c0 = h.panel.top_left();
		c1 = h.panel.top_right();
	}
	else {
		c0 = h.panel.bottom_left();
		c1 = h.panel.bottom_right();
	}
	return point_in_triangle(mouse, h.anchor, c0, c1);
}

auto gse::ide::hover_code_line_height(const gui::draw_context& ctx, const float scale) -> float {
	return ctx.fonts.code.resolve()->line_height(scale) * 1.25f;
}

auto gse::ide::hover_panel_code_hit(const gui::draw_context& ctx, const hover_state& h, const vec2f mouse) -> std::optional<panel_code_hit> {
	if (!h.body_is_code || h.code_rect.width() <= 0.f || !h.code_rect.contains(mouse)) {
		return std::nullopt;
	}
	const auto code_view = ctx.fonts.code.resolve();
	const float fs = ctx.style.font_size;
	const float line_h = hover_code_line_height(ctx, fs);
	const float rel = (h.code_rect.top() + h.scroll - mouse.y()) / line_h;
	if (rel < 0.f) {
		return std::nullopt;
	}
	const std::size_t li = static_cast<std::size_t>(rel);
	std::string_view row;
	std::size_t cs = 0;
	std::size_t current = 0;
	bool found = false;
	while (cs <= h.body.size()) {
		const std::size_t nl = h.body.find('\n', cs);
		if (current == li) {
			row = std::string_view(h.body).substr(cs, (nl == std::string::npos ? h.body.size() : nl) - cs);
			found = true;
			break;
		}
		if (nl == std::string::npos) {
			break;
		}
		cs = nl + 1;
		++current;
	}
	if (!found) {
		return std::nullopt;
	}
	const std::vector<float> offsets = code_view->caret_offsets(row, fs);
	const float x = mouse.x() - (h.panel.left() + ctx.style.padding) + h.scroll_x;
	std::size_t col = row.size();
	for (std::size_t j = 0; j + 1 < offsets.size(); ++j) {
		if (x >= offsets[j] && x < offsets[j + 1]) {
			col = j;
			break;
		}
	}
	if (col >= row.size()) {
		return std::nullopt;
	}
	std::size_t a = col;
	std::size_t b = col;
	while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
		--a;
	}
	while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
		++b;
	}
	if (b == a || std::isdigit(static_cast<unsigned char>(row[a]))) {
		return std::nullopt;
	}
	return panel_code_hit{
		.line = static_cast<std::uint32_t>(li),
		.start_col = static_cast<std::uint32_t>(a),
		.end_col = static_cast<std::uint32_t>(b),
		.row = row,
		.ident = row.substr(a, b - a),
		.member_access = a > 0 && (row[a - 1] == '.' || (a >= 2 && row[a - 1] == '>' && row[a - 2] == '-')),
	};
}

auto gse::ide::highlight_code_body(const docs::doc_card& card, const search::index_state* index) -> std::vector<gui::text_span> {
	constexpr std::uint32_t no_line = std::numeric_limits<std::uint32_t>::max();
	syntax_producer::semantic_data sem;
	if (index && !card.source.empty() && !card.line_source.empty()) {
		std::uint32_t lo = no_line;
		std::uint32_t hi = 0;
		for (const std::uint32_t s : card.line_source) {
			if (s == no_line) {
				continue;
			}
			lo = std::min(lo, s);
			hi = std::max(hi, s);
		}
		if (lo != no_line) {
			const std::vector<search::positioned_kind> toks = index->semantic_tokens_in(card.source, lo, hi + 1);
			std::unordered_map<std::uint32_t, std::vector<const search::positioned_kind*>> by_line;
			for (const search::positioned_kind& t : toks) {
				by_line[t.line].push_back(&t);
			}
			for (std::size_t k = 0; k < card.line_source.size() && k < card.line_render.size(); ++k) {
				const std::uint32_t src = card.line_source[k];
				if (src == no_line) {
					continue;
				}
				const auto it = by_line.find(src);
				if (it == by_line.end()) {
					continue;
				}
				const std::string_view render = card.line_render[k];
				for (const search::positioned_kind* t : it->second) {
					if (t->column < card.indent || static_cast<std::size_t>(t->column) + t->length > render.size()) {
						continue;
					}
					std::uint32_t body_col = 0;
					for (std::size_t c = card.indent; c < t->column; ++c) {
						body_col += render[c] == '\t' ? 4u : 1u;
					}
					sem.at[{
						.line = static_cast<std::uint32_t>(k),
						.column = body_col,
					}] = t->kind;
				}
			}
		}
	}
	return syntax_producer::highlight(card.body, sem.at.empty() ? nullptr : &sem, nullptr);
}

auto gse::ide::resolve_hover_card(hover_state& h, const std::string_view ident, const std::string_view row, const std::size_t ident_start, const bool declaration_fallback, const search::index_state* index, docs::cppref_index& cppref, const std::span<const search::lookup_error> issues, resolved_symbol resolved) -> void {
	auto& [qualified, sym_kind, type, value, def] = resolved;
	auto append_issue = [&](std::string issue) {
		if (std::ranges::find(h.issues, issue) == h.issues.end()) {
			h.issues.push_back(std::move(issue));
		}
	};
	std::vector<search::lookup_error> lookup_issues(issues.begin(), issues.end());
	if (qualified.empty()) {
		qualified = docs::expand_qualified(row, ident_start);
	}
	const bool language_word = syntax_producer::is_language_word(qualified);
	if (!language_word && !def && index && declaration_fallback) {
		std::string qualifier;
		if (const std::size_t sep = qualified.rfind("::"); sep != std::string::npos) {
			qualifier = qualified.substr(0, sep + 2);
		}
		if (const std::expected<search::hover_hit, search::lookup_error> decl = index->declaration_of(ident, qualifier)) {
			def = decl->def;
			if (sym_kind.empty()) {
				sym_kind = decl->kind;
			}
			if (!decl->qualified.empty()) {
				qualified = decl->qualified;
			}
			if (type.empty()) {
				type = decl->type;
			}
			if (value.empty()) {
				value = decl->value;
			}
			for (const search::lookup_error& issue : decl->issues) {
				lookup_issues.push_back(issue);
			}
		}
		else {
			lookup_issues.push_back(decl.error());
		}
	}
	if (const std::optional<docs::cppref_hit> cref = cppref.find(docs::normalize_qualified(qualified))) {
		h.title = qualified;
		h.kind = std::string(cref->kind);
		h.body = std::string(cref->brief);
		if (h.body.empty()) {
			append_issue(std::format("cppreference supplied no summary for '{}'", qualified));
		}
		h.url = docs::cppref_url(cref->page);
		h.from_cppref = true;
		h.has_card = true;
	}
	else if (language_word) {
		h.title = qualified;
		h.kind = qualified == "true" || qualified == "false" || qualified == "nullptr" ? "C++ literal" : "C++ keyword";
		h.body = "Reserved C++ language token. Open cppreference for its syntax, language-defined uses, and examples.";
		const std::string page = qualified == "override" || qualified == "final"
			? "cpp/language/" + qualified
			: "cpp/keyword/" + qualified;
		h.url = docs::cppref_url(page);
		h.from_cppref = true;
		h.has_card = true;
	}
	else if (def && !def->path.empty()) {
		if (const std::optional<docs::doc_card> card = docs::extract_definition(def->path, def->line, qualified, sym_kind == "function")) {
			h.title = card->title;
			h.kind = sym_kind;
			h.body = card->body;
			if (h.body.empty()) {
				append_issue(std::format("the extracted definition preview for '{}' was empty", qualified));
			}
			h.body_is_code = true;
			h.code_spans = highlight_code_body(*card, index);
			h.has_card = true;
		}
		else {
			append_issue(std::format("definition preview could not be extracted from {}", def->path.generic_display_string()));
		}
	}
	const bool has_resolved_type = !type.empty();
	if (!type.empty()) {
		const bool has_decl = h.body_is_code && !h.body.empty();
		auto head_hides_type = [](const std::string_view decl) {
			std::string_view head = decl.substr(0, decl.find('\n'));
			head = head.substr(0, head.find('='));
			auto is_ident = [](const char c) {
				return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
			};
			auto has_word = [&](const std::string_view word) {
				for (std::size_t p = head.find(word); p != std::string_view::npos; p = head.find(word, p + 1)) {
					const bool left = p == 0 || !is_ident(head[p - 1]);
					const bool right = p + word.size() >= head.size() || !is_ident(head[p + word.size()]);
					if (left && right) {
						return true;
					}
				}
				return false;
			};
			return has_word("auto") || has_word("decltype");
		};
		if (!has_decl || head_hides_type(h.body)) {
			std::vector<gui::text_span> spans = syntax_producer::highlight(type, nullptr, nullptr);
			std::string body = std::move(type);
			if (has_decl) {
				for (gui::text_span& sp : h.code_spans) {
					++sp.line;
				}
				body.push_back('\n');
				body += h.body;
				spans.insert(spans.end(), h.code_spans.begin(), h.code_spans.end());
			}
			h.body = std::move(body);
			h.code_spans = std::move(spans);
			h.body_is_code = true;
		}
	}
	else if (sym_kind == "variable" || sym_kind == "parameter" || sym_kind == "member") {
		append_issue(search::describe({ .reason = search::lookup_failure::type_not_recorded, .subject = qualified.empty() ? std::string(ident) : qualified }));
	}
	if (!value.empty() && (h.body_is_code || h.body.empty())) {
		const std::string line = "= " + value;
		std::vector<gui::text_span> spans = syntax_producer::highlight(line, nullptr, nullptr);
		std::uint32_t base = 0;
		if (!h.body.empty()) {
			base = static_cast<std::uint32_t>(std::ranges::count(h.body, '\n')) + 1;
			h.body.push_back('\n');
		}
		for (gui::text_span& sp : spans) {
			sp.line += base;
		}
		h.body += line;
		h.code_spans.insert(h.code_spans.end(), spans.begin(), spans.end());
		h.body_is_code = true;
	}
	if (!h.from_cppref) {
		h.pending = !lookup_issues.empty() && std::ranges::all_of(lookup_issues, search::is_pending);
		const search::lookup_evidence evidence{
			.definition = def.has_value(),
			.kind = !sym_kind.empty(),
			.type = has_resolved_type,
		};
		for (const search::lookup_error& issue : lookup_issues) {
			const search::lookup_failure_info info = annotation_from_enum<search::lookup_failure_info>(issue.reason, {});
			if (!(evidence.*info.superseded_by)) {
				append_issue(search::describe(issue));
			}
		}
	}
	if (h.title.empty()) {
		if (h.body.empty() && h.url.empty() && h.issues.empty()) {
			append_issue(std::format("no indexed definition, resolved type, or documentation preview was found for '{}'", qualified.empty() ? ident : std::string_view(qualified)));
		}
		h.title = qualified.empty() ? std::string(ident) : qualified;
	}
	if (h.kind.empty()) {
		h.kind = sym_kind;
	}
	h.has_card = true;
}

auto gse::ide::module_name_at(const std::string_view row, const std::uint32_t column) -> std::optional<module_link> {
	auto is_ident_start = [](const char ch) {
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
	};
	auto is_ident_continue = [&](const char ch) {
		return is_ident_start(ch) || (ch >= '0' && ch <= '9');
	};
	auto skip_ws = [](const std::string_view text, std::size_t pos) {
		while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
			++pos;
		}
		return pos;
	};
	auto consume_keyword = [&](const std::string_view text, std::size_t& pos, const std::string_view keyword) {
		if (text.substr(pos, keyword.size()) != keyword) {
			return false;
		}
		const std::size_t end = pos + keyword.size();
		if (end < text.size() && is_ident_continue(text[end])) {
			return false;
		}
		pos = end;
		return true;
	};
	auto parse_name = [&](std::size_t pos) -> std::optional<module_link> {
		const std::size_t start = pos;
		bool need_ident = true;
		bool saw_colon = false;
		while (pos < row.size()) {
			const char ch = row[pos];
			if (is_ident_start(ch)) {
				++pos;
				while (pos < row.size() && is_ident_continue(row[pos])) {
					++pos;
				}
				need_ident = false;
				continue;
			}
			if (ch == '.' && !need_ident) {
				++pos;
				need_ident = true;
				continue;
			}
			if (ch == ':' && !saw_colon) {
				++pos;
				saw_colon = true;
				need_ident = true;
				continue;
			}
			break;
		}
		if (pos == start || need_ident || column < start || column >= pos) {
			return std::nullopt;
		}
		const std::string name(row.substr(start, pos - start));
		if (name == ":private") {
			return std::nullopt;
		}
		return module_link{
			.name = name,
			.start_col = static_cast<std::uint32_t>(start),
			.end_col = static_cast<std::uint32_t>(pos),
		};
	};

	std::size_t pos = skip_ws(row, 0);
	if (pos >= row.size() || row[pos] == '/' || row[pos] == '*') {
		return std::nullopt;
	}
	if (consume_keyword(row, pos, "export")) {
		pos = skip_ws(row, pos);
	}
	if (consume_keyword(row, pos, "import")) {
		pos = skip_ws(row, pos);
		return parse_name(pos);
	}
	if (consume_keyword(row, pos, "module")) {
		pos = skip_ws(row, pos);
		if (pos >= row.size() || row[pos] == ';') {
			return std::nullopt;
		}
		return parse_name(pos);
	}
	return std::nullopt;
}

auto gse::ide::draw_hover_panel(const gui::draw_context& ctx, const rectf& text_rect, hover_state& h, const std::uint32_t z, const bool input_owner) -> bool {
	const auto text_view = ctx.fonts.text.resolve();
	const auto code_view = ctx.fonts.code.resolve();
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.25f;

	if (h.body_is_code) {
		const float code_line_h = hover_code_line_height(ctx, fs);
		constexpr std::size_t max_visible = 18;
		const float max_w = 640.f * sty.scale_factor;

		std::vector<std::string> header;
		header.push_back(h.title);
		if (!h.kind.empty()) {
			header.push_back(h.kind);
		}

		for (const std::string& issue : h.issues) {
			header.push_back("Unavailable: " + issue);
		}
		std::vector<std::string_view> code;
		std::size_t cs = 0;
		while (cs <= h.body.size()) {
			const std::size_t nl = h.body.find('\n', cs);
			code.push_back(std::string_view(h.body).substr(cs, (nl == std::string::npos ? h.body.size() : nl) - cs));
			if (nl == std::string::npos) {
				break;
			}
			cs = nl + 1;
		}
		const std::size_t visible = std::min(code.size(), max_visible);

		float content_w = 0.f;
		for (const std::string& l : header) {
			content_w = std::max(content_w, code_view->width(l, fs));
		}
		for (const std::string_view l : code) {
			content_w = std::max(content_w, code_view->width(l, fs));
		}
		const float header_rows = static_cast<float>(header.size());
		const float pw = std::min(max_w, content_w) + pad * 2.f;
		const float ph = (header_rows + static_cast<float>(visible)) * code_line_h + pad * 2.f;

		const float area_top = text_rect.top();
		const float area_bottom = text_rect.top() - text_rect.height();
		float px = h.anchor.x() + 16.f * sty.scale_factor;
		if (px + pw > text_rect.right()) {
			px = text_rect.right() - pw;
		}
		if (px < text_rect.left()) {
			px = text_rect.left();
		}
		float top_y = h.anchor.y() - 16.f * sty.scale_factor;
		if (top_y - ph < area_bottom) {
			top_y = h.anchor.y() + 16.f * sty.scale_factor + ph;
		}
		if (top_y > area_top) {
			top_y = area_top;
		}

		const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
		h.panel = panel;

		const float view_h = static_cast<float>(visible) * code_line_h;
		const float content_h = static_cast<float>(code.size()) * code_line_h;
		const float max_scroll = std::max(0.f, content_h - view_h);
		const float view_w = pw - pad * 2.f;
		const float max_scroll_x = std::max(0.f, content_w - view_w);
		const float char_w = code_view->width("0", fs);
		const vec2f wheel = input_owner ? ctx.scroll_delta_for(panel) : vec2f{};
		const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
		const float wheel_y = shift ? 0.f : wheel.y();
		const float wheel_x = wheel.x() + (shift ? wheel.y() : 0.f);
		h.scroll = std::clamp(h.scroll - wheel_y * code_line_h * 2.f, 0.f, max_scroll);
		h.scroll_x = std::clamp(h.scroll_x - wheel_x * char_w * 4.f, 0.f, max_scroll_x);

		const auto scope = ctx.scoped_layer(render_layer::popup);
		ctx.queue_sprite({
			.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
			.color = sty.color_shadow,
			.texture = ctx.blank_texture,
			.z_order = z,
			.corner_radius = sty.corner_radius_menu,
		});
		ctx.queue_sprite({
			.rect = panel,
			.color = { vec3f(sty.color_menu_body), 1.f },
			.texture = ctx.blank_texture,
			.z_order = z,
			.corner_radius = sty.corner_radius_menu,
		});

		for (std::size_t i = 0; i < header.size(); ++i) {
			const float center_y = top_y - pad - code_line_h * (static_cast<float>(i) + 0.5f);
			ctx.queue_text({
				.font = ctx.fonts.code,
				.text = header[i],
				.position = { px + pad - h.scroll_x, center_y + code_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = i == 0 ? sty.color_text : (i == 1 && !h.kind.empty() ? h.kind_color : sty.color_text_secondary),
				.clip_rect = panel,
				.z_order = z,
			});
		}

		const float code_top = top_y - pad - header_rows * code_line_h;
		const rectf code_rect = rectf::from_position_size({ px, code_top }, { pw, view_h });
		h.code_rect = code_rect;

		const vec2f bar_mouse = ctx.mouse_position();
		const bool bar_pressed = input_owner && ctx.mouse_pressed(mouse_button::button_1) && ctx.input_available();
		const bool bar_held = input_owner && ctx.mouse_held(mouse_button::button_1);
		const float min_thumb = 24.f * sty.scale_factor;

		h.y_axis.offset = h.y_axis.target = h.scroll;
		const gui::scroll_bar_result ybar = gui::update_scroll_bar(h.y_axis, {
			.track_rect = rectf::from_position_size({ px + pw - pad, code_top }, { pad, view_h }),
			.visible_extent = view_h,
			.content_extent = content_h,
			.horizontal = false,
			.mouse = bar_mouse,
			.mouse_pressed = bar_pressed,
			.mouse_held = bar_held,
			.min_thumb = min_thumb,
		});
		h.scroll = h.y_axis.offset;

		h.x_axis.offset = h.x_axis.target = h.scroll_x;
		const gui::scroll_bar_result xbar = gui::update_scroll_bar(h.x_axis, {
			.track_rect = rectf::from_position_size({ px + pad, top_y - ph + pad }, { view_w, pad }),
			.visible_extent = view_w,
			.content_extent = content_w,
			.horizontal = true,
			.mouse = bar_mouse,
			.mouse_pressed = bar_pressed,
			.mouse_held = bar_held,
			.min_thumb = min_thumb,
		});
		h.scroll_x = h.x_axis.offset;

		if (xbar.used_press || ybar.used_press) {
			ctx.consume_press(mouse_button::button_1);
		}

		for (std::size_t li = 0; li < code.size(); ++li) {
			const float center_y = code_top - code_line_h * (static_cast<float>(li) + 0.5f) + h.scroll;
			if (center_y > code_top + code_line_h || center_y < code_top - view_h - code_line_h) {
				continue;
			}
			draw_code_line(ctx, code[li], h.code_spans, static_cast<std::uint32_t>(li), { px + pad - h.scroll_x, center_y }, fs, sty.color_text, code_rect, z);
		}

		const float thumb_thin = 3.f * sty.scale_factor;
		if (ybar.visible) {
			const rectf t = ybar.thumb_rect;
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ t.left() + (t.width() - thumb_thin) * 0.5f, t.top() }, { thumb_thin, t.height() }),
				.color = h.y_axis.held ? sty.color_accent : (ybar.hovered ? sty.color_text_secondary : sty.color_border),
				.texture = ctx.blank_texture,
				.clip_rect = panel,
				.z_order = z,
			});
		}

		if (xbar.visible) {
			const rectf t = xbar.thumb_rect;
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ t.left(), t.top() - (t.height() - thumb_thin) * 0.5f }, { t.width(), thumb_thin }),
				.color = h.x_axis.held ? sty.color_accent : (xbar.hovered ? sty.color_text_secondary : sty.color_border),
				.texture = ctx.blank_texture,
				.clip_rect = panel,
				.z_order = z,
			});
		}

		return false;
	}

	const float max_w = 460.f * sty.scale_factor;

	std::vector<std::string> lines;
	std::vector<int> roles;
	lines.push_back(h.title);
	roles.push_back(0);
	if (!h.kind.empty()) {
		lines.push_back(h.kind);
		roles.push_back(1);
	}
	if (!h.body.empty() && h.body != h.title) {
		for (std::string& bl : hover_wrap_lines(ctx, h.body, max_w, fs)) {
			lines.push_back(std::move(bl));
			roles.push_back(2);
		}
	}
	for (const std::string& issue : h.issues) {
		const std::string prefix = h.pending ? "Pending: " : "Unavailable: ";
		for (std::string& wrapped : hover_wrap_lines(ctx, prefix + issue, max_w, fs)) {
			lines.push_back(std::move(wrapped));
			roles.push_back(2);
		}
	}
	if (!h.url.empty()) {
		lines.emplace_back("cppreference");
		roles.push_back(3);
	}

	float content_w = 0.f;
	for (const std::string& l : lines) {
		content_w = std::max(content_w, text_view->width(l, fs));
	}
	const float pw = std::min(max_w, content_w) + pad * 2.f;
	const float ph = line_h * static_cast<float>(lines.size()) + pad * 2.f;

	const float area_top = text_rect.top();
	const float area_bottom = text_rect.top() - text_rect.height();
	float px = h.anchor.x() + 16.f * sty.scale_factor;
	if (px + pw > text_rect.right()) {
		px = text_rect.right() - pw;
	}
	if (px < text_rect.left()) {
		px = text_rect.left();
	}
	float top_y = h.anchor.y() - 16.f * sty.scale_factor;
	if (top_y - ph < area_bottom) {
		top_y = h.anchor.y() + 16.f * sty.scale_factor + ph;
	}
	if (top_y > area_top) {
		top_y = area_top;
	}

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	h.panel = panel;
	const vec2f mouse = ctx.mouse_position();
	bool link_clicked = false;

	const auto scope = ctx.scoped_layer(render_layer::popup);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.z_order = z,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.z_order = z,
		.corner_radius = sty.corner_radius_menu,
	});
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
		vec4f col = sty.color_text_secondary;
		if (roles[i] == 0) {
			col = sty.color_text;
		}
		else if (roles[i] == 1) {
			col = h.kind_color;
		}
		else if (roles[i] == 3) {
			const rectf link_rect = rectf::from_position_size({ px + pad, center_y + line_h * 0.5f }, { text_view->width(lines[i], fs), line_h });
			const bool over = ctx.hovers(link_rect);
			col = over ? sty.color_text : sty.color_accent;
			if (ctx.mouse_pressed_for(link_rect)) {
				link_clicked = true;
			}
		}
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = lines[i],
			.position = { px + pad, center_y + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = col,
			.clip_rect = panel,
			.z_order = z,
		});
	}
	return link_clicked;
}

auto gse::ide::draw_diagnostic_tooltip(const gui::draw_context& ctx, const rectf& text_rect, const std::string_view label, const std::string_view message, const vec4f label_color, const vec2f anchor) -> void {
	const auto text_view = ctx.fonts.text.resolve();
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.25f;
	const float max_w = 460.f * sty.scale_factor;

	std::vector<std::string> lines;
	lines.emplace_back(label.empty() ? "diagnostic unavailable" : label);
	for (std::string& wrapped : hover_wrap_lines(ctx, message.empty() ? "Unavailable: diagnostic provider supplied no message" : message, max_w, fs)) {
		lines.push_back(std::move(wrapped));
	}

	float content_w = 0.f;
	for (const std::string& l : lines) {
		content_w = std::max(content_w, text_view->width(l, fs));
	}
	const float pw = std::min(max_w, content_w) + pad * 2.f;
	const float ph = line_h * static_cast<float>(lines.size()) + pad * 2.f;

	const float area_top = text_rect.top();
	const float area_bottom = text_rect.top() - text_rect.height();
	float px = anchor.x() + 16.f * sty.scale_factor;
	if (px + pw > text_rect.right()) {
		px = text_rect.right() - pw;
	}
	if (px < text_rect.left()) {
		px = text_rect.left();
	}
	float top_y = anchor.y() - 16.f * sty.scale_factor;
	if (top_y - ph < area_bottom) {
		top_y = anchor.y() + 16.f * sty.scale_factor + ph;
	}
	if (top_y > area_top) {
		top_y = area_top;
	}

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(render_layer::popup);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = lines[i],
			.position = { px + pad, center_y + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = i == 0 ? label_color : sty.color_text,
			.clip_rect = panel,
		});
	}
}

auto gse::ide::apply_quickfix(document& doc, const std::span<const text_edit> edits) -> void {
	if (edits.empty()) {
		return;
	}
	doc.view.undo_stack.push_back({ doc.buffer.lines, doc.view.caret, doc.view.anchor });
	doc.view.redo_stack.clear();
	doc.view.last_edit_kind = quickfix_edit_kind;
	if (fix_engine::apply(doc.buffer.lines, edits) == 0) {
		doc.view.undo_stack.pop_back();
		return;
	}
	doc.view.caret = doc.buffer.clamp(doc.view.caret);
	doc.view.anchor = doc.view.caret;
	++doc.revision.value;
	mark_document_dirty(doc);
	doc.diag_dirty = true;
	doc.highlight_dirty = true;
	doc.edit_clock.emplace();
}

auto gse::ide::apply_lint_edits(workspace::data& ws, const std::span<const lint_file_edits> files) -> std::size_t {
	std::size_t applied = 0;
	for (const lint_file_edits& file : files) {
		const id document_id = workspace::open_file(ws, file.path);
		const auto it = ws.documents.find(document_id);
		if (it == ws.documents.end()) {
			continue;
		}
		apply_quickfix(it->second, file.edits);
		++applied;
	}
	return applied;
}

auto gse::ide::adjust_after_format(gui::buffer_position& p, const std::span<const format::line_edit> edits) -> void {
	for (const format::line_edit& e : edits) {
		if (e.line != p.line) {
			continue;
		}
		const auto old_len = static_cast<std::uint32_t>(e.expected.size());
		const auto new_len = static_cast<std::uint32_t>(e.replacement.size());
		p.column = p.column >= old_len ? p.column - old_len + new_len : std::min(p.column, new_len);
		break;
	}
}

auto gse::ide::apply_format(document& doc, const format::options& opts) -> void {
	const std::vector<format::line_edit> edits = format::compute(doc.buffer.lines, opts);
	if (edits.empty()) {
		return;
	}
	doc.view.undo_stack.push_back({ doc.buffer.lines, doc.view.caret, doc.view.anchor });
	doc.view.redo_stack.clear();
	doc.view.last_edit_kind = format_edit_kind;
	if (format::apply(doc.buffer.lines, edits) == 0) {
		doc.view.undo_stack.pop_back();
		return;
	}
	adjust_after_format(doc.view.caret, edits);
	adjust_after_format(doc.view.anchor, edits);
	doc.view.caret = doc.buffer.clamp(doc.view.caret);
	doc.view.anchor = doc.buffer.clamp(doc.view.anchor);
	++doc.revision.value;
	mark_document_dirty(doc);
	doc.diag_dirty = true;
	doc.highlight_dirty = true;
	doc.edit_clock.emplace();
}

auto gse::ide::mark_document_dirty(document& doc) -> void {
	if (doc.persistence == document_persistence::clean) {
		doc.persistence = document_persistence::dirty;
	}
}

auto gse::ide::format_and_save(workspace::data& ws, const format_save_request& request, channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> channels) -> document_save_result {
	const auto it = ws.documents.find(request.document_id);
	if (it == ws.documents.end()) {
		return document_save_result::failed;
	}
	document& doc = it->second;
	if (request.format_enabled && doc.language == document_language::cpp) {
		apply_format(doc, request.format_options);
	}
	if (request.force_save || doc.persistence != document_persistence::clean) {
		const document_save_result result = workspace::save_document(ws, request.document_id);
		if (result == document_save_result::saved) {
			channels.push<git_system::refresh_request>({});
		}
		return result;
	}
	return document_save_result::unchanged;
}

auto gse::ide::apply_diagnostics(workspace::data& ws, const std::shared_ptr<analysis::diagnostics_check>& check, channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> channels) -> void {
	const auto pending = ws.documents.find(check->document_id);
	if (pending == ws.documents.end()) {
		return;
	}

	document& doc = pending->second;
	if (doc.revision != check->revision) {
		return;
	}
	doc.analysis_status = check->status;
	doc.analysis_detail = std::move(check->failure_output);
	doc.analysis_duration = check->duration;
	if (doc.analysis_status == analysis::diagnostics_status::cancelled) {
		doc.diag_dirty = true;
		return;
	}
	if (doc.analysis_status != analysis::diagnostics_status::success && doc.analysis_status != analysis::diagnostics_status::plugin_unavailable) {
		doc.diagnostics.clear();
		doc.lint.clear();
		syntax_producer::clear_semantic(doc.syntax);
		doc.highlight_dirty = true;
		return;
	}

	doc.diagnostics = std::move(check->result);
	doc.lint = std::move(check->lint);
	syntax_producer::set_semantic(
		doc.syntax,
		{
			.tokens = check->tokens,
			.buffer = doc.buffer,
			.revision = doc.revision,
			.source_path = doc.path.generic_display_string(),
		}
	);
	doc.highlight_dirty = true;

	if (check->symbols_complete) {
		channels.push<search::index_merge_request>({
			.path = doc.path,
			.symbols = std::move(check->symbols),
			.refs = std::move(check->refs),
			.params = std::move(check->params),
			.files = check->files,
		});
	}

	for (const diagnostic& diag : doc.diagnostics) {
		const log::level level = diag.level == severity::error
			? log::level::error
			: (diag.level == severity::warning ? log::level::warning : log::level::info);
		const std::filesystem::path diagnostic_path = diag.file.empty()
			? doc.path
			: diag.file;
		std::error_code relative_ec;
		const std::filesystem::path relative_path = std::filesystem::relative(diagnostic_path, config::worktree_for(diagnostic_path).project_root, relative_ec);
		const std::string where = relative_ec ? diagnostic_path.generic_display_string() : relative_path.generic_display_string();
		log::println(level, log::category::task, "analysis: {}:{}:{}: {}", where, diag.line + 1, diag.start_col + 1, diag.message);
	}
}

auto gse::ide::audit_semantic_coverage(const document& doc, const search::index_state* index) -> void {
	const std::string path = doc.path.generic_display_string();
	if (!index) {
		log::println(log::level::warning, log::category::general, "[semantic] audit of {} skipped: the symbol index is unavailable", path);
		return;
	}
	if (!index->symbols_ready.load(std::memory_order_acquire)) {
		log::println(log::level::warning, log::category::general, "[semantic] audit of {} skipped: the symbol index is still building", path);
		return;
	}

	std::unordered_map<std::uint32_t, std::vector<const gui::text_span*>> spans_by_line;
	for (const gui::text_span& span : doc.syntax.current_spans(doc.revision)) {
		spans_by_line[span.line].push_back(&span);
	}

	constexpr std::size_t report_limit = 200;
	std::size_t misses = 0;
	std::size_t identifiers = 0;

	log::println(log::level::info, log::category::general, "[semantic] auditing highlight coverage of {} ({} lines)", path, doc.buffer.line_count());

	for (std::size_t li = 0; li < doc.buffer.line_count(); ++li) {
		const std::uint32_t line = static_cast<std::uint32_t>(li);
		const std::string_view row = doc.buffer.line(li);
		const auto spans = spans_by_line.find(line);
		for (std::size_t a = 0; a < row.size(); ++a) {
			if (row[a] != '_' && !std::isalpha(static_cast<unsigned char>(row[a]))) {
				continue;
			}
			std::size_t b = a;
			while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
				++b;
			}
			++identifiers;

			bool covered = false;
			if (spans != spans_by_line.end()) {
				for (const gui::text_span* span : spans->second) {
					if (a < span->end_col && b > span->start_col) {
						covered = true;
						break;
					}
				}
			}
			if (covered) {
				a = b - 1;
				continue;
			}

			++misses;
			if (misses > report_limit) {
				a = b - 1;
				continue;
			}

			const std::string_view ident = row.substr(a, b - a);
			std::vector<std::string> causes;
			if (const std::expected<search::hover_hit, search::lookup_error> hit = index->symbol_at(doc.path, line, static_cast<std::uint32_t>(a))) {
				causes.push_back(std::format("the index resolved '{}' here, so the reference exists but the highlight pass produced no span", hit->qualified));
			}
			else {
				causes.push_back(search::describe(hit.error()));
			}
			search::report_lookup_failure(
				*index,
				{
					.file = doc.path,
					.line = line,
					.column = static_cast<std::uint32_t>(a),
					.length = static_cast<std::uint32_t>(b - a),
					.ident = std::string(ident),
					.row = std::string(row),
					.context = "highlight",
				},
				causes
			);
			a = b - 1;
		}
	}

	log::println(
		log::level::info,
		log::category::general,
		"[semantic] audit of {} found {} unhighlighted of {} identifiers{}",
		path,
		misses,
		identifiers,
		misses > report_limit ? std::format(", {} listed above", report_limit) : std::string{}
	);
}

auto gse::ide::update_diagnostics(context& ctx, const channel_read<analysis::diagnostics_completed, build_runner::build_finished> diag_in, const channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> code_out, workspace::data& ws, const shared_view<config_system::data> config, const bool building) -> void {
	for (const analysis::diagnostics_completed& completed : diag_in.of<analysis::diagnostics_completed>()) {
		if (!completed.check) {
			continue;
		}
		apply_diagnostics(ws, completed.check, code_out);
		if (ws.diagnostics_pending == completed.check->document_id) {
			ws.diagnostics_pending.reset();
		}
	}

	if (!diag_in.of<build_runner::build_finished>().empty()) {
		for (document& doc : std::views::values(ws.documents)) {
			if (doc.analysis_status != analysis::diagnostics_status::success) {
				doc.diag_dirty = true;
			}
		}
	}

	ws.diagnostics_paused_for_build = building;

	if (ws.diagnostics_pending) {
		const time stall_limit = seconds(90.f);
		if (ws.diagnostics_clock.elapsed() > stall_limit) {
			const auto stalled = ws.documents.find(*ws.diagnostics_pending);
			log::println(
				log::level::error,
				log::category::task,
				"analysis: no result for {} after {} s - clearing the queue so other files can be analyzed",
				stalled == ws.documents.end() ? std::string("a closed document") : stalled->second.path.filename().generic_display_string(),
				static_cast<int>(ws.diagnostics_clock.elapsed() / seconds(1.f))
			);
			ws.diagnostics_pending.reset();
			if (stalled != ws.documents.end()) {
				stalled->second.diag_dirty = true;
			}
		}
	}

	if (ws.diagnostics_pending || building) {
		return;
	}

	auto ready = [](const document& doc) {
		return doc.language == document_language::cpp
			&& !doc.path.empty()
			&& doc.diag_dirty
			&& doc.persistence != document_persistence::conflicted
			&& (!doc.edit_clock || doc.edit_clock->elapsed() > milliseconds(500));
	};

	auto candidate = ws.documents.end();
	const std::optional<id> active_document_id = workspace::active_document_id(ws);
	if (const auto active = active_document_id ? ws.documents.find(*active_document_id) : ws.documents.end(); active != ws.documents.end() && ready(active->second)) {
		candidate = active;
	}
	else {
		for (const id document_id : ws.documents.order()) {
			const auto document = ws.documents.find(document_id);
			if (document != ws.documents.end() && ready(document->second)) {
				candidate = document;
				break;
			}
		}
	}
	if (candidate == ws.documents.end()) {
		return;
	}

	document& doc = candidate->second;
	if (doc.persistence == document_persistence::dirty) {
		const document_save_result saved = format_and_save(ws, {
			.document_id = candidate->first,
			.format_options = {
				.indent_width = config.indent_width,
				.indent_with_spaces = config.indent_with_spaces,
			},
			.format_enabled = config.format_on_save,
			.force_save = false,
		}, code_out);
		if (saved == document_save_result::failed || saved == document_save_result::conflict) {
			doc.diag_dirty = false;
			return;
		}
	}

	code_out.push<analysis::diagnostics_request>({
		.document_id = candidate->first,
		.revision = doc.revision,
		.compile_commands = ide::config::compile_commands_for(doc.path),
		.file = doc.path,
		.plugin = config::token_plugin(),
		.workspace_roots = ide::config::analysis_roots(),
		.lint_hook = &lint::analyze_check,
	});
	ws.diagnostics_pending = candidate->first;
	ws.diagnostics_clock.reset();
	doc.diag_dirty = false;
}

auto gse::ide::draw_quickfix_tooltip(const gui::draw_context& ctx, const rectf& text_rect, const std::string_view label, const vec4f label_color, const std::string_view message, const std::string_view fix_title, const int fixall_count, const vec2f anchor, const vec2f mouse) -> quickfix_layout {
	const auto text_view = ctx.fonts.text.resolve();
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.25f;
	const float max_w = 460.f * sty.scale_factor;

	std::vector<std::string> info;
	info.emplace_back(label.empty() ? "quick fix unavailable" : label);
	for (std::string& wrapped : hover_wrap_lines(ctx, message.empty() ? "Unavailable: quick-fix provider supplied no diagnostic message" : message, max_w, fs)) {
		info.push_back(std::move(wrapped));
	}

	const std::string fix_line = fix_title.empty() ? "Apply fix (provider supplied no title)" : std::string(fix_title);
	const bool has_fixall = fixall_count > 1;
	const std::string fixall_line = has_fixall ? "Fix all in file  (" + std::to_string(fixall_count) + ")" : std::string{};

	float content_w = text_view->width(fix_line, fs);
	for (const std::string& l : info) {
		content_w = std::max(content_w, text_view->width(l, fs));
	}
	if (has_fixall) {
		content_w = std::max(content_w, text_view->width(fixall_line, fs));
	}

	const std::size_t action_rows = has_fixall ? 2u : 1u;
	const float info_h = line_h * static_cast<float>(info.size());
	const float sep_h = line_h * 0.35f;
	const float actions_h = line_h * static_cast<float>(action_rows);
	const float pw = std::min(max_w, content_w) + pad * 2.f;
	const float ph = info_h + sep_h + actions_h + pad * 2.f;

	const float area_top = text_rect.top();
	const float area_bottom = text_rect.top() - text_rect.height();
	float px = anchor.x() - 8.f * sty.scale_factor;
	if (px + pw > text_rect.right()) {
		px = text_rect.right() - pw;
	}
	if (px < text_rect.left()) {
		px = text_rect.left();
	}
	float top_y = anchor.y() + 6.f * sty.scale_factor;
	if (top_y > area_top) {
		top_y = area_top;
	}
	if (top_y - ph < area_bottom) {
		top_y = area_bottom + ph;
	}

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(render_layer::popup);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	for (std::size_t i = 0; i < info.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = info[i],
			.position = { px + pad, center_y + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = i == 0 ? label_color : sty.color_text,
			.clip_rect = panel,
		});
	}

	auto action_rect = [&](const std::size_t idx) -> rectf {
		const float row_top = top_y - pad - info_h - sep_h - line_h * static_cast<float>(idx);
		return rectf::from_position_size({ px + 2.f * sty.scale_factor, row_top }, { pw - 4.f * sty.scale_factor, line_h });
	};
	auto draw_action = [&](const rectf row, const std::string& text, const vec4f color) {
		if (row.contains(mouse)) {
			ctx.queue_sprite({
				.rect = row,
				.color = sty.color_input_background,
				.texture = ctx.blank_texture,
				.corner_radius = sty.corner_radius_menu,
			});
		}
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = text,
			.position = { px + pad, row.top() - line_h * 0.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = color,
			.clip_rect = panel,
		});
	};

	quickfix_layout layout;
	layout.panel = panel;
	layout.fix_row = action_rect(0);
	draw_action(layout.fix_row, fix_line, sty.color_text);
	if (has_fixall) {
		layout.fixall_row = action_rect(1);
		draw_action(layout.fixall_row, fixall_line, sty.color_text_secondary);
	}
	return layout;
}

auto gse::ide::draw_document_prompt(gui::builder& ui, workspace::data& ws, const rectf& body, channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> channels) -> void {
	if (!ws.pending_document_prompt) {
		return;
	}
	const auto text_view = ui.ctx.fonts.text.resolve();
	const document_prompt prompt = *ws.pending_document_prompt;
	const auto document_it = ws.documents.find(prompt.document_id);
	if (document_it == ws.documents.end()) {
		ws.pending_document_prompt.reset();
		return;
	}

	const gui::draw_context& ctx = ui.ctx;
	const gui::style& style = ctx.style;
	const float pad = style.padding;
	const float font_size = style.font_size;
	const float line_height = text_view->line_height(font_size) * 1.25f;
	const float button_height = text_view->line_height(font_size) + pad;
	const bool conflict = prompt.kind == document_prompt_kind::save_conflict;
	const std::string_view title = conflict ? "File changed on disk" : "Save changes before closing?";
	const std::string message = conflict
		? std::format("Choose which version of '{}' to keep.", document_it->second.tab_name)
		: std::format("'{}' has unsaved changes.", document_it->second.tab_name);

	const auto scope = ctx.scoped_layer(render_layer::modal);
	ctx.register_hit_region(render_layer::modal, body);
	ctx.queue_sprite({
		.rect = body,
		.color = { 0.f, 0.f, 0.f, 0.45f },
		.texture = ctx.blank_texture,
	});

	const float content_width = std::max({ text_view->width(title, font_size), text_view->width(message, font_size), 360.f * style.scale_factor });
	const float dialog_width = content_width + pad * 4.f;
	const float dialog_height = line_height * 2.f + button_height + pad * 4.f;
	const vec2f center = body.center();
	const rectf dialog = rectf::from_position_size(
		{ center.x() - dialog_width * 0.5f, center.y() + dialog_height * 0.5f },
		{ dialog_width, dialog_height }
	);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ dialog.left() + 4.f, dialog.top() - 4.f }, { dialog_width, dialog_height }),
		.color = style.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = style.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = dialog,
		.color = { vec3f(style.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = style.corner_radius_menu,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = title,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_height * 0.5f + text_view->vertical_center_offset(font_size) },
		.scale = font_size,
		.color = style.color_text,
		.clip_rect = dialog,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = message,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_height * 1.5f + text_view->vertical_center_offset(font_size) },
		.scale = font_size,
		.color = style.color_text_secondary,
		.clip_rect = dialog,
	});

	const float button_width = (dialog_width - pad * 4.f) / 3.f;
	const rectf cancel_button = rectf::from_position_size({ dialog.left() + pad, dialog.bottom() + pad + button_height }, { button_width, button_height });
	const rectf secondary_button = rectf::from_position_size({ cancel_button.right() + pad, cancel_button.top() }, { button_width, button_height });
	const rectf primary_button = rectf::from_position_size({ secondary_button.right() + pad, cancel_button.top() }, { button_width, button_height });
	bool cancel = ui.draw<gui::button>({
		.text = "Cancel",
		.rect = cancel_button,
		.key = "##document_prompt_cancel",
	});
	const bool secondary = ui.draw<gui::button>({
		.text = conflict ? "Reload Disk" : "Discard",
		.rect = secondary_button,
		.key = "##document_prompt_secondary",
		.role = conflict ? gui::button_role::standard : gui::button_role::danger,
	});
	const bool primary = ui.draw<gui::button>({
		.text = conflict ? "Overwrite Disk" : "Save",
		.rect = primary_button,
		.key = "##document_prompt_primary",
		.role = conflict ? gui::button_role::danger : gui::button_role::accent,
	});
	if (ctx.key_pressed_for(key::escape)) {
		ctx.consume_key_press(key::escape);
		cancel = true;
	}

	if (primary) {
		const document_save_result result = workspace::save_document(
			ws,
			prompt.document_id,
			conflict ? document_save_mode::overwrite_external : document_save_mode::preserve_external
		);
		if (result == document_save_result::saved || result == document_save_result::unchanged) {
			channels.push<git_system::refresh_request>({});
			if (prompt.close_after_resolution || !conflict) {
				workspace::discard_document(ws, prompt.document_id);
			}
			else {
				ws.pending_document_prompt.reset();
			}
		}
		else if (result == document_save_result::conflict && ws.pending_document_prompt) {
			ws.pending_document_prompt->close_after_resolution = true;
		}
	}
	else if (secondary) {
		if (!conflict || workspace::discard_document_changes(ws, prompt.document_id)) {
			if (!conflict || prompt.close_after_resolution) {
				workspace::discard_document(ws, prompt.document_id);
			}
			else {
				ws.pending_document_prompt.reset();
			}
		}
	}
	else if (cancel) {
		ws.pending_document_prompt.reset();
	}
}

auto gse::ide::draw_game_placeholder(const gui::draw_context& ctx, const rectf& rect, const std::string_view message) -> void {
	const auto text_view = ctx.fonts.text.resolve();
	const float font_sz = ctx.style.font_size;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = message,
		.position = {
			rect.center().x() - text_view->width(message, font_sz) * 0.5f,
			rect.center().y() + text_view->vertical_center_offset(font_sz),
		},
		.scale = font_sz,
		.color = ctx.style.color_text_secondary,
		.clip_rect = rect,
	});
}

auto gse::ide::draw_code_panel(gui::builder& ui, workspace::data& ws, channel_write<analysis::diagnostics_request, build_runner::build_request, git_system::refresh_request, jump_to_request, set_cursor_shape_request, search::index_merge_request> channels, const code_panel_inputs& inputs) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}
	const shared_view<config_system::data> config = inputs.config;
	const search::index_state* index = inputs.index;
	const bool building = inputs.building;
	const auto text_view = ctx.fonts.text.resolve();
	const auto code_view = ctx.fonts.code.resolve();
	const rectf body = ctx.clip_stack.back();
	const float pad = ctx.style.padding;
	const float font_sz = ctx.style.font_size;

	const std::optional<id> analyzing_id = ws.diagnostics_pending;

	const float tab_gap = 2.f * ctx.style.scale_factor;

	const float document_area_width = std::max(0.f, body.width() - tab_gap);

	std::vector<gui::tab_desc> tab_descs;
	tab_descs.reserve(ws.documents.order().size());
	for (const id id : ws.documents.order()) {
		const document& doc = ws.documents.at(id);
		tab_descs.push_back({
			.tab_id = id,
			.caption = doc.tab_name,
			.dirty = doc.persistence != document_persistence::clean,
			.busy = analyzing_id == id,
		});
	}

	const gui::tab_strip_metrics metrics = gui::tab_strip_measure(ctx.style, {
		.font = ctx.fonts.text,
		.tabs = tab_descs,
		.available_extent = document_area_width,
	});
	ws.tab_strip.visible_rows = std::clamp(ws.tab_strip.visible_rows, 1u, std::max(1u, metrics.required_rows));

	const float tab_bar_h = std::min(body.height(), gui::tab_strip_extent(metrics, ws.tab_strip.visible_rows));
	const rectf tab_bar = rectf::from_position_size(
		{ body.left(), body.top() },
		{ body.width(), tab_bar_h }
	);
	const rectf document_tab_bar = rectf::from_position_size(
		{ tab_bar.left(), tab_bar.top() },
		{ document_area_width, tab_bar.height() }
	);
	ctx.queue_sprite({
		.rect = tab_bar,
		.color = ctx.style.color_tab_background,
		.texture = ctx.blank_texture,
	});

	const vec2f mouse = ctx.mouse_position();

	const gui::tab_strip_result doc_tabs = gui::tab_strip(ctx, {
		.area = document_tab_bar,
		.tabs = tab_descs,
		.active = workspace::active_tab_id(ws),
		.orientation = gui::tab_orientation::horizontal,
		.overflow = gui::tab_overflow::wrap,
		.allow_reorder = true,
	}, ws.tab_strip);

	id close_requested;
	if (doc_tabs.activated.exists()) {
		workspace::activate_document(ws, doc_tabs.activated);
	}
	if (doc_tabs.close_requested.exists()) {
		close_requested = doc_tabs.close_requested;
	}
	if (doc_tabs.context_requested.exists()) {
		workspace::activate_document(ws, doc_tabs.context_requested);
		ctx.open_context_menu({
			.position = doc_tabs.context_position,
			.items = gui::to_menu_items(tab_actions()),
			.target = doc_tabs.context_requested,
			.tag = tab_context_tag(),
		});
	}
	if (doc_tabs.reorder_id.exists()) {
		workspace::reorder_document(ws, doc_tabs.reorder_id, doc_tabs.reorder_to);
	}

	ctx.queue_sprite({
		.rect = rectf::from_position_size(
			{ tab_bar.left(), tab_bar.bottom() },
			{ tab_bar.width(), ctx.style.accent_bar_width }
		),
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});

	if (close_requested.exists()) {
		workspace::close_document(ws, close_requested);
	}
	draw_document_prompt(ui, ws, body, channels);

	const float requested_status_h = text_view->line_height(font_sz) + pad * 0.5f;
	const float content_h = std::max(0.f, body.height() - tab_bar_h);
	const float status_h = std::min(requested_status_h, content_h);
	const float text_h = content_h - status_h;
	const rectf text_rect = rectf::from_position_size(
		{ body.left(), body.top() - tab_bar_h },
		{ body.width(), text_h }
	);
	const rectf status_rect = rectf::from_position_size(
		{ body.left(), body.bottom() + status_h },
		{ body.width(), status_h }
	);


	const std::optional<id> active_document_id = workspace::active_document_id(ws);
	const auto it = active_document_id ? ws.documents.find(*active_document_id) : ws.documents.end();
	if (it == ws.documents.end()) {
		if (text_rect.width() <= 0.f || text_rect.height() <= 0.f) {
			return;
		}
		constexpr std::string_view label = "Open a file from the explorer";
		const float w = text_view->width(label, font_sz);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = label,
			.position = { text_rect.center().x() - w * 0.5f, text_rect.center().y() + text_view->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.color = ctx.style.color_text_secondary,
			.clip_rect = text_rect,
		});
		return;
	}
	document& doc = it->second;

	{
		const bool analyzing = ws.diagnostics_pending == active_document_id;
		const bool failing = !analyzing && doc.analysis_status != analysis::diagnostics_status::success;
		int errors = 0;
		int warnings = 0;
		int elsewhere = 0;
		for (const auto& diagnostic : doc.diagnostics) {
			if (!diagnostic_targets_document(diagnostic, doc.path)) {
				if (diagnostic.level == severity::error) {
					++elsewhere;
				}
				continue;
			}
			if (diagnostic.level == severity::error) {
				++errors;
			}
			else if (diagnostic.level == severity::warning) {
				++warnings;
			}
		}

		ctx.queue_sprite({
			.rect = status_rect,
			.color = ctx.style.color_input_background,
			.texture = ctx.blank_texture,
		});

		float text_left = status_rect.left() + pad;
		std::string status_text;
		vec4f status_color = ctx.style.color_text_secondary;
		bool showing_failure = false;
		const analysis::diagnostics_status_info status_details = failing
			? analysis::status_info(doc.analysis_status)
			: analysis::diagnostics_status_info{};

		if (!doc.persistence_error.empty()) {
			status_text = doc.persistence_error;
			status_color = ctx.style.color_error;
		}
		else if (doc.persistence == document_persistence::conflicted) {
			status_text = "file changed on disk - save to choose reload or overwrite";
			status_color = ctx.style.color_error;
		}
		else if (analyzing) {
			const float glyph_size = status_rect.height() * 0.62f;
			const rectf spin_rect = rectf::from_position_size(
				{ status_rect.left() + pad, status_rect.center().y() + glyph_size * 0.5f },
				{ glyph_size, glyph_size }
			);
			gui::symbol::spinner(ctx, spin_rect, gui::symbol::spinner_rotation(), {
				.color = ctx.style.color_text_secondary,
				.extent = ctx.style.icon_extent,
				.clip_rect = spin_rect,
			});
			text_left = spin_rect.right() + pad * 0.5f;
			status_text = "analyzing...";
		}
		else if (ws.diagnostics_paused_for_build && doc.analysis_status == analysis::diagnostics_status::not_analyzed) {
			status_text = "analysis paused - build running";
		}
		else if (failing) {
			status_text = status_details.label;
			status_color = analysis::status_color(status_details);
			showing_failure = true;
		}
		else if (doc.language == document_language::cpp) {
			const int ms = static_cast<int>(doc.analysis_duration / milliseconds(1.f));
			const std::string prefix = ms > 0 ? "Analyzed in " + std::to_string(ms) + " ms - " : "";
			const std::string elsewhere_note = elsewhere > 0
				? std::format(", {} {} in other files", elsewhere, elsewhere == 1 ? "error" : "errors")
				: std::string{};
			if (errors > 0) {
				status_text = prefix + std::to_string(errors) + (errors == 1 ? " error" : " errors") + (warnings > 0 ? ", " + std::to_string(warnings) + (warnings == 1 ? " warning" : " warnings") : "") + elsewhere_note;
				status_color = ctx.style.color_error;
			}
			else if (warnings > 0) {
				status_text = prefix + std::to_string(warnings) + (warnings == 1 ? " warning" : " warnings") + elsewhere_note;
				status_color = ctx.style.color_warning;
			}
			else if (elsewhere > 0) {
				status_text = prefix + "no issues in this file" + elsewhere_note;
				status_color = ctx.style.color_warning;
			}
			else {
				status_text = prefix + "no issues";
				status_color = ctx.style.color_added;
			}
		}

		if (!status_text.empty()) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = status_text,
				.position = { text_left, status_rect.center().y() + text_view->vertical_center_offset(font_sz) },
				.scale = font_sz,
				.color = status_color,
				.clip_rect = status_rect,
			});
		}

		if (doc.language == document_language::markdown) {
			const std::string_view label = doc.mode == document_view::rendered ? "rendered" : "source";
			const float label_width = text_view->width(label, font_sz);
			const rectf toggle_rect = rectf::from_position_size(
				{ status_rect.right() - label_width - pad * 2.f, status_rect.top() },
				{ label_width + pad * 2.f, status_rect.height() }
			);
			const gui::interaction::press toggle = gui::interaction::press_in_rect(
				ctx,
				ui.hot_widget_id,
				ui.active_widget_id,
				gui::ids::make_from_key(hash_combine(active_document_id->number(), stable_id("doc_view_toggle"))),
				toggle_rect
			);
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = label,
				.position = { toggle_rect.left() + pad, toggle_rect.center().y() + text_view->vertical_center_offset(font_sz) },
				.scale = font_sz,
				.color = toggle.color({
					.idle = ctx.style.color_text_secondary,
					.hot = ctx.style.color_text,
					.active = ctx.style.color_accent,
					.disabled = ctx.style.color_text_disabled,
				}),
				.clip_rect = status_rect,
			});
			if (toggle.activated) {
				doc.mode = doc.mode == document_view::rendered ? document_view::source : document_view::rendered;
			}
		}

		if (showing_failure && ctx.hovers(status_rect)) {
			std::string body = status_details.hint;
			if (!doc.analysis_detail.empty()) {
				body += '\n';
				body += doc.analysis_detail;
			}
			if (doc.analysis_status != analysis::diagnostics_status::not_analyzed) {
				body += "\nSemantic highlighting stays cleared until analysis succeeds again.";
			}
			draw_diagnostic_tooltip(ctx, text_rect, status_text, body, status_color, mouse);
		}
	}

	if (text_rect.width() <= 0.f || text_rect.height() <= 0.f) {
		return;
	}

	syntax_producer::poll(doc.syntax, doc.revision);

	if (doc.language != document_language::plain && doc.highlight_dirty && !doc.syntax.pending
		&& (!doc.edit_clock || doc.edit_clock->elapsed() > milliseconds(120))) {
		syntax_producer::rebuild(doc.syntax, doc.buffer, doc.revision, doc.language);
		doc.highlight_dirty = false;
	}

	struct diag_region {
		std::uint32_t line = 0;
		std::uint32_t byte_start = 0;
		std::uint32_t byte_end = 0;
		vec4f color;
		std::string_view message;
		severity level = severity::error;
		const diagnostic* diag = nullptr;
	};

	std::vector<gui::text_underline> underlines;
	std::vector<gui::text_fade> fades;
	std::vector<diag_region> diag_regions;
	if (!doc.diagnostics.empty() || !doc.lint.empty()) {
		const std::size_t line_count = doc.buffer.line_count();
		auto display_to_byte = [](const std::string_view row, const std::uint32_t display_col) -> std::uint32_t {
			constexpr std::size_t gcc_tab_width = 8;
			std::size_t display = 0;
			for (std::size_t i = 0; i < row.size(); ++i) {
				if (display >= display_col) {
					return static_cast<std::uint32_t>(i);
				}
				display += row[i] == '\t' ? gcc_tab_width - display % gcc_tab_width : 1;
			}
			return static_cast<std::uint32_t>(row.size());
		};
		underlines.reserve(doc.diagnostics.size() + doc.lint.size());
		diag_regions.reserve(doc.diagnostics.size() + doc.lint.size());
		const std::array<std::span<const diagnostic>, 2> diag_sources{ doc.diagnostics, doc.lint };
		for (const std::span<const diagnostic> diag_group : diag_sources)
		for (const auto& diagnostic : diag_group) {
			if (!diagnostic_targets_document(diagnostic, doc.path)) {
				continue;
			}
			if (diagnostic.line >= line_count) {
				continue;
			}
			const vec4f color = diagnostic.level == severity::warning
				? ctx.style.color_warning
				: (diagnostic.level == severity::hint ? vec4f{ 0.45f, 0.51f, 0.58f, 0.9f } : diagnostic.level == severity::note ? vec4f{ 0.43f, 0.50f, 0.64f, 1.f } : ctx.style.color_error);
			const bool faded = diagnostic.rule.has_value() && diagnostic.level == severity::hint;
			const std::uint32_t last_line = std::min<std::uint32_t>(std::max(diagnostic.end_line, diagnostic.line), static_cast<std::uint32_t>(line_count) - 1);
			for (std::uint32_t seg_line = diagnostic.line; seg_line <= last_line; ++seg_line) {
				const std::string_view row = doc.buffer.line(seg_line);
				const std::uint32_t byte_start = seg_line == diagnostic.line ? display_to_byte(row, diagnostic.start_col) : 0;
				std::uint32_t byte_end = seg_line == last_line ? display_to_byte(row, diagnostic.end_col) : static_cast<std::uint32_t>(row.size());
				if (seg_line == diagnostic.line && seg_line == last_line) {
					byte_end = std::max(byte_start + 1, byte_end);
				}
				if (byte_end <= byte_start) {
					continue;
				}
				if (faded) {
					fades.push_back({ .line = seg_line, .start_col = byte_start, .end_col = byte_end, .alpha = 0.45f });
				}
				else {
					underlines.push_back({ .line = seg_line, .start_col = byte_start, .end_col = byte_end, .color = color });
				}
				diag_regions.push_back({ .line = seg_line, .byte_start = byte_start, .byte_end = byte_end, .color = color, .message = diagnostic.message, .level = diagnostic.level, .diag = &diagnostic });
			}
		}
	}

	if (doc.pending_center_line) {
		const float line_h = code_view->line_height(font_sz) * 1.25f;
		const float view_h = std::max(0.f, text_rect.height() - pad * 2.f);
		const float content_h = static_cast<float>(doc.buffer.line_count()) * line_h;
		const float caret_y = static_cast<float>(*doc.pending_center_line) * line_h;
		const float centered = caret_y - view_h * 0.5f + line_h * 0.5f;
		const float target = std::clamp(centered, 0.f, std::max(0.f, content_h - view_h));
		doc.view.scroll.y.offset = target;
		doc.view.scroll.y.target = target;
		doc.pending_center_line.reset();
	}

	const std::size_t display_tab_width = static_cast<std::size_t>(std::max(1, config.indent_width));
	const bool rendered_mode = doc.language == document_language::markdown && doc.mode == document_view::rendered;
	const bool drag_selecting = doc.view.selecting;
	const bool text_hoverable = !rendered_mode && !drag_selecting && ctx.hovers(text_rect) && !doc.buffer.lines.empty();
	const std::span<const gui::text_span> doc_spans = doc.language != document_language::plain
		? doc.syntax.current_spans(doc.revision)
		: std::span<const gui::text_span>{};

	auto position_at = [&](const vec2f at) -> gui::buffer_position {
		return gui::text_area_position_at(ctx, {
			.buffer = doc.buffer,
			.state = doc.view,
			.rect = text_rect,
			.spans = doc_spans,
			.show_line_numbers = config.show_line_numbers,
			.indent_width = display_tab_width,
		}, at);
	};

	const bool goto_ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);
	std::optional<search::location> link_target;
	std::vector<search::lookup_error> link_issues;
	std::string link_subject;
	bool link_attempted = false;
	auto accept_link = [&](std::expected<search::location, search::lookup_error> result) {
		if (result) {
			link_target = *result;
			link_issues.clear();
		}
		else {
			link_issues.push_back(result.error());
		}
	};
	if (goto_ctrl && text_hoverable) {
		const gui::buffer_position hover = position_at(mouse);
		const std::string_view row = doc.buffer.line(hover.line);
		if (const std::optional<module_link> mod = module_name_at(row, hover.column)) {
			link_attempted = true;
			link_subject = mod->name;
			if (index) {
				accept_link(index->module_definition(mod->name, doc.path));
			}
			else {
				link_issues.push_back({ .reason = search::lookup_failure::index_unavailable });
			}
			if (link_target) {
				underlines.push_back({ .line = hover.line, .start_col = mod->start_col, .end_col = mod->end_col, .color = ctx.style.color_text_secondary });
			}
		}
		else {
			std::size_t a = std::min<std::size_t>(hover.column, row.size());
			std::size_t b = a;
			while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
				--a;
			}
			while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
				++b;
			}
			const bool member_access = a > 0 && (row[a - 1] == '.' || (a >= 2 && row[a - 1] == '>' && row[a - 2] == '-'));
			if (b > a && !std::isdigit(static_cast<unsigned char>(row[a]))) {
				link_subject = row.substr(a, b - a);
				if (!syntax_producer::is_language_word(link_subject)) {
					link_attempted = true;
				}
				if (link_attempted && index) {
					accept_link(index->definition_at(doc.path, hover.line, hover.column));
					if (!link_target && !member_access) {
						std::string qualifier;
						std::size_t qp = a;
						while (qp >= 2 && row[qp - 1] == ':' && row[qp - 2] == ':') {
							std::size_t qs = qp - 2;
							while (qs > 0 && (row[qs - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[qs - 1])))) {
								--qs;
							}
							if (qs == qp - 2) {
								break;
							}
							qualifier.insert(0, std::string(row.substr(qs, qp - qs)));
							qp = qs;
						}
						accept_link(index->symbol_definition(row.substr(a, b - a), qualifier, doc.path));
					}
				}
				else if (link_attempted) {
					link_issues.push_back({ .reason = search::lookup_failure::index_unavailable });
				}
				if (link_target) {
					underlines.push_back({ .line = hover.line, .start_col = static_cast<std::uint32_t>(a), .end_col = static_cast<std::uint32_t>(b), .color = ctx.style.color_text_secondary });
				}
			}
		}
	}
	if (link_target) {
		channels.push<set_cursor_shape_request>({ .shape = cursor_shape::hand });
	}
	const bool goto_press = goto_ctrl && link_attempted && ctx.mouse_pressed_for(text_rect);
	const bool goto_click = link_target.has_value() && goto_press;
	if (link_attempted && !link_target) {
		const bool link_pending = !link_issues.empty() && std::ranges::all_of(link_issues, search::is_pending);
		std::string reason;
		for (const search::lookup_error& issue : link_issues) {
			if (!reason.empty()) {
				reason += "\n";
			}
			reason += search::describe(issue);
		}
		if (reason.empty()) {
			reason = "semantic navigation failed without a recorded reason";
		}
		draw_diagnostic_tooltip(
			ctx,
			text_rect,
			link_pending ? "navigation pending" : "navigation unavailable",
			reason,
			link_pending ? ctx.style.color_text_secondary : vec4f{ 0.91f, 0.67f, 0.35f, 1.f },
			mouse
		);
		if (goto_press && !link_pending) {
			log::println(log::level::warning, log::category::task, "semantic ctrl-click '{}': {}", link_subject, reason);
		}
	}

	if (!ws.cppref.loaded) {
		ws.cppref.load(config::cppref_index());
	}
	std::optional<std::size_t> hovered_diag;
	if (!goto_ctrl && !diag_regions.empty() && text_hoverable) {
		const float diag_line_h = code_view->line_height(font_sz) * 1.25f;
		const float content_top = text_rect.top() - pad + doc.view.scroll.y.offset;
		const float row_rel = (content_top - mouse.y()) / diag_line_h;
		if (row_rel >= 0.f && row_rel < static_cast<float>(doc.buffer.line_count())) {
			const gui::buffer_position dp = position_at(mouse);
			for (std::size_t di = 0; di < diag_regions.size(); ++di) {
				const diag_region& dr = diag_regions[di];
				if (dr.line == dp.line && dp.column >= dr.byte_start && dp.column < dr.byte_end) {
					hovered_diag = di;
					break;
				}
			}
		}
	}
	int deepest_alive = -1;
	for (std::size_t i = 0; !drag_selecting && i < ws.hover_stack.size(); ++i) {
		if (ws.hover_stack[i].has_card && hover_kept_alive(ws.hover_stack[i], mouse)) {
			deepest_alive = static_cast<int>(i);
		}
	}
	if (deepest_alive >= 0 && !hovered_diag && !goto_ctrl && text_hoverable) {
		bool over_panel = false;
		for (const hover_state& hv : ws.hover_stack) {
			if (hv.has_card && hv.panel.width() > 0.f && hv.panel.contains(mouse)) {
				over_panel = true;
				break;
			}
		}
		if (!over_panel) {
			const gui::buffer_position hp = position_at(mouse);
			const std::string_view row = doc.buffer.line(hp.line);
			std::size_t a = std::min<std::size_t>(hp.column, row.size());
			std::size_t b = a;
			while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
				--a;
			}
			while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
				++b;
			}
			const hover_state& primary = ws.hover_stack.front();
			if (b > a && !std::isdigit(static_cast<unsigned char>(row[a])) && (primary.line != hp.line || primary.column != static_cast<std::uint32_t>(a))) {
				ws.hover_stack.clear();
				deepest_alive = -1;
			}
		}
	}
	const bool panel_alive = !hovered_diag && deepest_alive >= 0;
	if (!goto_ctrl && !hovered_diag && !panel_alive && text_hoverable) {
		const gui::buffer_position hp = position_at(mouse);
		const std::string_view row = doc.buffer.line(hp.line);
		std::size_t a = std::min<std::size_t>(hp.column, row.size());
		std::size_t b = a;
		while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
			--a;
		}
		while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
			++b;
		}
		const bool member_access = a > 0 && (row[a - 1] == '.' || (a >= 2 && row[a - 1] == '>' && row[a - 2] == '-'));
		if (b > a && !std::isdigit(static_cast<unsigned char>(row[a]))) {
			const std::string_view ident = row.substr(a, b - a);
			const std::optional<module_link> module = module_name_at(row, hp.column);
			if (ws.hover_stack.empty()) {
				ws.hover_stack.emplace_back();
			}
			else if (ws.hover_stack.size() > 1) {
				ws.hover_stack.resize(1);
			}
			hover_state& hv = ws.hover_stack.front();
			if (hv.line != hp.line || hv.column != static_cast<std::uint32_t>(a) || std::string_view(hv.ident) != ident) {
				hv = {};
				hv.line = hp.line;
				hv.column = static_cast<std::uint32_t>(a);
				hv.ident = std::string(ident);
				hv.since = system_clock::now<time>();
			}
			else if (!hv.resolved && system_clock::now<time>() - hv.since > milliseconds(350)) {
				hv.resolved = true;
				std::string qualified;
				std::string sym_kind;
				std::string type;
				std::string value;
				std::optional<search::location> def;
				std::vector<search::lookup_error> lookup_issues;
				bool declaration_fallback = !member_access;
				const syntax_producer::semantic_data* semantic = doc.syntax.current_semantic(doc.revision);
				const analysis::semantic_kind* token_kind = semantic ? semantic->kind_at({
					.line = hp.line,
					.column = static_cast<std::uint32_t>(a),
				}) : nullptr;
				const bool attribute_word = token_kind && *token_kind == analysis::semantic_kind::attribute;
				const bool language_word = syntax_producer::is_language_word(ident);
				if (language_word || attribute_word) {
					qualified = ident;
					declaration_fallback = false;
				}
				else if (module) {
					qualified = module->name;
					sym_kind = "module";
					declaration_fallback = false;
					if (index) {
						if (const std::expected<search::location, search::lookup_error> target = index->module_definition(module->name, doc.path)) {
							def = *target;
						}
						else {
							lookup_issues.push_back(target.error());
						}
					}
					else {
						lookup_issues.push_back({ .reason = search::lookup_failure::index_unavailable });
					}
				}
				else if (index) {
					if (const std::expected<search::hover_hit, search::lookup_error> hit = index->symbol_at(doc.path, hp.line, static_cast<std::uint32_t>(a))) {
						const std::string_view q = hit->qualified;
						const std::size_t qsep = q.rfind("::");
						const std::string_view last = qsep == std::string_view::npos ? q : q.substr(qsep + 2);
						if (last == ident) {
							qualified = hit->qualified;
							sym_kind = hit->kind;
							type = hit->type;
							value = hit->value;
							def = hit->def;
							lookup_issues = hit->issues;
						}
						else {
							lookup_issues.push_back({ .reason = search::lookup_failure::symbol_not_found, .subject = std::string(ident) });
						}
					}
					else {
						lookup_issues.push_back(hit.error());
					}
				}
				else {
					lookup_issues.push_back({ .reason = search::lookup_failure::index_unavailable });
				}
				if (sym_kind.empty() && token_kind) {
					sym_kind = std::format("{}", *token_kind);
				}
				resolve_hover_card(
					hv,
					ident,
					row,
					a,
					declaration_fallback,
					index,
					ws.cppref,
					lookup_issues,
					{
						.qualified = std::move(qualified),
						.kind = std::move(sym_kind),
						.type = std::move(type),
						.value = std::move(value),
						.def = def,
					}
				);
				hv.kind_color = ctx.style.color_text_secondary;
				for (const gui::text_span& sp : doc.syntax.current_spans(doc.revision)) {
					if (sp.line == hp.line && static_cast<std::uint32_t>(a) >= sp.start_col && static_cast<std::uint32_t>(a) < sp.end_col) {
						hv.kind_color = sp.color;
						break;
					}
				}
				hv.anchor = mouse;
				if (!hv.pending && !hv.issues.empty() && index) {
					search::report_lookup_failure(
						*index,
						{
							.file = doc.path,
							.line = hp.line,
							.column = static_cast<std::uint32_t>(a),
							.length = static_cast<std::uint32_t>(b - a),
							.ident = std::string(ident),
							.row = std::string(row),
							.context = "hover",
						},
						hv.issues
					);
				}
			}
		}
		else {
			ws.hover_stack.clear();
		}
	}
	else if (!panel_alive) {
		ws.hover_stack.clear();
	}
	if (!goto_ctrl && !hovered_diag && deepest_alive >= 0 && ctx.input_available()) {
		const std::size_t k = static_cast<std::size_t>(deepest_alive);
		if (ws.hover_stack.size() > k + 2) {
			ws.hover_stack.resize(k + 2);
		}
		if (ws.hover_stack.size() == k + 2 && ws.hover_stack[k + 1].has_card && ws.hover_stack[k + 1].panel.width() > 0.f) {
			ws.hover_stack.resize(k + 1);
		}
		if (const std::optional<panel_code_hit> hit = hover_panel_code_hit(ctx, ws.hover_stack[k], mouse)) {
			const std::string ident(hit->ident);
			const std::string row_text(hit->row);
			if (ws.hover_stack.size() == k + 1) {
				ws.hover_stack.emplace_back();
			}
			hover_state& child = ws.hover_stack[k + 1];
			if (child.line != hit->line || child.column != hit->start_col || child.ident != ident) {
				child = {};
				child.line = hit->line;
				child.column = hit->start_col;
				child.ident = ident;
				child.since = system_clock::now<time>();
			}
			else if (!child.resolved && system_clock::now<time>() - child.since > milliseconds(350)) {
				child.resolved = true;
				resolve_hover_card(child, ident, row_text, hit->start_col, !hit->member_access, index, ws.cppref, {}, {});
				child.kind_color = ctx.style.color_text_secondary;
				for (const gui::text_span& sp : ws.hover_stack[k].code_spans) {
					if (sp.line == hit->line && hit->start_col >= sp.start_col && hit->start_col < sp.end_col) {
						child.kind_color = sp.color;
						break;
					}
				}
				child.anchor = mouse;
				if (!child.pending) {
					for (const std::string& issue : child.issues) {
						log::println(log::level::warning, log::category::general, "[semantic] preview hover miss '{}' inside '{}': {}", ident, ws.hover_stack[k].title, issue);
					}
				}
			}
		}
		else if (ws.hover_stack.size() == k + 2 && !ws.hover_stack[k + 1].has_card) {
			ws.hover_stack.resize(k + 1);
		}
	}
	if (hovered_diag) {
		ws.hover_stack.clear();
		const diag_region& dr = diag_regions[*hovered_diag];
		if (dr.diag && dr.diag->fix.has_value()) {
			if (!ws.quickfix || ws.quickfix->document_id != *active_document_id || ws.quickfix->line != dr.diag->line || ws.quickfix->start_col != dr.diag->start_col) {
				ws.quickfix = quickfix_popup{
					.document_id = *active_document_id,
					.line = dr.diag->line,
					.start_col = dr.diag->start_col,
					.anchor = mouse,
				};
			}
		}
		else {
			ws.quickfix.reset();
			const std::string_view diag_label = enum_to_string(dr.level);
			draw_diagnostic_tooltip(ctx, text_rect, diag_label, dr.message, dr.color, mouse);
		}
	}
	else if (!(ws.quickfix && ws.quickfix->document_id == *active_document_id && ws.quickfix->panel.width() > 0.f && ws.quickfix->panel.contains(mouse))) {
		ws.quickfix.reset();
	}

	if (ws.quickfix && ws.quickfix->document_id == *active_document_id) {
		ws.hover_stack.clear();
		const diagnostic* found = nullptr;
		for (const diagnostic& d : doc.lint) {
			if (d.fix && d.line == ws.quickfix->line && d.start_col == ws.quickfix->start_col) {
				found = &d;
				break;
			}
		}
		if (found) {
			int fixall_count = 0;
			for (const diagnostic& d : doc.lint) {
				if (d.fix && d.rule == found->rule) {
					++fixall_count;
				}
			}
			const std::string_view qlabel = enum_to_string(found->level);
			const quickfix_layout layout = draw_quickfix_tooltip(ctx, text_rect, qlabel, vec4f{ 0.55f, 0.61f, 0.68f, 1.f }, found->message, found->fix->title, fixall_count, ws.quickfix->anchor, mouse);
			ws.quickfix->panel = layout.panel;
			if (ctx.input_available() && ctx.mouse_pressed_for(text_rect)) {
				if (layout.fix_row.contains(mouse)) {
					apply_quickfix(doc, found->fix->edits);
					ws.quickfix.reset();
				}
				else if (layout.fixall_row.width() > 0.f && layout.fixall_row.contains(mouse)) {
					apply_quickfix(doc, fix_engine::rule_edits(doc.lint, *found->rule));
					ws.quickfix.reset();
				}
			}
		}
		else {
			ws.quickfix.reset();
		}
	}
	else if (!hovered_diag && !ws.hover_stack.empty()) {
		int scroll_owner = -1;
		for (std::size_t i = 0; i < ws.hover_stack.size(); ++i) {
			const hover_state& hv = ws.hover_stack[i];
			if (hv.has_card && hv.panel.width() > 0.f && hv.panel.contains(mouse)) {
				scroll_owner = static_cast<int>(i);
			}
		}
		for (std::size_t i = 0; i < ws.hover_stack.size(); ++i) {
			hover_state& hv = ws.hover_stack[i];
			if (!hv.has_card) {
				continue;
			}
			if (draw_hover_panel(ctx, text_rect, hv, ctx.current_z_order + 1 + static_cast<std::uint32_t>(i), static_cast<int>(i) == scroll_owner) && !hv.url.empty()) {
				analysis::process::open_url(hv.url);
			}
		}
	}

	const id text_id = gui::ids::make_from_key(hash_combine(active_document_id->number(), stable_id("doc_text")));
	bool edited = false;
	if (rendered_mode) {
		refresh_rendered_view(ctx, doc, text_rect);
		ui.draw<gui::text_area>({
			.buffer = doc.rendered.content.buffer,
			.state = doc.rendered.view,
			.widget_id = text_id,
			.spans = doc.rendered.content.spans,
			.blocks = doc.rendered.content.blocks,
			.stops = doc.rendered.content.stops,
			.rect = text_rect,
			.read_only = true,
			.indent_width = display_tab_width,
			.blink_interval = time{},
			.font = ctx.fonts.text,
		});
	}
	else {
		edited = ui.draw<gui::text_area>({
			.buffer = doc.buffer,
			.state = doc.view,
			.widget_id = text_id,
			.spans = doc_spans,
			.underlines = underlines,
			.fades = fades,
			.rect = text_rect,
			.show_line_numbers = config.show_line_numbers,
			.indent_width = display_tab_width,
			.indent_with_spaces = config.indent_with_spaces,
			.auto_indent = true,
			.blink_interval = config.caret_blink,
		});
	}
	if (edited) {
		++doc.revision.value;
		mark_document_dirty(doc);
		doc.highlight_dirty = true;
		doc.diag_dirty = true;
		doc.edit_clock.emplace();
	}

	if (goto_click && link_target) {
		workspace::jump_to(ws, {
			.path = link_target->path,
			.line = link_target->line,
			.column = link_target->column,
			.end_line = link_target->line,
			.end_column = link_target->column + static_cast<std::uint32_t>(link_subject.size()),
			.highlight = jump_highlight::span,
		});
		return;
	}

	const format::options format_opts = {
		.indent_width = config.indent_width,
		.indent_with_spaces = config.indent_with_spaces,
	};

	const bool ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);
	if (ui.focus_widget_id == text_id && ctrl && ctx.key_pressed(key::s)) {
		format_and_save(ws, {
			.document_id = *active_document_id,
			.format_options = format_opts,
			.format_enabled = config.format_on_save,
			.force_save = true,
		}, channels);
		doc.diag_dirty = true;
		doc.edit_clock.reset();
	}

	const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
	const bool alt = ctx.key_held(key::left_alt) || ctx.key_held(key::right_alt);

	if (doc.language == document_language::markdown && ctrl && shift && ctx.key_pressed(key::v)
		&& (ui.focus_widget_id == text_id || ctx.hovers(text_rect))) {
		doc.mode = doc.mode == document_view::rendered ? document_view::source : document_view::rendered;
	}

	if (ui.focus_widget_id == text_id && shift && alt && ctx.key_pressed(key::f) && doc.language == document_language::cpp) {
		apply_format(doc, format_opts);
	}

	if (ui.focus_widget_id == text_id && ctrl && shift && ctx.key_pressed(key::h) && doc.language == document_language::cpp) {
		audit_semantic_coverage(doc, index);
	}
}