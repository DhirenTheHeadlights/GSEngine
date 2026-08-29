export module gse.runtime:state_dump;

import std;

import gse.core;
import gse.math;
import gse.containers;

export namespace gse {
	constexpr std::uint32_t state_dump_magic = 0x44534753;
	constexpr std::uint32_t state_dump_version = 2;

	struct [[= archive_raw{}]] state_dump_record {
		std::uint64_t owner = 0;
		std::uint32_t frame = 0;
		vec3<position> position;
		vec3<velocity> velocity;
		quat orientation;
		vec3<angular_velocity> angular_velocity;
	};

	auto read_state_dump(
		const std::filesystem::path& path
	) -> std::expected<std::vector<state_dump_record>, archive_mismatch>;
}
