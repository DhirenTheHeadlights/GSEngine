export module gse.examples:free_camera;

import std;

import gse.runtime;
import gse.utility;
import gse.graphics;
import gse.physics;
import gse.platform;

export namespace gse {
	class free_camera final : public hook<entity> {
	public:
		struct params {
			vec3<length> initial_position = vec3<length>(0.f, 0.f, 5.f);
		};

		explicit free_camera(const params& p) : m_initial_position(p.initial_position) {}

		auto initialize() -> void override {
			renderer::camera().set_position(m_initial_position);

			actions::bind_button_channel<"FreeCamera_Move_Forward">(owner_id(), key::w, m_forward);
			actions::bind_button_channel<"FreeCamera_Move_Left">(owner_id(), key::a, m_left);
			actions::bind_button_channel<"FreeCamera_Move_Backward">(owner_id(), key::s, m_back);
			actions::bind_button_channel<"FreeCamera_Move_Right">(owner_id(), key::d, m_right);
			actions::bind_button_channel<"FreeCamera_Move_Up">(owner_id(), key::space, m_up);
			actions::bind_button_channel<"FreeCamera_Move_Down">(owner_id(), key::left_control, m_down);

			actions::bind_axis2_channel(
				owner_id(),
				actions::pending_axis2_info{
					.left = m_left.action_id,
					.right = m_right.action_id,
					.back = m_back.action_id,
					.fwd = m_forward.action_id,
					.scale = 1.f
				},
				m_move_axis_channel,
				find_or_generate_id("FreeCamera_Move")
			);
		}

		auto update() -> void override {
			auto& camera = renderer::camera();

			const auto v = m_move_axis_channel.value;
			const float lift = (m_up.held ? 1.f : 0.f) - (m_down.held ? 1.f : 0.f);

			camera.move(camera.direction_relative_to_origin({ v.x(), lift, v.y() }) * 100.f);
		}
	private:
		vec3<length> m_initial_position;

		actions::button_channel m_forward;
		actions::button_channel m_left;
		actions::button_channel m_back;
		actions::button_channel m_right;
		actions::button_channel m_up;
		actions::button_channel m_down;
		actions::axis2_channel m_move_axis_channel;
	};
}
