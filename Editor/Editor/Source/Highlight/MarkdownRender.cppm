export module gse.ide.highlight:markdown_render;

import std;
import gse;

import gse.syntax;
import :markdown;

export namespace gse::ide::markdown {
	enum class family : std::uint8_t {
		proportional,
		monospace,
	};

	struct display_style {
		vec4f color;
		gui::text_face face = gui::text_face::inherit;
		float scale = 1.f;
	};

	struct rendered_run {
		std::size_t start = 0;
		std::size_t end = 0;
		kind tone = kind::body;
	};

	struct rendered_line {
		std::string text;
		std::vector<rendered_run> runs;
	};

	struct rendered_document {
		gui::text_buffer buffer;
		std::vector<gui::text_span> spans;
		std::vector<gui::text_block> blocks;
		std::vector<gui::text_stop> stops;
	};

	struct theme {
		const gui::font_set& fonts;
		const gui::style& sty;
		float font_size = 0.f;
		float content_width = 0.f;
	};

	auto heading_scale(
		std::size_t level
	) -> float;

	auto style_of(
		kind tone,
		family group,
		const gui::style& sty,
		const vec4f& fallback
	) -> display_style;

	auto render_line(
		std::string_view line,
		const line_info& info,
		std::vector<run>& scratch,
		rendered_line& out
	) -> void;

	auto render_document(
		std::string_view source,
		const theme& look
	) -> rendered_document;
}

namespace gse::ide::markdown {
	constexpr std::size_t table_cell_gap = 2;
	constexpr std::string_view rule_glyph = "─";

	auto bullet_for(
		std::string_view marker
	) -> std::string;

	auto render_cells(
		std::string_view line,
		std::vector<run>& scratch,
		std::vector<rendered_line>& out
	) -> void;

	auto measure(
		const rendered_line& text,
		const theme& look,
		const vec4f& fallback
	) -> float;

	auto push_spans(
		rendered_document& doc,
		const rendered_line& text,
		std::uint32_t index,
		const theme& look,
		const display_style& base
	) -> void;
}

auto gse::ide::markdown::heading_scale(const std::size_t level) -> float {
	switch (level) {
		case 1:
			return 1.85f;
		case 2:
			return 1.5f;
		case 3:
			return 1.28f;
		case 4:
			return 1.14f;
		case 5:
			return 1.06f;
		default:
			return 1.f;
	}
}

auto gse::ide::markdown::style_of(const kind tone, const family group, const gui::style& sty, const vec4f& fallback) -> display_style {
	const bool mono = group == family::monospace;
	const gui::text_face body = mono ? gui::text_face::code : gui::text_face::text;
	const gui::text_face strong = mono ? gui::text_face::code_strong : gui::text_face::text_strong;
	const gui::text_face slanted = mono ? gui::text_face::code : gui::text_face::text_emphasis;

	switch (tone) {
		case kind::heading:
			return {
				.color = sty.color_section_header,
				.face = strong,
			};
		case kind::strong:
			return {
				.color = sty.color_text,
				.face = strong,
			};
		case kind::emphasis:
			return {
				.color = sty.color_icon,
				.face = slanted,
			};
		case kind::code:
			return {
				.color = sty.color_file,
				.face = gui::text_face::code,
			};
		case kind::link_text:
		case kind::link_url:
			return {
				.color = sty.color_folder,
				.face = body,
			};
		case kind::quote:
			return {
				.color = sty.color_text_secondary,
				.face = slanted,
			};
		case kind::rule:
		case kind::marker:
			return {
				.color = sty.color_border,
				.face = body,
			};
		case kind::strike:
			return {
				.color = sty.color_text_disabled,
				.face = body,
			};
		default:
			return {
				.color = fallback,
				.face = body,
			};
	}
}

auto gse::ide::markdown::bullet_for(const std::string_view marker) -> std::string {
	const std::string_view body = trim(marker);
	if (body == "-" || body == "*" || body == "+") {
		return "• ";
	}
	if (body == ">") {
		return {};
	}
	return std::string(body) + ' ';
}

