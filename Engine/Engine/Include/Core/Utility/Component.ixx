export module gse.utility:component;

import std;

export namespace gse {
	struct component {
		explicit component(const std::uint32_t initial_unique_id) : parent_id(initial_unique_id) {}
		std::uint32_t parent_id = 0;
		std::string_view parent_name;
	};
}
