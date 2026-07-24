export module gse.ide.app:layout;

import std;
import gse;

import gse.ide.workspace;
import gse.ide.config;

namespace gse::ide {
	struct layout_section {
		std::string name;
		std::map<std::string, std::string> values;
	};

	auto editor_layout_path() -> std::filesystem::path;

	auto parse_layout_sections(
		std::string_view text
	) -> std::vector<layout_section>;

	auto replace_layout_sections(
		layout_store::owner sections,
		std::string_view block
	) -> void;

	auto parse_layout_float(
		const std::string& value,
		float fallback
	) -> float;

	auto parse_layout_uint(
		const std::string& value,
		std::uint32_t fallback
	) -> std::uint32_t;

	auto editor_layout_owner() -> layout_store::owner;

	auto workspace_layout_owner() -> layout_store::owner;

	auto load_workspace_layout(
		workspace::data& ws
	) -> void;

	auto save_workspace_layout(
		const workspace::data& ws
	) -> void;
}

auto gse::ide::editor_layout_path() -> std::filesystem::path {
	return config::resource_path / "editor_layout.ini";
}

auto gse::ide::parse_layout_sections(const std::string_view text) -> std::vector<layout_section> {
	std::vector<layout_section> sections;
	layout_section* current = nullptr;

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t line_end = text.find('\n', pos);
		const std::string_view line = text.substr(
			pos,
			line_end == std::string_view::npos ? text.size() - pos : line_end - pos
		);
		pos = line_end == std::string_view::npos ? text.size() : line_end + 1;

		const std::string_view trimmed = layout_store::trimmed(line);
		if (trimmed.empty() || trimmed.front() == '#') {
			continue;
		}

		if (const std::string_view name = layout_store::section_name(trimmed); !name.empty()) {
			sections.push_back({ .name = std::string(name) });
			current = &sections.back();
			continue;
		}

		if (!current) {
			continue;
		}

		const std::size_t eq = trimmed.find('=');
		if (eq == std::string_view::npos) {
			continue;
		}

		const std::string key(layout_store::trimmed(trimmed.substr(0, eq)));
		const std::string value(layout_store::trimmed(trimmed.substr(eq + 1)));
		current->values[key] = value;
	}

	return sections;
}

auto gse::ide::replace_layout_sections(layout_store::owner sections, const std::string_view block) -> void {
	layout_store::submit(editor_layout_path(), std::move(sections), std::string(block));
}

auto gse::ide::parse_layout_float(const std::string& value, const float fallback) -> float {
	float parsed = 0.f;
	if (std::from_chars(value.data(), value.data() + value.size(), parsed).ec == std::errc{}) {
		return parsed;
	}
	return fallback;
}

auto gse::ide::parse_layout_uint(const std::string& value, const std::uint32_t fallback) -> std::uint32_t {
	std::uint32_t parsed = 0;
	if (std::from_chars(value.data(), value.data() + value.size(), parsed).ec == std::errc{}) {
		return parsed;
	}
	return fallback;
}

auto gse::ide::editor_layout_owner() -> layout_store::owner {
	return {
		.names = { "editor" },
	};
}

auto gse::ide::workspace_layout_owner() -> layout_store::owner {
	return {
		.names = { "workspace" },
		.prefixes = { "document " },
	};
}

auto gse::ide::load_workspace_layout(workspace::data& ws) -> void {
	const std::vector<layout_section> sections = parse_layout_sections(layout_store::read(editor_layout_path()));
	std::string active;
	std::filesystem::path active_path;
	std::unordered_map<std::string, std::uint32_t> id_by_path;

	for (const layout_section& section : sections) {
		if (section.name != "workspace") {
			continue;
		}
		if (const auto it = section.values.find("active"); it != section.values.end()) {
			active = it->second;
		}
		if (const auto it = section.values.find("active_path"); it != section.values.end()) {
			active_path = it->second;
		}
		break;
	}

	for (const layout_section& section : sections) {
		if (!section.name.starts_with("document ")) {
			continue;
		}

		const auto path_it = section.values.find("path");
		if (path_it == section.values.end() || path_it->second.empty()) {
			continue;
		}

		const std::filesystem::path path = path_it->second;
		std::error_code exists_ec;
		if (!std::filesystem::exists(path, exists_ec)) {
			continue;
		}

		const std::uint32_t id = workspace::open_file(ws, path);
		document& doc = ws.documents.at(id);
		const std::uint32_t line = section.values.contains("line") ? parse_layout_uint(section.values.at("line"), 0) : 0;
		const std::uint32_t column = section.values.contains("column") ? parse_layout_uint(section.values.at("column"), 0) : 0;
		doc.view.caret = doc.buffer.clamp({ .line = line, .column = column });
		doc.view.anchor = doc.view.caret;

		if (const auto it = section.values.find("scroll_x"); it != section.values.end()) {
			doc.view.scroll.x.offset = std::max(0.f, parse_layout_float(it->second, 0.f));
			doc.view.scroll.x.target = doc.view.scroll.x.offset;
		}
		if (const auto it = section.values.find("scroll_y"); it != section.values.end()) {
			doc.view.scroll.y.offset = std::max(0.f, parse_layout_float(it->second, 0.f));
			doc.view.scroll.y.target = doc.view.scroll.y.offset;
		}

		id_by_path.emplace(doc.path.generic_native_encoded_string(), id);
	}

	if (active == "game") {
		ws.active_document_id = viewport_tab_id;
	}
	else if (!active_path.empty()) {
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(active_path, ec);
		const std::string key = (ec ? active_path : canonical).generic_native_encoded_string();
		if (const auto it = id_by_path.find(key); it != id_by_path.end()) {
			ws.active_document_id = it->second;
		}
	}
	else if (!ws.tab_order.empty()) {
		ws.active_document_id = ws.tab_order.front();
	}

	ws.tab_strip.dragging = 0;
}

auto gse::ide::save_workspace_layout(const workspace::data& ws) -> void {
	std::string out;
	out.append("[workspace]\n");
	if (ws.active_document_id == viewport_tab_id) {
		out.append("active = game\n");
		out.append("active_path = \n");
	}
	else if (const auto it = ws.documents.find(ws.active_document_id); it != ws.documents.end() && !it->second.path.empty()) {
		out.append("active = document\n");
		out.append(std::format("active_path = {}\n", it->second.path.generic_native_encoded_string()));
	}
	else {
		out.append("active = none\n");
		out.append("active_path = \n");
	}

	std::size_t index = 0;
	for (const std::uint32_t id : ws.tab_order) {
		const document& doc = ws.documents.at(id);
		if (doc.path.empty()) {
			continue;
		}
		const gse::gui::buffer_position caret = doc.buffer.clamp(doc.view.caret);
		out.push_back('\n');
		out.append(std::format("[document {}]\n", index));
		out.append(std::format("path = {}\n", doc.path.generic_native_encoded_string()));
		out.append(std::format("line = {}\n", caret.line));
		out.append(std::format("column = {}\n", caret.column));
		out.append(std::format("scroll_x = {}\n", std::max(0.f, doc.view.scroll.x.offset)));
		out.append(std::format("scroll_y = {}\n", std::max(0.f, doc.view.scroll.y.offset)));
		++index;
	}

	replace_layout_sections(workspace_layout_owner(), out);
}
