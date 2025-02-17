export module gse.core.component;

import std;

export namespace gse {
	struct component {
		component(const std::uint32_t initial_unique_id) : parent_id(initial_unique_id) {}
		virtual ~component() = default;

		std::uint32_t parent_id = 0;
		std::string_view parent_name;

		virtual auto required_components() -> std::vector<std::type_index> { return {}; }
	};
}
