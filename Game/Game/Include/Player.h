#pragma once

#include <Engine.h>

namespace game {
	auto create_player(std::uint32_t object_uuid) -> void;
	auto create_player() -> std::uint32_t;
}