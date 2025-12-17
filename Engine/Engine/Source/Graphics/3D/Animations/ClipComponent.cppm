export module gse.graphics:clip_component;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse {
	struct clip_component_data {
		id clip_id;
		time scale = 1.f;
		bool loop = true;
	};

	struct clip_component : component<clip_component_data> {
		using component::component;

		bool playing = true;
	};
}