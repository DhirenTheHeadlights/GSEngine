module sandbox:sidearm_impl;

import std;
import gse;

import :characters;
import :sidearm;

auto sandbox::sidearm::run(gse::context& ctx, data& d, const gse::shared_view<gse::actions::data> as, const gse::shared_view<gse::camera::data> cam_s, const gse::shared_view<gse::physics::data> phys_s, const gse::channel_read<gse::network::received<fire_request>> fire_in, const gse::channel_write<gse::network::send_request<fire_request>, spawn_round> fire_out, gse::read<character_controller::component> characters, gse::read<orbit_camera::component> orbits, gse::read<gse::player_controller> controllers, gse::write<component> sidearms) -> gse::async::task<> {
	const auto dt = gse::system_clock::dt();

	const auto sidearm_ids = sidearms.owner_ids();
	for (std::size_t i = 0; i < sidearms.size(); ++i) {
		auto& s = sidearms[i];
		if (s.cooldown > gse::time{}) {
			s.cooldown = std::max(s.cooldown - dt, gse::time{});
		}
	}

	const auto fire = [&](const gse::id owner, component& s, const std::uint32_t shot, const gse::angle yaw, const gse::angle pitch) {
		const auto* character = characters.find(owner);
		if (!character) {
			return;
		}
		const auto snapshot = gse::physics::query_transform(phys_s, character->proxy);
		if (!snapshot) {
			return;
		}

		const auto aim = gse::normalize(
			gse::quat(gse::vec3f(0.f, 1.f, 0.f), yaw) *
			gse::quat(gse::vec3f(1.f, 0.f, 0.f), pitch)
		);
		const gse::vec3f forward = gse::rotate_vector(aim, gse::vec3f(0.f, 0.f, -1.f));
		const gse::vec3f right = gse::rotate_vector(aim, gse::vec3f(1.f, 0.f, 0.f));

		const gse::vec3<gse::displacement> rise(gse::meters(0.f), s.muzzle_height, gse::meters(0.f));
		const gse::vec3<gse::displacement> sideways(right * s.muzzle_side);
		const gse::vec3<gse::displacement> reach(forward * (s.muzzle_forward + s.round_radius));
		const auto muzzle_offset = rise + sideways + reach;

		fire_out.push<spawn_round>({
			.owner = owner,
			.shot = shot,
			.position = snapshot->position + muzzle_offset,
			.launch = forward * s.muzzle_speed,
			.radius = s.round_radius,
			.mass = s.round_mass,
			.lifetime = s.round_lifetime,
		});
		s.fired_seen = shot;
		s.cooldown = s.refire_delay;
	};

	for (const auto& received : fire_in.of<gse::network::received<fire_request>>()) {
		const auto* pc = controllers.find(received.controller);
		if (!pc || !pc->controlled_entity_id.exists()) {
			continue;
		}
		auto* s = sidearms.find(pc->controlled_entity_id);
		if (!s || received.message.shot <= s->fired_seen || s->cooldown > gse::time{}) {
			continue;
		}
		fire(pc->controlled_entity_id, *s, received.message.shot, received.message.yaw, received.message.pitch);
	}

	const auto& local_state = gse::actions::current_state(as);
	const bool trigger = !cam_s.ui_focus && gse::actions::pressed(d.binds.fire, local_state, as);
	if (!trigger) {
		return {};
	}

	for (std::size_t i = 0; i < sidearms.size(); ++i) {
		auto& s = sidearms[i];
		const auto owner = sidearm_ids[i];
		const auto* character = characters.find(owner);
		const auto* orbit = orbits.find(owner);
		if (!character || !character->possessed || !orbit || s.cooldown > gse::time{}) {
			continue;
		}
		++d.shot;
		fire_out.push<gse::network::send_request<fire_request>>({
			.message = {
				.shot = d.shot,
				.yaw = orbit->yaw,
				.pitch = orbit->pitch,
			},
			.reliable = true,
		});
		fire(owner, s, d.shot, orbit->yaw, orbit->pitch);
	}

	return {};
}

auto sandbox::sidearm::rounds::run(data& d, const gse::shared_view<gse::world_system::data> world_d, const gse::channel_read<spawn_round> spawn_in, gse::structural<gse::physics::transform_component> round_transforms, gse::structural<gse::physics::motion_component>, gse::structural<gse::physics::collision_component>, gse::structural<gse::primitive_sphere_spec>) -> gse::async::task<> {
	auto* scene = world_d.active_scene_ptr;
	if (!scene) {
		return {};
	}

	const auto dt = gse::system_clock::dt();

	for (auto& round : d.rounds) {
		round.remaining -= dt;
	}

	std::erase_if(
		d.rounds,
		[scene, &round_transforms](const live_round& r) {
			if (!round_transforms.contains(r.entity)) {
				return true;
			}
			if (r.remaining > gse::time{}) {
				return false;
			}
			scene->remove_entity(r.entity);
			return true;
		}
	);

	for (const auto& request : spawn_in.of<spawn_round>()) {
		const auto round = scene->build(predicted_entity_name("Round", request.owner, request.shot))
			.with<gse::physics::transform_component>({
				.position = request.position,
			})
			.with<gse::physics::motion_component>({
				.current_velocity = request.launch,
				.body = gse::physics::dynamic_body{
					.mass = request.mass,
				},
			})
			.with<gse::physics::collision_component>({
				.shape = gse::physics::sphere_shape{
					.radius = request.radius,
				},
			})
			.with<gse::primitive_sphere_spec>({
				.material = {
					.base_color = gse::vec3f(0.93f, 0.78f, 0.32f),
					.roughness = 0.25f,
					.metallic = 0.9f,
				},
				.lod = gse::sphere_lod::lo,
				.radius = request.radius,
			})
			.identify();

		d.rounds.push_back({
			.entity = round,
			.remaining = request.lifetime,
		});
	}

	return {};
}
