export module gse.graphics:save;

import std;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.fs;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

import gse.math;
import :types;
import :styles;

export namespace gse::gui {
	struct loaded_menu_data {
		std::string tag;
		std::string owner_tag;
		rectf rect;
		dock::location docked_to = dock::location::none;
		float dock_split_ratio = 0.5f;
		std::uint32_t active_tab_index = 0;
		std::uint32_t tab_visible_rows = 1;
		std::vector<std::string> tab_tags;
	};

	auto save(
		id_mapped_collection<menu>& menus,
		const std::filesystem::path& file_path
	) -> void;

	auto load(
		const std::filesystem::path& file_path,
		id_mapped_collection<menu>& default_menus
	) -> id_mapped_collection<menu>;
}

namespace gse::gui {
	auto dock_to_string(
		dock::location location
	) -> std::string_view;

	auto location_from_string(
		std::string_view str
	) -> dock::location;

	auto join(
		std::span<const std::string> parts,
		char sep
	) -> std::string;

	auto split(
		std::string_view text,
		char sep
	) -> std::vector<std::string>;

	auto trim(
		std::string_view s
	) -> std::string_view;

	auto is_menu_section(
		std::string_view line
	) -> bool;

	auto parse_layout(
		std::string_view text
	) -> std::vector<loaded_menu_data>;
}

auto gse::gui::dock_to_string(const dock::location location) -> std::string_view {
	switch (location) {
		case dock::location::none:
			return "none";
		case dock::location::center:
			return "center";
		case dock::location::left:
			return "left";
		case dock::location::right:
			return "right";
		case dock::location::top:
			return "top";
		case dock::location::bottom:
			return "bottom";
		default:
			return "none";
	}
}

auto gse::gui::location_from_string(const std::string_view str) -> dock::location {
	if (str == "left") {
		return dock::location::left;
	}
	if (str == "right") {
		return dock::location::right;
	}
	if (str == "top") {
		return dock::location::top;
	}
	if (str == "bottom") {
		return dock::location::bottom;
	}
	if (str == "center") {
		return dock::location::center;
	}
	return dock::location::none;
}

auto gse::gui::join(const std::span<const std::string> parts, const char sep) -> std::string {
	std::string out;
	for (std::size_t i = 0; i < parts.size(); ++i) {
		if (i > 0) {
			out.push_back(sep);
		}
		out.append(parts[i]);
	}
	return out;
}

auto gse::gui::split(const std::string_view text, const char sep) -> std::vector<std::string> {
	std::vector<std::string> out;
	std::size_t start = 0;
	while (start <= text.size()) {
		const std::size_t end = text.find(sep, start);
		const auto piece = end == std::string_view::npos ? text.substr(start) : text.substr(start, end - start);
		if (!piece.empty()) {
			out.emplace_back(piece);
		}
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}
	return out;
}

auto gse::gui::trim(const std::string_view s) -> std::string_view {
	std::size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
		++start;
	}
	std::size_t end = s.size();
	while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
		--end;
	}
	return s.substr(start, end - start);
}

auto gse::gui::is_menu_section(const std::string_view line) -> bool {
	const std::string_view trimmed = trim(line);
	return trimmed.starts_with("[menu ") && trimmed.ends_with("]");
}

auto gse::gui::parse_layout(const std::string_view text) -> std::vector<loaded_menu_data> {
	std::vector<loaded_menu_data> result;
	std::optional<loaded_menu_data> current;

	auto flush = [&] {
		if (current) {
			result.push_back(std::move(*current));
			current.reset();
		}
	};

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t line_end = text.find('\n', pos);
		const std::string_view line_raw =
			text.substr(
				pos,
				line_end == std::string_view::npos ? text.size() - pos : line_end - pos
			);
		pos = line_end == std::string_view::npos ? text.size() : line_end + 1;

		const auto line = trim(line_raw);
		if (line.empty() || line.front() == '#') {
			continue;
		}

		if (line.front() == '[' && line.back() == ']') {
			flush();
			if (is_menu_section(line)) {
				current.emplace();
			}
			else {
				current.reset();
			}
			continue;
		}

		if (!current) {
			continue;
		}

		const std::size_t eq = line.find('=');
		if (eq == std::string_view::npos) {
			continue;
		}

		const auto key = trim(line.substr(0, eq));
		const auto value = trim(line.substr(eq + 1));

		if (key == "tag") {
			current->tag = std::string(value);
		}
		else if (key == "owner") {
			current->owner_tag = std::string(value);
		}
		else if (key == "docked_to") {
			current->docked_to = location_from_string(value);
		}
		else if (key == "dock_split_ratio") {
			current->dock_split_ratio = std::stof(std::string(value));
		}
		else if (key == "active_tab") {
			current->active_tab_index = static_cast<std::uint32_t>(std::stoul(std::string(value)));
		}
		else if (key == "tab_visible_rows") {
			current->tab_visible_rows = static_cast<std::uint32_t>(std::stoul(std::string(value)));
		}
		else if (key == "rect") {
			const auto parts = split(value, ',');
			if (parts.size() == 4) {
				const vec2f p{ std::stof(parts[0]), std::stof(parts[1]) };
				const vec2f sz{ std::stof(parts[2]), std::stof(parts[3]) };
				current->rect = rectf::from_position_size(p, sz);
			}
		}
		else if (key == "tabs") {
			current->tab_tags = split(value, ',');
		}
	}

	flush();
	return result;
}

