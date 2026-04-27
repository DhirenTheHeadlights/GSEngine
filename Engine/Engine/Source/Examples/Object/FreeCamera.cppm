export module gse.examples:free_camera;

import std;

import gse.runtime;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.graphics;
import gse.physics;
import gse.os;
import gse.assets;
import gse.gpu;

export namespace gse::free_camera {
	struct component_data {
		vec3<position> initial_position = vec3<position>(0.f, 0.f, 5.f);
		int priority = 10;
		velocity speed = meters_per_second(100.f);
	};

	using component = component<component_data>;

	struct bindings {
		actions::button_channel forward;
		actions::button_channel left;
		actions::button_channel back;
		actions::button_channel right;
		actions::button_channel up;
		actions::button_channel down;
		actions::axis2_channel move;
	};

	struct state : non_copyable {
		~state() override = default;

		std::unordered_map<id, std::unique_ptr<bindings>> bindings_by_owner;
	};

	struct system {
		static auto update(
			update_context& ctx,
			state& s
		) -> async::task<>;
	};
}

auto gse::free_camera::system::update(update_context& ctx, state& s) -> async::task<> {
	auto [cameras, follows] = co_await ctx.acquire<
		write<component>,
		write<camera::follow_component>
	>();

	for (auto& c : cameras) {
		const auto owner_id = c.owner_id();
		auto& slot = s.bindings_by_owner[owner_id];
		if (slot) {
			continue;
		}

		slot = std::make_unique<bindings>();
		auto& b = *slot;

		actions::bind_button_channel<"FreeCamera_Move_Forward">(owner_id, key::w, b.forward);
		actions::bind_button_channel<"FreeCamera_Move_Left">(owner_id, key::a, b.left);
		actions::bind_button_channel<"FreeCamera_Move_Backward">(owner_id, key::s, b.back);
		actions::bind_button_channel<"FreeCamera_Move_Right">(owner_id, key::d, b.right);
		actions::bind_button_channel<"FreeCamera_Move_Up">(owner_id, key::space, b.up);
		actions::bind_button_channel<"FreeCamera_Move_Down">(owner_id, key::left_control, b.down);

		actions::bind_axis2_channel(
			owner_id,
			actions::pending_axis2_info{
				.left = b.left.handle(),
				.right = b.right.handle(),
				.back = b.back.handle(),
				.fwd = b.forward.handle(),
				.scale = 1.f,
			},
			b.move,
			trace_id<"FreeCamera_Move">()
		);

		ctx.add_component<camera::follow_component>(owner_id, {
			.offset = vec3<length>(meters(0.f)),
			.priority = c.priority,
			.blend_in_duration = milliseconds(300),
			.active = true,
			.use_entity_position = false,
			.position = c.initial_position,
		});
	}

	for (auto& c : cameras) {
		const auto owner_id = c.owner_id();
		const auto& b = *s.bindings_by_owner[owner_id];

		auto* cam_follow = follows.find(owner_id);
		if (!cam_follow) {
			continue;
		}

		const auto v = b.move.value;
		const float lift = (b.up.held ? 1.f : 0.f) - (b.down.held ? 1.f : 0.f);

		const auto direction = camera_direction_relative_to_origin(
			{ v.x(), lift, v.y() },
			owner_id
		);
		cam_follow->position += direction * c.speed * system_clock::dt();
	}

	std::erase_if(s.bindings_by_owner, [&cameras](const auto& entry) {
		return !cameras.find(entry.first);
	});

	co_return;
}
