export module gse.graphics:clip_component;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import :clip;

export namespace gse {
	struct clip_component_data {
		id clip_id;
		float scale = 1.f;
		bool loop = true;
	};

	struct clip_component : component<clip_component_data, clip_component_data> {
		clip_component(const id owner_id, const params& p) : component(owner_id, p) {}

		resource::handle<clip_asset> clip;
		time t;
		bool playing = true;
	};
}
