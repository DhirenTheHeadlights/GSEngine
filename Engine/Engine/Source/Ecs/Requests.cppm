export module gse.ecs:requests;

import std;

import gse.core;
import gse.math;

export namespace gse {
	struct set_networked_request {
		bool value = false;
	};

	struct set_authoritative_request {
		bool value = true;
	};

	struct set_local_controller_id_request {
		id controller_id;
	};

	struct activate_scene_request {
		id scene_id;
	};

	struct deactivate_active_scene_request {};

	struct camera_yaw_request {};

	struct camera_yaw_response {
		angle yaw{};
	};
}
