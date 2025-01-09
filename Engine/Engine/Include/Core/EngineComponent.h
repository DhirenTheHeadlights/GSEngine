#pragma once

#include <string_view>

namespace gse {
	struct component {
		component(std::uint32_t initial_unique_id);
		std::uint32_t parent_id = 0;
		std::string_view parent_name;
	};
}
