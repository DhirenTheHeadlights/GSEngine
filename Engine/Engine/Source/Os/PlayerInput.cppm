export module gse.os:player_input;

import std;

import gse.core;

import :actions;

export namespace gse {
	struct player_input {
		actions::state state;
		std::uint32_t sequence = 0;
		std::uint32_t acked_sequence = 0;
		std::uint64_t acked_step = 0;
	};
}
