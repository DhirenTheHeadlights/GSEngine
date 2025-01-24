export module game.player;

import std;

export namespace game {
	auto create_player(std::uint32_t object_uuid) -> void;
	auto create_player() -> std::uint32_t;
}