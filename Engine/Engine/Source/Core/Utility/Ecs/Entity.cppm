export module gse.utility:entity;

import std;

import :misc;

import gse.math;

export namespace gse {
	struct entity {
		std::uint32_t index = random_value(std::numeric_limits<std::uint32_t>::max());
		std::uint32_t generation = 0;

		auto operator==(const entity& other) const -> bool = default;
	};
}
