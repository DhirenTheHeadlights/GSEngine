export module gse.graphics:clip_component;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.time;
import gse.ecs;
import gse.meta;
import :clip;

export namespace gse {
	struct clip_component {
		[[= networked]] id clip_id;
		[[= networked]] float scale = 1.f;
		[[= networked]] bool loop = true;

		resource::handle<clip_asset> clip;
		time t;
		bool playing = true;
	};
}