auto gse::gui::save(id_mapped_collection<menu>& menus, const std::filesystem::path& file_path) -> void {
	std::string out;
	std::size_t i = 0;
	for (const auto& menu : menus.items()) {
		if (i > 0) {
			out.push_back('\n');
		}
		out.append(std::format("[menu {}]\n", i));
		out.append(std::format("tag = {}\n", std::string(menu.id().tag())));
		out.append(
			std::format(
				"owner = {}\n",
				menu.owner_id().exists() ? std::string(menu.owner_id().tag()) : std::string{}
			)
		);
		out.append(
			std::format(
				"rect = {},{},{},{}\n",
				menu.rect.left(),
				menu.rect.top(),
				menu.rect.width(),
				menu.rect.height()
			)
		);
		out.append(std::format("docked_to = {}\n", dock_to_string(menu.docked_to)));
		out.append(std::format("dock_split_ratio = {}\n", menu.dock_split_ratio));
		out.append(std::format("active_tab = {}\n", menu.active_tab_index));
		out.append(std::format("tab_visible_rows = {}\n", std::max(1u, menu.tab_bar.visible_rows)));
		out.append(std::format("tabs = {}\n", join(menu.tab_contents, ',')));
		++i;
	}

	layout_store::submit(
		file_path,
		{
			.prefixes = { "menu " },
		},
		std::move(out)
	);
}

auto gse::gui::load(const std::filesystem::path& file_path, id_mapped_collection<menu>& default_menus) -> id_mapped_collection<menu> {
	const std::string content = layout_store::read(file_path);
	if (content.empty()) {
		id_mapped_collection<menu> menus_to_save = default_menus;
		save(menus_to_save, file_path);
		return default_menus;
	}

	auto loaded_data_vec = parse_layout(content);

	if (loaded_data_vec.empty()) {
		return default_menus;
	}

	std::map<std::string, loaded_menu_data> loaded_map;
	for (const auto& data : loaded_data_vec) {
		if (data.tag.empty()) {
			continue;
		}
		loaded_map[data.tag] = data;
	}

	if (loaded_map.empty()) {
		return default_menus;
	}

	id_mapped_collection<menu> new_layout;
	std::unordered_map<std::string, id> id_by_tag;
	id_by_tag.reserve(loaded_map.size());

	for (const auto& [tag, data] : loaded_map) {
		menu_data md = {
			.rect = data.rect,
			.parent_id = id()
		};
		menu new_menu(tag, md);

		new_menu.docked_to = data.docked_to;
		new_menu.dock_split_ratio = data.dock_split_ratio;
		new_menu.tab_contents = data.tab_tags;
		new_menu.tab_bar.visible_rows = std::max(1u, data.tab_visible_rows);

		if (!new_menu.tab_contents.empty()) {
			new_menu.active_tab_index =
				std::min(
					static_cast<std::uint32_t>(new_menu.tab_contents.size() - 1),
					data.active_tab_index
				);
		}
		else {
			new_menu.active_tab_index = 0;
		}

		const id created_id = new_menu.id();
		id_by_tag.emplace(tag, created_id);
		new_layout.add(created_id, std::move(new_menu));
	}

	for (auto& menu_item : new_layout.items()) {
		const auto tag = std::string(menu_item.id().tag());

		if (const auto& data = loaded_map.at(tag); !data.owner_tag.empty()) {
			if (const auto it = id_by_tag.find(data.owner_tag); it != id_by_tag.end()) {
				menu_item.swap_parent(it->second);
			}
		}
	}

	return new_layout;
}
