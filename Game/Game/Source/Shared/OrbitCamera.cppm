export module gs:orbit_camera;

import std;
import gse;

export namespace gs::orbit_camera {
	struct component {
		gse::id target;
		gse::angle yaw = gse::degrees(0.f);
		gse::angle pitch = gse::degrees(-15.f);
		gse::length distance = gse::meters(8.f);
		gse::length min_distance = gse::meters(1.5f);
		gse::length max_distance = gse::meters(50.f);
		int priority = 60;
		bool active = false;
		float mouse_sensitivity = 0.25f;
		float arrow_speed = 90.f;
		float scroll_zoom_step = 0.5f;
		gse::length collision_radius = gse::meters(0.25f);
		gse::length collision_skin = gse::meters(0.05f);
		bool collide_with_geometry = true;
	};

	struct bindings {
		gse::actions::handle toggle;
		gse::actions::handle yaw_left;
		gse::actions::handle yaw_right;
		gse::actions::handle pitch_up;
		gse::actions::handle pitch_down;
	};

	struct system {
		struct data {
			std::unordered_map<gse::id, bindings> bindings_by_owner;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::actions::system::data& as,
			const gse::input::system::data& input_s,
			const gse::camera::system::data& cam_s,
			const gse::physics::system::data& phys_s
		) -> gse::async::task<>;
	};
}

