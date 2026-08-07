module gse.graphics:save_impl;

import std;

import gse.core;
import gse.fs;
import gse.math;
import gse.meta;

import :save;
import :types;

auto gse::gui::menu_to_data(const menu& item, const vec2f viewport_size, const float scale_factor) -> loaded_menu_data {
	return {
		.tag = std::string(item.id().tag()),
		.owner_tag = item.owner_id().exists() ? std::string(item.owner_id().tag()) : std::string{},
		.position_ratio = vec2f{
			item.rect.left() / viewport_size.x(),
			(viewport_size.y() - item.rect.top()) / viewport_size.y()
		},
		.design_size = vec2f{
			item.rect.width() / scale_factor,
			item.rect.height() / scale_factor
		},
		.docked_to = item.docked_to,
		.dock_split_ratio = item.dock_split_ratio,
		.active_tab_index = item.active_tab_index,
		.tab_visible_rows = std::max(1u, item.tab_bar.visible_rows),
		.tab_tags = item.tab_contents,
	};
}

auto gse::gui::menu_data_from_section(const layout_store::section& section) -> loaded_menu_data {
	loaded_menu_data data;
	meta::read_fields(section.values, data);
	return data;
}

auto gse::gui::parse_layout(const std::string_view text) -> std::vector<loaded_menu_data> {
	return layout_store::parse_sections(text)
		| std::views::filter([](const layout_store::section& section) { return section.name.starts_with("menu "); })
		| std::views::transform(menu_data_from_section)
		| std::ranges::to<std::vector>();
}

auto gse::gui::resolve_rect(const loaded_menu_data& data, const vec2f viewport_size, const float scale_factor) -> rectf {
	if (!data.position_ratio || !data.design_size) {
		return {};
	}

	const vec2f size{ data.design_size->x() * scale_factor, data.design_size->y() * scale_factor };
	const vec2f position{
		data.position_ratio->x() * viewport_size.x(),
		viewport_size.y() - data.position_ratio->y() * viewport_size.y()
	};

	return rectf::from_position_size(position, size);
}

auto gse::gui::save(id_mapped_collection<menu>& menus, const std::filesystem::path& file_path, const vec2f viewport_size, const float scale_factor) -> void {
	if (viewport_size.x() <= 0.f || viewport_size.y() <= 0.f || scale_factor <= 0.f) {
		return;
	}

	std::string out;

	for (const auto& [index, item] : std::views::enumerate(menus.items())) {
		if (index > 0) {
			out.push_back('\n');
		}
		out.append(std::format("[menu {}]\n", index));

		std::map<std::string, std::string> values;
		meta::write_fields(menu_to_data(item, viewport_size, scale_factor), values);

		for (const auto& [key, value] : values) {
			out.append(std::format("{} = {}\n", key, value));
		}
	}

	layout_store::submit(
		file_path,
		{
			.prefixes = { "menu " },
		},
		std::move(out)
	);
}

auto gse::gui::load(const std::filesystem::path& file_path, id_mapped_collection<menu>& default_menus, const vec2f viewport_size, const float scale_factor) -> id_mapped_collection<menu> {
	const std::string content = layout_store::read(file_path);
	if (content.empty()) {
		id_mapped_collection<menu> menus_to_save = default_menus;
		save(menus_to_save, file_path, viewport_size, scale_factor);
		return default_menus;
	}

	std::map<std::string, loaded_menu_data> loaded_map;

	for (const auto& data : parse_layout(content) | std::views::filter([](const loaded_menu_data& d) { return !d.tag.empty(); })) {
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
			.rect = resolve_rect(data, viewport_size, scale_factor),
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

auto gse::gui::save_ui_scales(const std::unordered_map<std::string, float>& scales, const std::filesystem::path& file_path) -> void {
	std::string out;

	if (!scales.empty()) {
		out.append("[ui_scale]\n");
		for (const auto& [key, scale] : std::map<std::string, float>(scales.begin(), scales.end())) {
			out.append(std::format("{} = {}\n", key, scale));
		}
	}

	layout_store::submit(
		file_path,
		{
			.names = { "ui_scale" },
		},
		std::move(out)
	);
}

auto gse::gui::load_ui_scales(const std::filesystem::path& file_path) -> std::unordered_map<std::string, float> {
	std::unordered_map<std::string, float> scales;

	const auto sections = layout_store::parse_sections(layout_store::read(file_path));
	const auto it = std::ranges::find(sections, std::string_view("ui_scale"), &layout_store::section::name);
	if (it == sections.end()) {
		return scales;
	}

	for (const auto& [key, value] : it->values) {
		if (float scale = 0.f; parse(value, scale)) {
			scales[key] = scale;
		}
	}

	return scales;
}
