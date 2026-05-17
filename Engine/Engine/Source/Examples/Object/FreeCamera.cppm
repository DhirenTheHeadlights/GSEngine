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
	struct component {
		vec3<position> initial_position = vec3<position>(0.f, 0.f, 5.f);
		int priority = 10;
		velocity speed = meters_per_second(100.f);
	};

	struct bindings {
		actions::handle forward;
		actions::handle left;
		actions::handle back;
		actions::handle right;
		actions::handle up;
		actions::handle down;
		id move_axis_id;
	};

	struct system {
		struct data {
			std::unordered_map<id, std::unique_ptr<bindings>> bindings_by_owner;
		};

		static auto run(
			run_context& ctx,
			data& d,
			const actions::system::data& as,
			const camera::system::data& cam_s
		) -> async::task<>;
	};
}

auto gse::free_camera::system::run(run_context& ctx, data& d, const actions::system::data& as, const camera::system::data& cam_s) -> async::task<> {
	while (true) {
		{
			auto [cameras, follows] = co_await ctx.acquire_with(
				write_v<component>,
				write_v<camera::follow_component>
			);

			const auto camera_ids = cameras.owner_ids();
			for (std::size_t i = 0; i < cameras.size(); ++i) {
				auto& c = cameras[i];
				const auto owner_id = camera_ids[i];
				auto& slot = d.bindings_by_owner[owner_id];
				if (slot) {
					continue;
				}

				slot = std::make_unique<bindings>();
				auto& b = *slot;

				b.forward = actions::add<"FreeCamera_Move_Forward">(ctx.channels, key::w);
				b.left = actions::add<"FreeCamera_Move_Left">(ctx.channels, key::a);
				b.back = actions::add<"FreeCamera_Move_Backward">(ctx.channels, key::s);
				b.right = actions::add<"FreeCamera_Move_Right">(ctx.channels, key::d);
				b.up = actions::add<"FreeCamera_Move_Up">(ctx.channels, key::space);
				b.down = actions::add<"FreeCamera_Move_Down">(ctx.channels, key::left_control);

				b.move_axis_id = actions::bind_axis2(
					ctx.channels,
					actions::pending_axis2_info{
						.left = b.left,
						.right = b.right,
						.back = b.back,
						.fwd = b.forward,
						.scale = 1.f,
					},
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

			const auto& cs = actions::system::current_state(as);

			for (std::size_t i = 0; i < cameras.size(); ++i) {
				auto& c = cameras[i];
				const auto owner_id = camera_ids[i];
				const auto& b = *d.bindings_by_owner[owner_id];

				auto* cam_follow = follows.find(owner_id);
				if (!cam_follow) {
					continue;
				}

				const auto v = cs.axis2_v(static_cast<std::uint16_t>(b.move_axis_id.number()));
				const float lift = (actions::held(b.up, cs, as) ? 1.f : 0.f) - (actions::held(b.down, cs, as) ? 1.f : 0.f);

				const auto direction = camera::system::direction_relative_to_origin(
					cam_s,
					{ v.x(), lift, v.y() }
				);
				(void)owner_id;
				cam_follow->position += direction * c.speed * system_clock::dt();
			}

			std::erase_if(d.bindings_by_owner, [&cameras](const auto& entry) {
				return !cameras.find(entry.first);
			});
		}

		co_await ctx.next_tick();
	}
}
