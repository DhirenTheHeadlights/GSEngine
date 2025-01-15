export module gse.core.engine_component;

import std;

export namespace gse {
	struct component {
		component(std::uint32_t initial_unique_id);
		std::uint32_t parent_id = 0;
		std::string_view parent_name;
	};
}

gse::component::component(const std::uint32_t initial_unique_id)
	: parent_id(initial_unique_id) {
}