auto gse::ide::markdown::render_line(const std::string_view line, const line_info& info, std::vector<run>& scratch, rendered_line& out) -> void {
	out.text.clear();
	out.runs.clear();

	if (verbatim(info.shape) || info.shape == block::blank) {
		out.text.assign(line);
		if (!line.empty()) {
			out.runs.push_back({
				.start = 0,
				.end = line.size(),
				.tone = base_tone(info),
			});
		}
		return;
	}

	out.text.append(info.lead, ' ');
	if (info.shape == block::list_item) {
		out.text += bullet_for(line.substr(info.lead, info.content - info.lead));
	}

	inline_runs(line, info, scratch);

	for (const run& r : scratch) {
		if (r.part == role::marker) {
			continue;
		}
		const std::size_t begin = out.text.size();
		out.text.append(line.substr(r.start, r.end - r.start));
		if (out.text.size() > begin) {
			out.runs.push_back({
				.start = begin,
				.end = out.text.size(),
				.tone = r.tone,
			});
		}
	}
}

auto gse::ide::markdown::render_cells(const std::string_view line, std::vector<run>& scratch, std::vector<rendered_line>& out) -> void {
	out.clear();
	const table_row parsed = split_cells(line);
	if (parsed.ambiguous) {
		return;
	}

	for (const std::string& cell : parsed.cells) {
		rendered_line rendered;
		scratch.clear();
		scan(cell, 0, cell.size(), kind::body, {}, scratch);
		for (const run& r : scratch) {
			if (r.part == role::marker) {
				continue;
			}
			const std::size_t begin = rendered.text.size();
			rendered.text.append(std::string_view(cell).substr(r.start, r.end - r.start));
			if (rendered.text.size() > begin) {
				rendered.runs.push_back({
					.start = begin,
					.end = rendered.text.size(),
					.tone = r.tone,
				});
			}
		}
		out.push_back(std::move(rendered));
	}
}

auto gse::ide::markdown::measure(const rendered_line& text, const theme& look, const vec4f& fallback) -> float {
	float width = 0.f;
	std::size_t written = 0;
	auto advance = [&](const std::size_t from, const std::size_t to, const kind tone) {
		if (to <= from) {
			return;
		}
		const display_style shown = style_of(tone, family::proportional, look.sty, fallback);
		const auto face = look.fonts.face(shown.face, look.fonts.text).resolve();
		width += face->width(std::string_view(text.text).substr(from, to - from), look.font_size * shown.scale);
	};

	for (const rendered_run& r : text.runs) {
		advance(written, r.start, kind::body);
		advance(r.start, r.end, r.tone);
		written = r.end;
	}
	advance(written, text.text.size(), kind::body);
	return width;
}

auto gse::ide::markdown::push_spans(rendered_document& doc, const rendered_line& text, const std::uint32_t index, const theme& look, const display_style& base) -> void {
	for (const rendered_run& r : text.runs) {
		const display_style shown = style_of(r.tone, family::proportional, look.sty, base.color);
		doc.spans.push_back({
			.line = index,
			.start_col = static_cast<std::uint32_t>(r.start),
			.end_col = static_cast<std::uint32_t>(r.end),
			.color = shown.color,
			.face = shown.face == gui::text_face::text && base.face != gui::text_face::inherit ? base.face : shown.face,
			.scale = base.scale,
		});
	}
}