auto gs::orbit_camera::system::run(gse::run_context& ctx, data& d, const gse::actions::system::data& as, const gse::input::system::data& input_s, const gse::camera::system::data& cam_s, const gse::physics::system::data& phys_s) -> gse::async::task<> {
	while (true) {
		{
			auto [orbits, follows, transforms, collisions, motions] = co_await ctx.acquire_with(
				gse::write_v<component>,
				gse::write_v<gse::camera::follow_component>,
				gse::read_v<gse::physics::transform_component>,
				gse::read_v<gse::physics::collision_component>,
				gse::read_v<gse::physics::motion_component>
			);

			const auto orbit_ids = orbits.owner_ids();
			for (std::size_t i = 0; i < orbits.size(); ++i) {
				const auto& o = orbits[i];
				const auto owner_id = orbit_ids[i];
				auto [it, inserted] = d.bindings_by_owner.try_emplace(owner_id);
				if (!inserted) {
					continue;
				}
				auto& b = it->second;

				b.toggle = gse::actions::add<"Orbit_Toggle">(ctx.channels, gse::key::c);
				b.yaw_left = gse::actions::add<"Orbit_Yaw_Left">(ctx.channels, gse::key::left);
				b.yaw_right = gse::actions::add<"Orbit_Yaw_Right">(ctx.channels, gse::key::right);
				b.pitch_up = gse::actions::add<"Orbit_Pitch_Up">(ctx.channels, gse::key::up);
				b.pitch_down = gse::actions::add<"Orbit_Pitch_Down">(ctx.channels, gse::key::down);

				const gse::quat initial_orientation = gse::normalize(
					gse::quat(gse::vec3f(0.f, 1.f, 0.f), o.yaw) *
					gse::quat(gse::vec3f(1.f, 0.f, 0.f), o.pitch)
				);

				ctx.add_component<gse::camera::follow_component>(
					owner_id,
					{
						.offset = gse::vec3<gse::length>(gse::meters(0.f)),
						.priority = o.priority,
						.blend_in_duration = gse::milliseconds(300),
						.active = o.active,
						.use_entity_position = false,
						.orientation = initial_orientation,
					}
				);
			}

			const auto& cs = gse::actions::system::current_state(as);
			const auto& in = gse::input::system::current_state(input_s);
			const float dt_seconds = gse::system_clock::dt() / gse::seconds(1.f);

			for (std::size_t i = 0; i < orbits.size(); ++i) {
				auto& o = orbits[i];
				const auto owner_id = orbit_ids[i];
				const auto& b = d.bindings_by_owner[owner_id];

				auto* cam_follow = follows.find(owner_id);
				if (!cam_follow) {
					continue;
				}

				if (gse::actions::pressed(b.toggle, cs, as)) {
					o.active = !o.active;
					cam_follow->active = o.active;
				}

				if (!o.active) {
					continue;
				}

				const bool middle_held = !cam_s.ui_focus && in.mouse_button_held(gse::mouse_button::button_3);
				if (middle_held) {
					const auto delta = in.mouse_delta();
					o.yaw -= gse::degrees(delta.x() * o.mouse_sensitivity);
					o.pitch -= gse::degrees(delta.y() * o.mouse_sensitivity);
				}
				else {
					const float yaw_axis = (gse::actions::held(b.yaw_left, cs, as)
												? 1.f
												: 0.f) -
						(gse::actions::held(b.yaw_right, cs, as)
							 ? 1.f
							 : 0.f);
					const float pitch_axis = (gse::actions::held(b.pitch_up, cs, as)
												  ? 1.f
												  : 0.f) -
						(gse::actions::held(b.pitch_down, cs, as)
							 ? 1.f
							 : 0.f);
					o.yaw += gse::degrees(yaw_axis * o.arrow_speed * dt_seconds);
					o.pitch += gse::degrees(pitch_axis * o.arrow_speed * dt_seconds);
				}

				o.pitch = std::clamp(o.pitch, gse::degrees(-89.f), gse::degrees(89.f));

				if (!cam_s.ui_focus) {
					const auto scroll = in.scroll_delta();
					o.distance -= gse::meters(scroll.y() * o.scroll_zoom_step);
				}
				o.distance = std::clamp(o.distance, o.min_distance, o.max_distance);

				const gse::quat orientation = gse::normalize(
					gse::quat(gse::vec3f(0.f, 1.f, 0.f), o.yaw) *
					gse::quat(gse::vec3f(1.f, 0.f, 0.f), o.pitch)
				);

				gse::vec3<gse::position> target_pos;
				if (auto snap = gse::physics::system::query_transform(phys_s, o.target)) {
					target_pos = snap->position;
				}
				else if (const auto* tc = transforms.find(o.target)) {
					target_pos = tc->position;
				}
				else {
					continue;
				}

				const gse::vec3f forward = gse::rotate_vector(orientation, gse::vec3f(0.f, 0.f, -1.f));
				const auto inflation = o.collision_radius * 2.f;
				const auto for_each_static_box = [&](auto body) {
					const auto col_ids = collisions.owner_ids();
					for (std::size_t k = 0; k < collisions.size(); ++k) {
						const auto col_eid = col_ids[k];
						const auto* mc = motions.find(col_eid);
						if (!mc || !gse::physics::is_static(*mc)) {
							continue;
						}
						const auto* col_tc = transforms.find(col_eid);
						if (!col_tc) {
							continue;
						}
						const auto* shape = std::get_if<gse::physics::box_shape>(&collisions[k].shape);
						if (!shape) {
							continue;
						}
						const gse::physics::box_shape inflated{
							.size = shape->size + gse::vec3<gse::displacement>(inflation, inflation, inflation)
						};
						body(gse::bounding_box(*col_tc, inflated));
					}
				};

				gse::vec3<gse::displacement> spring_displacement = -forward * o.distance;

				if (o.collide_with_geometry) {
					for_each_static_box([&](const gse::bounding_box& bb) {
						const auto q = gse::narrow_phase_collision::query_obb(bb, target_pos);
						if (q.signed_distance < gse::meters(0.f)) {
							const auto into_wall = -gse::dot(spring_displacement, q.normal);
							if (into_wall > gse::meters(0.f)) {
								spring_displacement += q.normal * into_wall;
							}
						}
					});
				}

				const auto desired_camera_pos = target_pos + spring_displacement;
				const auto displacement_mag = gse::magnitude(spring_displacement);
				float spring_factor = 1.f;
				if (o.collide_with_geometry && displacement_mag > gse::meters(1e-5f)) {
					for_each_static_box([&](const gse::bounding_box& bb) {
						if (const auto hit = gse::narrow_phase_collision::segment_obb_first_hit(bb, target_pos, desired_camera_pos)) {
							const auto safe = std::max(hit->distance - o.collision_skin, gse::meters(0.f));
							const float factor = safe / displacement_mag;
							if (factor < spring_factor) {
								spring_factor = factor;
							}
						}
					});
				}

				auto camera_pos = target_pos + spring_displacement * spring_factor;

				if (o.collide_with_geometry) {
					for_each_static_box([&](const gse::bounding_box& bb) {
						const auto result = gse::narrow_phase_collision::query_obb(bb, camera_pos);
						if (result.signed_distance < o.collision_skin) {
							const auto push = o.collision_skin - result.signed_distance;
							camera_pos = camera_pos + result.normal * push;
						}
					});
				}

				cam_follow->position = camera_pos;
				cam_follow->orientation = orientation;
			}

			std::erase_if(
				d.bindings_by_owner,
				[&orbits](const auto& entry) {
					return !orbits.find(entry.first);
				}
			);
		}

		co_await ctx.next_tick();
	}
}
