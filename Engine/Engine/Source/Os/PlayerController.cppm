export module gse.os:player_controller;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;

export namespace gse {
	struct player_controller_data {
		[[= networked]] id controlled_entity_id;
		bool local_only_flag = false;
	};

	using player_controller_net = project_by_annotation<player_controller_data, networked_tag>;

	struct player_controller : component<player_controller_data, player_controller_net> {
		player_controller() = default;

		explicit player_controller(
			const id owner_id,
			const player_controller_data& p = {}
		) : component(owner_id, p) {}

		explicit player_controller(
			const id owner_id,
			const player_controller_net& nd
		) : component(owner_id) {
			controlled_entity_id = nd.controlled_entity_id;
		}
	};
}
