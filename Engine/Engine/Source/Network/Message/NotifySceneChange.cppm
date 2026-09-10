export module gse.network:notify_scene_change;


import gse.core;

import :message;

export namespace gse::network {
	struct [[= network_message{}]] notify_scene_change {
		id scene_id{};
	};
}