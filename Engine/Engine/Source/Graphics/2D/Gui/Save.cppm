export module gse.graphics:save;

import std;
import tomlplusplus;

import gse.assert;
import gse.utility;

import :types;
import :styles;

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
        const std::filesystem::path& file_path,
        id_mapped_collection<menu>& default_menus
    ) -> id_mapped_collection<menu>;
}

namespace gse::gui {
    auto dock_to_string(const dock::location location) -> std::string_view {
        switch (location) {
            case dock::location::none:   return "none";
            case dock::location::center: return "center";
            case dock::location::left:   return "left";
            case dock::location::right:  return "right";
            case dock::location::top:    return "top";
            case dock::location::bottom: return "bottom";
            default:                               return "none";
        }
    }

    auto location_from_string(const std::string_view str) -> dock::location {
        if (str == "left")   return dock::location::left;
        if (str == "right")  return dock::location::right;
        if (str == "top")    return dock::location::top;
        if (str == "bottom") return dock::location::bottom;
        if (str == "center") return dock::location::center;
        return dock::location::none;
    }
}

auto gse::gui::save(id_mapped_collection<menu>& menus, const std::filesystem::path& file_path) -> void {
    if (const auto parent_dir = file_path.parent_path(); !parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
        std::filesystem::create_directories(parent_dir);
    }

    toml::array menu_array;

    for (const auto& menu : menus.items()) {
        toml::table menu_table;

        menu_table.insert("tag", std::string(menu.id().tag()));
        menu_table.insert("owner", menu.owner_id().exists() ? std::string(menu.owner_id().tag()) : "");
        menu_table.insert("rect", toml::array{
            static_cast<double>(menu.rect.left()),
            static_cast<double>(menu.rect.top()),
            static_cast<double>(menu.rect.width()),
            static_cast<double>(menu.rect.height())
        });
        menu_table.insert("docked_to", dock_to_string(menu.docked_to));
        menu_table.insert("dock_split_ratio", static_cast<double>(menu.dock_split_ratio));
        menu_table.insert("active_tab", static_cast<std::int64_t>(menu.active_tab_index));

        toml::array tabs;
        for (const auto& tab : menu.tab_contents) {
            tabs.push_back(tab);
        }
        menu_table.insert("tabs", std::move(tabs));

        menu_array.push_back(std::move(menu_table));
    }

    toml::table root;
    root.insert("menu", std::move(menu_array));

    std::ofstream file(file_path);
    file << root;
}

auto gse::gui::load(const std::filesystem::path& file_path, id_mapped_collection<menu>& default_menus) -> id_mapped_collection<menu> {
    if (!std::filesystem::exists(file_path)) {
        id_mapped_collection<menu> menus_to_save = default_menus;
        save(menus_to_save, file_path);
        return default_menus;
    }

    toml::table root;
    try {
        root = toml::parse_file(file_path.string());
    } catch (const toml::parse_error&) {
        return default_menus;
    }

    const auto* menu_array = root["menu"].as_array();
    if (!menu_array) {
        return default_menus;
    }

    std::vector<loaded_menu_data> loaded_data_vec;

    for (const auto& item : *menu_array) {
        const auto* tbl = item.as_table();
        if (!tbl) continue;

        loaded_menu_data data;

        if (const auto* tag_node = tbl->get("tag")) {
            data.tag = tag_node->value_or<std::string>("");
        }

        if (const auto* owner_node = tbl->get("owner")) {
            data.owner_tag = owner_node->value_or<std::string>("");
        }

        if (const auto* docked_node = tbl->get("docked_to")) {
            data.docked_to = location_from_string(docked_node->value_or<std::string>("none"));
        }

        if (const auto* ratio_node = tbl->get("dock_split_ratio")) {
            data.dock_split_ratio = static_cast<float>(ratio_node->value_or(0.5));
        }

        if (const auto* tab_node = tbl->get("active_tab")) {
            data.active_tab_index = static_cast<std::uint32_t>(tab_node->value_or<std::int64_t>(0));
        }

        if (const auto* rect_arr = tbl->get("rect"); rect_arr && rect_arr->is_array()) {
            const auto& arr = *rect_arr->as_array();
            if (arr.size() == 4) {
                unitless::vec2 pos{
                    static_cast<float>(arr[0].value_or(0.0)),
                    static_cast<float>(arr[1].value_or(0.0))
                };
                unitless::vec2 size{
                    static_cast<float>(arr[2].value_or(0.0)),
                    static_cast<float>(arr[3].value_or(0.0))
                };
                data.rect = ui_rect::from_position_size(pos, size);
            }
        }

        if (const auto* tabs_node = tbl->get("tabs"); tabs_node && tabs_node->is_array()) {
            for (const auto& tab : *tabs_node->as_array()) {
                if (auto val = tab.value<std::string>()) {
                    data.tab_tags.push_back(*val);
                }
            }
        }

        loaded_data_vec.push_back(std::move(data));
    }

    if (loaded_data_vec.empty()) {
        return default_menus;
    }

    std::map<std::string, loaded_menu_data> loaded_map;
    for (const auto& data : loaded_data_vec) {
        loaded_map[data.tag] = data;
    }

    id_mapped_collection<menu> new_layout;
    std::unordered_map<std::string, id> id_by_tag;
    id_by_tag.reserve(loaded_map.size());

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
