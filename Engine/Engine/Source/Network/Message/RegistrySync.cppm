export module gse.network:registry_sync;


import gse.core;
import gse.ecs;

import :message;

export namespace gse::network {
	template <typename T>
	struct [[= network_message{}]] component_upsert {
		id owner_id;
		network_data_t<T> data;
	};

	template <typename T>
	struct [[= network_message{}]] component_remove {
		id owner_id;
	};
}