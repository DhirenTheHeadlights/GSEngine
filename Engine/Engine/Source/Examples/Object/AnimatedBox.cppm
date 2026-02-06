export module gse.examples:animated_box;

import std;

import gse.runtime;
import gse.utility;
import gse.physics;
import gse.graphics;
import gse.platform;

import :procedural_models;
import :animation;

export namespace gse {
	class animated_box final : public hook<entity> {
	public:
		struct params {
			vec3<length> initial_position = vec3<length>(0.f, 0.f, 0.f);
			vec3<length> size = vec3<length>(1.f, 1.f, 1.f);
		};

		explicit animated_box(const params& p) : m_initial_position(p.initial_position), m_size(p.size) {}

		auto initialize() -> void override {
			add_component<physics::motion_component>({
				.current_position = m_initial_position,
				.affected_by_gravity = false
			});

			add_component<physics::collision_component>({
				.bounding_box = { m_initial_position, m_size }
			});

			const auto mat = gse::queue<material>(
				"animated_box_material",
				gse::get<texture>("Textures/Textures/concrete_bricks_1")
			);

			add_component<render_component>({
				.models = { procedural_model::box(mat) }
			});

			m_skeleton_id = animation_examples::create_test_skeleton("animated_box_skeleton");
			m_clip_id = animation_examples::create_oscillating_clip("animated_box_clip", seconds(2.f));

			add_component<animation_component>({
				.skeleton_id = m_skeleton_id
			});

			add_component<clip_component>({
				.clip_id = m_clip_id,
				.scale = 1.f,
				.loop = true
			});

			actions::bind_button_channel<"AnimatedBox_Toggle">(owner_id(), key::p, m_toggle_channel);
		}

		auto update() -> void override {
			if (auto* clip = try_component_write<clip_component>(); m_toggle_channel.pressed && clip) {
				clip->playing = !clip->playing;
			}

			const auto* anim = try_component_read<animation_component>();

			if (!anim || anim->global_pose.empty()) {
				return;
			}

			const mat4& root_pose = anim->global_pose[0];

			auto& motion = component_write<physics::motion_component>();
			motion.current_position = m_initial_position + vec3<length>(
				meters(root_pose[3][0]),
				meters(root_pose[3][1]),
				meters(root_pose[3][2])
			);
		}
	private:
		vec3<length> m_initial_position;
		vec3<length> m_size;
		id m_skeleton_id;
		id m_clip_id;

		actions::button_channel m_toggle_channel;
	};
}
