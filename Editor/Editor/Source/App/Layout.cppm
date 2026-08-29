export module gse.ide.app:layout;

import std;
import gse;

import gse.ide.workspace;
import gse.ide.config;
import gse.ide.profile;

namespace gse::ide {
	auto editor_layout_path() -> std::filesystem::path;

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
	return config::editor_layout();
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
		.names = { "editor", "dock" },
		.prefixes = { "dock node ", "dock window " },
	};
}

auto gse::ide::workspace_layout_owner() -> layout_store::owner {
	return {
		.names = { "workspace" },
		.prefixes = { "document " },
	};
}

auto gse::ide::load_workspace_layout(workspace::data& ws) -> void {
	const std::vector<layout_store::section> sections = layout_store::parse_sections(layout_store::read(editor_layout_path()));
	std::string active;
	std::filesystem::path active_path;
	std::unordered_map<std::string, id> id_by_path;

	for (const layout_store::section& section : sections) {
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

	for (const layout_store::section& section : sections) {
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

		const id id = workspace::open_file(ws, path);
		if (!id.exists()) {
			continue;
		}
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

	for (const layout_store::section& section : sections) {
		if (section.name != "workspace") {
			continue;
		}
		if (const auto it = section.values.find("profile_enabled"); it != section.values.end()) {
			ws.profile.enabled = it->second == "1";
		}
		if (const auto it = section.values.find("profile_source"); it != section.values.end()) {
			enum_from_string(it->second, ws.profile.source);
		}
		break;
	}

	if (!active_path.empty()) {
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(active_path, ec);
		const std::string key = (ec ? active_path : canonical).generic_native_encoded_string();
		if (const auto it = id_by_path.find(key); it != id_by_path.end()) {
			workspace::activate_document(ws, it->second);
		}
	}
	else if (!ws.documents.order().empty()) {
		workspace::activate_document(ws, ws.documents.order().front());
	}

	ws.tab_strip.dragging.reset();
}

auto gse::ide::save_workspace_layout(const workspace::data& ws) -> void {
	std::string out;
	out.append("[workspace]\n");
	out.append(std::format("profile_enabled = {}\n", ws.profile.enabled ? 1 : 0));
	out.append(std::format("profile_source = {}\n", enum_to_string(ws.profile.source)));
	if (const std::optional<id> active_document_id = workspace::active_document_id(ws); active_document_id) {
		const auto it = ws.documents.find(*active_document_id);
		if (it == ws.documents.end() || it->second.path.empty()) {
			out.append("active = none\n");
			out.append("active_path = \n");
		}
		else {
			out.append("active = document\n");
			out.append(std::format("active_path = {}\n", it->second.path.generic_native_encoded_string()));
		}
	}
	else {
		out.append("active = none\n");
		out.append("active_path = \n");
	}

	std::size_t index = 0;
	for (const id id : ws.documents.order()) {
		const document& doc = ws.documents.at(id);
		if (doc.path.empty()) {
			continue;
		}
		const gui::buffer_position caret = doc.buffer.clamp(doc.view.caret);
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