export module gse.graphics:save;

import std;

import gse.core;
import gse.fs;
import gse.math;
import gse.meta;

import :types;

export namespace gse::gui {
	struct loaded_menu_data {
		std::string tag;

		[[= field_key<"owner">{}]]
		std::string owner_tag;

		std::optional<vec2f> position_ratio;
		std::optional<vec2f> design_size;
		dock::location docked_to = dock::location::none;
		float dock_split_ratio = 0.5f;

		[[= field_key<"active_tab">{}]]
		std::uint32_t active_tab_index = 0;

		std::uint32_t tab_visible_rows = 1;

		[[= field_key<"tabs">{}]]
		std::vector<std::string> tab_tags;
	};

	auto save(
		id_mapped_collection<menu>& menus,
		const std::filesystem::path& file_path,
		vec2f viewport_size,
		float scale_factor
	) -> void;

	auto load(
		const std::filesystem::path& file_path,
		id_mapped_collection<menu>& default_menus,
		vec2f viewport_size,
		float scale_factor
	) -> id_mapped_collection<menu>;

	auto save_ui_scales(
		const std::unordered_map<std::string, float>& scales,
		const std::filesystem::path& file_path
	) -> void;

	auto load_ui_scales(
		const std::filesystem::path& file_path
	) -> std::unordered_map<std::string, float>;
}

namespace gse::gui {
	auto menu_to_data(
		const menu& item,
		vec2f viewport_size,
		float scale_factor
	) -> loaded_menu_data;

	auto menu_data_from_section(
		const layout_store::section& section
	) -> loaded_menu_data;

	auto parse_layout(
		std::string_view text
	) -> std::vector<loaded_menu_data>;

	auto resolve_rect(
		const loaded_menu_data& data,
		vec2f viewport_size,
		float scale_factor
	) -> rectf;
}