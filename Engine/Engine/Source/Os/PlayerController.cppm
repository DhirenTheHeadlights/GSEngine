export module gse.os:player_controller;

import std;

import gse.core;
import gse.ecs;
import gse.meta;

export namespace gse {
	struct player_controller {
		[[= networked]] id controlled_entity_id;

		bool local_only_flag = false;
	};
}
