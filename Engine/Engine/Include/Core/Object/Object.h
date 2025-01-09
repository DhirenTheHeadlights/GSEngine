#pragma once

#include "Core/ID.h"

namespace gse {
	struct object {
		std::uint32_t index = generate_random_entity_placeholder_id();
		std::uint32_t generation = 0;
	};
}
