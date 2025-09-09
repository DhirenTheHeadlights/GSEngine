export module gse.graphics:save;

import std;

import gse.assert;
import gse.utility;

import :types;

export namespace gse::gui {
	struct loaded_menu_data {
		std::string tag;
		std::string owner_tag;
		ui_rect rect;
		dock::location docked_to = dock::location::none;
		float dock_split_ratio = 0.5f;
		std::uint32_t active_tab_index = 0;
		std::vector<std::string> tab_tags;
	};

	auto save(
		id_mapped_collection<menu>& menus,
		const std::filesystem::path& file_path
	) -> void;

	auto load(
		const std::filesystem::path& file_path, id_mapped_collection<menu>& default_menus
	) -> id_mapped_collection<menu>;
}

auto gse::gui::save(id_mapped_collection<menu>& menus, const std::filesystem::path& file_path) -> void {
	if (const auto parent_dir = file_path.parent_path(); !parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
		create_directories(parent_dir);
	}

	std::ofstream file(file_path);
	assert(
		file.is_open(),
		std::format("Failed to open GUI layout file for writing: {}", file_path.string())
	);

	auto to_string = [](const dock::location location) -> std::string {
		switch (location) {
			case dock::location::none:   return "none";
			case dock::location::center: return "center";
			case dock::location::left:   return "left";
			case dock::location::right:  return "right";
			case dock::location::top:    return "top";
			case dock::location::bottom: return "bottom";
			default:                     return "unknown";
		}
	};

	for (const auto& menu : menus.items()) {
		file << "[Menu]\n";
		file << "Tag: " << menu.id().tag() << "\n";
		file << "Owner: " << menu.owner_id().tag() << "\n";
		file << "Rect: " << menu.rect.left() << " " << menu.rect.top() << " " << menu.rect.width() << " " << menu.rect.height() << "\n";
		file << "DockedTo: " << to_string(menu.docked_to) << "\n";
		file << "DockSplitRatio: " << menu.dock_split_ratio << "\n";
		file << "ActiveTab: " << menu.active_tab_index << "\n";
		
		file << "Tabs:";
		for (const auto& tag : menu.tab_contents) {
			file << " " << tag;
		}
		file << "\n";
		
		file << "[EndMenu]\n\n";
	}
}

auto gse::gui::load(const std::filesystem::path& file_path, id_mapped_collection<menu>& default_menus) -> id_mapped_collection<menu> {
	if (!std::filesystem::exists(file_path)) {
		id_mapped_collection<menu> menus_to_save = default_menus;
		std::filesystem::path path_to_save = file_path;
		save(menus_to_save, path_to_save);

		return default_menus;
	}
	
	std::ifstream file(file_path);
	if (!file.is_open()) {
		return default_menus;
	}

	std::vector<loaded_menu_data> loaded_data_vec;
	std::string line;
	std::optional<loaded_menu_data> current_data;

	auto location_from_string = [](const std::string& str) -> dock::location {
		if (str == "left")   return dock::location::left;
		if (str == "right")  return dock::location::right;
		if (str == "top")    return dock::location::top;
		if (str == "bottom") return dock::location::bottom;
		if (str == "center") return dock::location::center;
		return dock::location::none;
	};

	while (std::getline(file, line)) {
		if (line == "[Menu]") {
			current_data.emplace();
			continue;
		}
		if (line == "[EndMenu]") {
			if (current_data) {
				loaded_data_vec.push_back(*current_data);
				current_data.reset();
			}
			continue;
		}
		if (!current_data) continue;

		std::stringstream ss(line);
		std::string key;
		ss >> key;

		if (key == "Tag:") ss >> current_data->tag;
		else if (key == "Owner:") ss >> current_data->owner_tag;
		else if (key == "DockedTo:") {
			std::string val; ss >> val;
			current_data->docked_to = location_from_string(val);
		}
		else if (key == "DockSplitRatio:") ss >> current_data->dock_split_ratio;
		else if (key == "ActiveTab:") ss >> current_data->active_tab_index;
		else if (key == "Rect:") {
			unitless::vec2 pos, size;
			ss >> pos.x() >> pos.y() >> size.x() >> size.y();
			current_data->rect = ui_rect::from_position_size(pos, size);
		}
		else if (key == "Tabs:") {
			std::string tab_tag;
			while (ss >> tab_tag) {
				current_data->tab_tags.push_back(tab_tag);
			}
		}
	}

	if (loaded_data_vec.empty()) {
		return default_menus;
	}

	id_mapped_collection<menu> new_layout;
	std::map<std::string, loaded_menu_data> loaded_map;
	for (const auto& data : loaded_data_vec) {
		loaded_map[data.tag] = data;
	}

	for (const auto& [tag, data] : loaded_map) {
		menu_data md = { .rect = data.rect, .parent_id = id() };
		menu new_menu(tag, md);

		new_menu.docked_to = data.docked_to;
		new_menu.dock_split_ratio = data.dock_split_ratio;
		new_menu.tab_contents = data.tab_tags;
		
		if (!new_menu.tab_contents.empty()) {
			new_menu.active_tab_index = std::min(
				static_cast<std::uint32_t>(new_menu.tab_contents.size() - 1),
				data.active_tab_index
			);
		} else {
			new_menu.active_tab_index = 0;
		}

		new_layout.add(new_menu.id(), std::move(new_menu));
	}

	for (auto& menu_item : new_layout.items()) {
		const auto& data = loaded_map.at(menu_item.id().tag());
		if (!data.owner_tag.empty()) {
			menu_item.swap(find(data.owner_tag));
		}
	}

	return new_layout;
}
