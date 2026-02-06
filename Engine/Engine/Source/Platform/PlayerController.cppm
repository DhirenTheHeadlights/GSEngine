export module gse.platform:player_controller;

import std;

import gse.utility;

export namespace gse {
	struct player_controller_net {
		id controlled_entity_id;
	};

	struct player_controller_data {
		id controlled_entity_id;
		bool local_only_flag = false;
	};

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
