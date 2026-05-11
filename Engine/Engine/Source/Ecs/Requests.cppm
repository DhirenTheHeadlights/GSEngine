export module gse.ecs:requests;

import std;

import gse.core;
import gse.concurrency;

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

	template <>
	struct same_frame_channel_t<set_networked_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<set_authoritative_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<set_local_controller_id_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<activate_scene_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<deactivate_active_scene_request> : std::true_type {};
}
