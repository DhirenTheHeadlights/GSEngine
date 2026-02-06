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
			int priority = 10;
		};

		explicit free_camera(const params& p)
			: m_initial_position(p.initial_position)
			, m_priority(p.priority) {}

		auto initialize() -> void override {
			add_component<camera::follow_component>({
				.offset = vec3<length>(0.f),
				.priority = m_priority,
				.blend_in_duration = milliseconds(300),
				.active = true,
				.use_entity_position = false,
				.position = m_initial_position
			});

			actions::bind_button_channel<"FreeCamera_Move_Forward">(owner_id(), key::w, m_forward);
			actions::bind_button_channel<"FreeCamera_Move_Left">(owner_id(), key::a, m_left);
			actions::bind_button_channel<"FreeCamera_Move_Backward">(owner_id(), key::s, m_back);
			actions::bind_button_channel<"FreeCamera_Move_Right">(owner_id(), key::d, m_right);
			actions::bind_button_channel<"FreeCamera_Move_Up">(owner_id(), key::space, m_up);
			actions::bind_button_channel<"FreeCamera_Move_Down">(owner_id(), key::left_control, m_down);

			actions::bind_axis2_channel(
				owner_id(),
				actions::pending_axis2_info{
					.left = m_left.handle(),
					.right = m_right.handle(),
					.back = m_back.handle(),
					.fwd = m_forward.handle(),
					.scale = 1.f
				},
				m_move_axis_channel,
				find_or_generate_id("FreeCamera_Move")
			);
		}

		auto update() -> void override {
			auto& cam_follow = component_write<camera::follow_component>();

			const auto v = m_move_axis_channel.value;
			const float lift = (m_up.held ? 1.f : 0.f) - (m_down.held ? 1.f : 0.f);

			const auto direction = camera_direction_relative_to_origin({ v.x(), lift, v.y() });
			cam_follow.position += direction * meters(100.f) * system_clock::dt().as<seconds>();
		}
	private:
		vec3<length> m_initial_position;
		int m_priority;

		actions::button_channel m_forward;
		actions::button_channel m_left;
		actions::button_channel m_back;
		actions::button_channel m_right;
		actions::button_channel m_up;
		actions::button_channel m_down;
		actions::axis2_channel m_move_axis_channel;
	};
}
