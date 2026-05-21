export module gse.ecs:requests;

import std;

import gse.core;
import gse.concurrency;
import gse.meta;

export namespace gse {
	struct [[= same_frame_channel]] set_networked_request {
		bool value = false;
	};

	struct [[= same_frame_channel]] set_authoritative_request {
		bool value = true;
	};

	struct [[= same_frame_channel]] set_local_controller_id_request {
		id controller_id;
	};

	struct [[= same_frame_channel]] activate_scene_request {
		id scene_id;
	};

	struct [[= same_frame_channel]] deactivate_active_scene_request {};
}