auto gse::ide::markdown::render_document(const std::string_view source, const theme& look) -> rendered_document {
	const std::vector<std::string_view> lines = syntax::split_lines(source);
	const std::vector<line_info> classified = classify(lines);

	rendered_document doc;
	std::vector<run> scratch;
	rendered_line rendered;
	std::vector<rendered_line> cells;

	const auto body_face = look.fonts.text.resolve();
	const float space = body_face->width(" ", look.font_size);
	const float rule_width = body_face->width(rule_glyph, look.font_size);

	auto emit = [&doc](std::string text) -> std::uint32_t {
		const auto index = static_cast<std::uint32_t>(doc.buffer.lines.size());
		doc.buffer.lines.push_back(std::move(text));
		return index;
	};

	for (std::size_t i = 0; i < lines.size();) {
		const line_info& info = classified[i];

		if (info.shape == block::fence) {
			const std::size_t open = i;
			std::size_t last = i + 1;
			while (last < lines.size() && classified[last].shape == block::code) {
				++last;
			}
			const auto first_rendered = static_cast<std::uint32_t>(doc.buffer.lines.size());
			for (std::size_t k = open + 1; k < last; ++k) {
				const std::uint32_t index = emit(std::string(lines[k]));
				doc.spans.push_back({
					.line = index,
					.start_col = 0,
					.end_col = static_cast<std::uint32_t>(lines[k].size()),
					.color = look.sty.color_text,
					.face = gui::text_face::code,
				});
			}
			if (doc.buffer.lines.size() > first_rendered) {
				doc.blocks.push_back({
					.first_line = first_rendered,
					.last_line = static_cast<std::uint32_t>(doc.buffer.lines.size()) - 1,
					.fill = look.sty.color_panel_alt,
				});
			}
			i = last < lines.size() && classified[last].shape == block::fence ? last + 1 : last;
			continue;
		}

		if (info.shape == block::table_row || info.shape == block::table_delimiter) {
			std::size_t last = i;
			std::vector<std::vector<rendered_line>> rows;
			std::vector<float> widths;
			while (last < lines.size()
				&& (classified[last].shape == block::table_row || classified[last].shape == block::table_delimiter)) {
				if (classified[last].shape == block::table_delimiter) {
					++last;
					continue;
				}
				render_cells(lines[last], scratch, cells);
				widths.resize(std::max(widths.size(), cells.size()), 0.f);
				for (std::size_t c = 0; c < cells.size(); ++c) {
					widths[c] = std::max(widths[c], measure(cells[c], look, look.sty.color_text));
				}
				rows.push_back(std::move(cells));
				++last;
			}

			for (std::vector<rendered_line>& row : rows) {
				rendered.text.clear();
				rendered.runs.clear();
				std::vector<std::uint32_t> starts;
				for (const rendered_line& cell : row) {
					if (!rendered.text.empty()) {
						rendered.text += ' ';
					}
					const std::size_t offset = rendered.text.size();
					starts.push_back(static_cast<std::uint32_t>(offset));
					rendered.text += cell.text;
					for (const rendered_run& r : cell.runs) {
						rendered.runs.push_back({
							.start = offset + r.start,
							.end = offset + r.end,
							.tone = r.tone,
						});
					}
				}

				const std::uint32_t index = emit(rendered.text);
				push_spans(doc, rendered, index, look, {
					.color = look.sty.color_text,
				});

				float x = 0.f;
				for (std::size_t c = 0; c < starts.size(); ++c) {
					doc.stops.push_back({
						.line = index,
						.column = starts[c],
						.x = x,
					});
					x += widths[c] + space * static_cast<float>(table_cell_gap);
				}
			}

			i = last;
			continue;
		}

		if (info.shape == block::rule) {
			const auto glyphs = rule_width > 0.f
				? static_cast<std::size_t>(std::max(1.f, look.content_width / rule_width))
				: std::size_t{ 1 };
			std::string bar;
			bar.reserve(glyphs * rule_glyph.size());
			for (std::size_t k = 0; k < glyphs; ++k) {
				bar += rule_glyph;
			}
			const std::uint32_t index = emit(bar);
			doc.spans.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(doc.buffer.lines[index].size()),
				.color = look.sty.color_border,
				.face = gui::text_face::text,
			});
			++i;
			continue;
		}

		render_line(lines[i], info, scratch, rendered);
		const std::uint32_t index = emit(rendered.text);
		const display_style base = info.shape == block::heading
			? display_style{
				.color = look.sty.color_section_header,
				.face = gui::text_face::text_strong,
				.scale = heading_scale(info.heading_level),
			}
			: display_style{
				.color = info.quoted ? look.sty.color_text_secondary : look.sty.color_text,
				.face = gui::text_face::text,
			};
		push_spans(doc, rendered, index, look, base);
		++i;
	}

	if (doc.buffer.lines.empty()) {
		doc.buffer.lines.emplace_back();
	}

	return doc;
}
