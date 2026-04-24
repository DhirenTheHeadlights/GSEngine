export module gse.graphics:camera_component;

import std;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::camera {
	struct follow_component_data {
		vec3<length> offset{};
		int priority = 50;
		time blend_in_duration = milliseconds(300);
		bool active = true;
		bool use_entity_position = true;
		vec3<position> position{};
	};

	struct follow_component : component<follow_component_data> {
		follow_component(const id owner_id, const params& p) : component(owner_id, p) {}
	};
}
