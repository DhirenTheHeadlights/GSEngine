export module gse.network:registry_sync;

import std;

import :message;
import :bitstream;

import gse.physics;
import gse.graphics;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.std_meta;
import gse.meta;

export namespace gse::network {
	template <typename T>
	struct[[= network_message{}]] component_upsert {
		id owner_id;
		network_data_t<T> data;
	};

	template <typename T>
	struct[[= network_message{}]] component_remove {
		id owner_id;
	};
}
