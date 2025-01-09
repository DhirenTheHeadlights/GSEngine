#pragma once

#include <Engine.h>

namespace game {
	auto create_player(const gse::object* object) -> void;
	auto create_player() -> gse::object*;
}