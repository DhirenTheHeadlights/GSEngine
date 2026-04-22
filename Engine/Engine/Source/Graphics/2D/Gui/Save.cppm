export module gse.graphics:save;

import std;

import gse.assert;
import gse.log;
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

        menu_table.emplace("tag", toml::value{ .data = std::string(menu.id().tag()) });
        menu_table.emplace("owner", toml::value{ .data = menu.owner_id().exists() ? std::string(menu.owner_id().tag()) : std::string{} });
        menu_table.emplace("rect", toml::value{ .data = toml::array{
            toml::value{ .data = static_cast<double>(menu.rect.left()) },
            toml::value{ .data = static_cast<double>(menu.rect.top()) },
            toml::value{ .data = static_cast<double>(menu.rect.width()) },
            toml::value{ .data = static_cast<double>(menu.rect.height()) },
        }});
        menu_table.emplace("docked_to", toml::value{ .data = std::string(dock_to_string(menu.docked_to)) });
        menu_table.emplace("dock_split_ratio", toml::value{ .data = static_cast<double>(menu.dock_split_ratio) });
        menu_table.emplace("active_tab", toml::value{ .data = static_cast<std::int64_t>(menu.active_tab_index) });

        toml::array tabs;
        tabs.reserve(menu.tab_contents.size());
        for (const auto& tab : menu.tab_contents) {
            tabs.push_back(toml::value{ .data = tab });
        }
        menu_table.emplace("tabs", toml::value{ .data = std::move(tabs) });

        menu_array.push_back(toml::value{ .data = std::move(menu_table) });
    }

    toml::table root;
    root.emplace("menu", toml::value{ .data = std::move(menu_array) });

    const std::string content = toml::emit(toml::value{ .data = std::move(root) });

    std::ofstream file(file_path);
    file << content;
}

auto gse::gui::load(const std::filesystem::path& file_path, id_mapped_collection<menu>& default_menus) -> id_mapped_collection<menu> {
    if (!std::filesystem::exists(file_path)) {
        id_mapped_collection<menu> menus_to_save = default_menus;
        save(menus_to_save, file_path);
        return default_menus;
    }

    std::ifstream file(file_path);
    if (!file) {
        return default_menus;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    const std::string content = oss.str();

    auto parsed = toml::parse(content, file_path.string());
    if (!parsed) {
        log::println(log::level::warning, log::category::save_system, "TOML parse error in {} at {}:{}: {}", file_path.string(), parsed.error().line, parsed.error().column, parsed.error().message);
        return default_menus;
    }
    if (!parsed->is_table()) {
        return default_menus;
    }

    const auto& root = parsed->as_table();
    const auto menu_it = root.find("menu");
    if (menu_it == root.end() || !menu_it->second.is_array()) {
        return default_menus;
    }

    std::vector<loaded_menu_data> loaded_data_vec;

    for (const auto& item : menu_it->second.as_array()) {
        if (!item.is_table()) {
            continue;
        }
        const auto& tbl = item.as_table();

        loaded_menu_data data;

        if (const auto it = tbl.find("tag"); it != tbl.end()) {
            data.tag = it->second.value_or<std::string>("");
        }

        if (const auto it = tbl.find("owner"); it != tbl.end()) {
            data.owner_tag = it->second.value_or<std::string>("");
        }

        if (const auto it = tbl.find("docked_to"); it != tbl.end()) {
            data.docked_to = location_from_string(it->second.value_or<std::string>("none"));
        }

        if (const auto it = tbl.find("dock_split_ratio"); it != tbl.end()) {
            data.dock_split_ratio = it->second.value_or<float>(0.5f);
        }

        if (const auto it = tbl.find("active_tab"); it != tbl.end()) {
            data.active_tab_index = it->second.value_or<std::uint32_t>(0);
        }

        if (const auto it = tbl.find("rect"); it != tbl.end() && it->second.is_array()) {
            const auto& arr = it->second.as_array();
            if (arr.size() == 4) {
                vec2f pos{
                    arr[0].value_or<float>(0.f),
                    arr[1].value_or<float>(0.f),
                };
                vec2f size{
                    arr[2].value_or<float>(0.f),
                    arr[3].value_or<float>(0.f),
                };
                data.rect = ui_rect::from_position_size(pos, size);
            }
        }

        if (const auto it = tbl.find("tabs"); it != tbl.end() && it->second.is_array()) {
            for (const auto& tab : it->second.as_array()) {
                if (tab.is_string()) {
                    data.tab_tags.push_back(tab.as_string());
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
