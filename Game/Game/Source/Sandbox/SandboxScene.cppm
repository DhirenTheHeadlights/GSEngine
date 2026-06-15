export module gs:sandbox_scene;

import std;
import gse;

import :entity_builders;
import :humanoid_skeleton;
import :locomotion_types;
import :orbit_camera;
import :player;

export namespace gs {
	auto sandbox_scene_setup(
		gse::scene& s
	) -> void;

	auto world_training_setup(
		gse::engine& e,
		std::size_t n_envs
	) -> gse::scene*;

	auto physics_parity_world_setup(
		gse::engine& e
	) -> gse::scene*;
}

export namespace gs::physics_parity {
	struct [[= gse::system_state<"Physics Parity">{}]] data {
		int steps_run = 0;
		int max_steps = 200;
		std::uint64_t hash = 1469598103934665603ull;
		std::optional<gse::id> scene_id;
		bool done = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<gse::physics::transform_component> transforms,
		gse::read<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

auto gs::sandbox_scene_setup(gse::scene& s) -> void {
	s.set_player_factory([next_id = 0u](gse::scene& sc, std::optional<gse::id> server_id) mutable -> gse::id {
		gse::id player_id;
		if (server_id) {
			player_id = gse::find_or_generate_id(server_id->number());
			auto& reg = sc.registry();
			reg.ensure_exists(player_id);
			reg.ensure_active(player_id);
		}
		else {
			player_id = sc.add_entity(std::format("Player_{}", next_id++));
		}
		sc.registry().add_component<gse::free_camera::component>(
			player_id,
			{
				.initial_position = gse::vec3<gse::position>(0.f, 2.f, 0.f),
			}
		);
		return player_id;
	});

	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
	s.spawn(
		"Floor",
		gs::static_box(
			gse::vec3<gse::position>(0.f, -0.501f, 0.f),
			floor_size,
			gse::quat(1.f, 0.f, 0.f, 0.f),
			gse::vec3f(0.08f, 0.08f, 0.09f),
			0.45f,
			0.0f
		)
	);

	const auto player_initial_pos = gse::vec3<gse::position>(0.f, 1.005f, 0.f);
	const auto player_initial_yaw = gse::degrees(-90.f);
	const auto player_initial_orientation = gse::quat(gse::vec3f(0.f, 1.f, 0.f), player_initial_yaw);
	const auto humanoid_handle = gs::spawn_humanoid(s, player_initial_pos, player_initial_orientation);
	const auto pelvis_id = humanoid_handle.bone_ids[0];

	if (auto* m = s.registry().try_component<gse::physics::motion_component>(pelvis_id)) {
		if (auto* dyn = std::get_if<gse::physics::dynamic_body>(&m->body)) {
			dyn->update_orientation = true;
		}
	}

	s.registry().add_component<gs::locomotion::skeleton_refs>(
		pelvis_id,
		{
			.pelvis_id = pelvis_id,
			.torso_id = humanoid_handle.bone_ids[1],
			.thigh_l_id = humanoid_handle.bone_ids[9],
			.shin_l_id = humanoid_handle.bone_ids[10],
			.foot_l_id = humanoid_handle.bone_ids[11],
			.toe_l_id = humanoid_handle.bone_ids[15],
			.thigh_r_id = humanoid_handle.bone_ids[12],
			.shin_r_id = humanoid_handle.bone_ids[13],
			.foot_r_id = humanoid_handle.bone_ids[14],
			.toe_r_id = humanoid_handle.bone_ids[16],
			.hip_l_joint_id = humanoid_handle.joint_ids[8],
			.knee_l_joint_id = humanoid_handle.joint_ids[9],
			.ankle_l_joint_id = humanoid_handle.joint_ids[10],
			.hip_r_joint_id = humanoid_handle.joint_ids[11],
			.knee_r_joint_id = humanoid_handle.joint_ids[12],
			.ankle_r_joint_id = humanoid_handle.joint_ids[13],
			.shoulder_l_joint_id = humanoid_handle.joint_ids[2],
			.shoulder_r_joint_id = humanoid_handle.joint_ids[5],
			.thigh_length = gse::meters(0.45f),
			.shin_length = gse::meters(0.40f),
			.hip_offset_lateral = gse::meters(0.075f),
			.hip_offset_below_pelvis = gse::meters(0.10f),
			.pelvis_target_height = gse::meters(0.90f),
		}
	);

	s.registry().add_component<gs::locomotion::intent>(
		pelvis_id,
		{}
	);
	s.registry().add_component<gs::locomotion::state>(
		pelvis_id,
		{}
	);
	s.registry().add_component<gs::locomotion::gait>(
		pelvis_id,
		{}
	);
	s.registry().add_component<gs::locomotion::plan>(
		pelvis_id,
		{}
	);
	s.registry().add_component<gs::locomotion::leg_context>(
		pelvis_id,
		{}
	);
	s.registry().add_component<gse::physics::motor_component>(
		pelvis_id,
		{
			.velocity_drive_target = {},
			.horizontal_only = true,
			.requires_ground_contact = false,
			.max_force = gse::newtons(180.f),
		}
	);
	s.registry().add_component<gse::physics::motor_component>(
		humanoid_handle.bone_ids[11],
		{
			.velocity_drive_target = {},
			.horizontal_only = true,
			.requires_ground_contact = true,
			.max_force = gse::newtons(0.f),
		}
	);
	s.registry().add_component<gse::physics::motor_component>(
		humanoid_handle.bone_ids[14],
		{
			.velocity_drive_target = {},
			.horizontal_only = true,
			.requires_ground_contact = true,
			.max_force = gse::newtons(0.f),
		}
	);

	const auto player_id = s.build("Player")
		.with<gs::player::component>({
			.pelvis_id = pelvis_id,
			.yaw = player_initial_yaw,
		})
		.identify();

	s.build("Orbit Camera").with<gs::orbit_camera::component>({
		.target = player_id,
	});

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 5.f, 10.f),
		});
}

namespace gs {
	inline std::size_t g_training_n_envs = 32;
}

auto training_scene_setup(gse::scene& s) -> void {
	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
	s.spawn(
		"Floor",
		gs::static_box(
			gse::vec3<gse::position>(0.f, -0.501f, 0.f),
			floor_size,
			gse::quat(1.f, 0.f, 0.f, 0.f),
			gse::vec3f(0.08f, 0.08f, 0.09f),
			0.45f,
			0.0f
		)
	);

	const auto n = gs::g_training_n_envs;
	const auto grid_side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(n))));
	const auto spacing = 8.0f;

	for (std::size_t i = 0; i < n; ++i) {
		const auto row = static_cast<int>(i) / grid_side;
		const auto col = static_cast<int>(i) % grid_side;
		const auto x = (static_cast<float>(col) - static_cast<float>(grid_side - 1) * 0.5f) * spacing;
		const auto z = (static_cast<float>(row) - static_cast<float>(grid_side - 1) * 0.5f) * spacing;

		const auto spawn_pos = gse::vec3<gse::position>(gse::meters(x), gse::meters(1.005f), gse::meters(z));
		const auto humanoid = gs::spawn_humanoid(s, spawn_pos, gse::quat(1.f, 0.f, 0.f, 0.f), std::format("humanoid_{}", i));
		const auto pelvis_id = humanoid.bone_ids[0];

		if (auto* m = s.registry().try_component<gse::physics::motion_component>(pelvis_id)) {
			if (auto* dyn = std::get_if<gse::physics::dynamic_body>(&m->body)) {
				dyn->update_orientation = true;
			}
		}

		s.registry().add_component<gs::locomotion::skeleton_refs>(
			pelvis_id,
			{
				.pelvis_id = pelvis_id,
				.torso_id = humanoid.bone_ids[1],
				.thigh_l_id = humanoid.bone_ids[9],
				.shin_l_id = humanoid.bone_ids[10],
				.foot_l_id = humanoid.bone_ids[11],
				.toe_l_id = humanoid.bone_ids[15],
				.thigh_r_id = humanoid.bone_ids[12],
				.shin_r_id = humanoid.bone_ids[13],
				.foot_r_id = humanoid.bone_ids[14],
				.toe_r_id = humanoid.bone_ids[16],
				.hip_l_joint_id = humanoid.joint_ids[8],
				.knee_l_joint_id = humanoid.joint_ids[9],
				.ankle_l_joint_id = humanoid.joint_ids[10],
				.hip_r_joint_id = humanoid.joint_ids[11],
				.knee_r_joint_id = humanoid.joint_ids[12],
				.ankle_r_joint_id = humanoid.joint_ids[13],
				.shoulder_l_joint_id = humanoid.joint_ids[2],
				.shoulder_r_joint_id = humanoid.joint_ids[5],
				.thigh_length = gse::meters(0.45f),
				.shin_length = gse::meters(0.40f),
				.hip_offset_lateral = gse::meters(0.075f),
				.hip_offset_below_pelvis = gse::meters(0.10f),
				.pelvis_target_height = gse::meters(0.90f),
				.all_bone_ids = humanoid.bone_ids,
			}
		);

		s.registry().add_component<gs::locomotion::intent>(pelvis_id, {});
		s.registry().add_component<gs::locomotion::state>(pelvis_id, {});
		s.registry().add_component<gs::locomotion::gait>(pelvis_id, {});
		s.registry().add_component<gs::locomotion::plan>(pelvis_id, {});
		s.registry().add_component<gs::locomotion::leg_context>(pelvis_id, {});

		s.registry().add_component<gse::physics::motor_component>(
			pelvis_id,
			{
				.velocity_drive_target = {},
				.horizontal_only = true,
				.requires_ground_contact = false,
				.max_force = gse::newtons(0.f),
			}
		);
		s.registry().add_component<gse::physics::motor_component>(
			humanoid.bone_ids[11],
			{
				.velocity_drive_target = {},
				.horizontal_only = true,
				.requires_ground_contact = true,
				.max_force = gse::newtons(0.f),
			}
		);
		s.registry().add_component<gse::physics::motor_component>(
			humanoid.bone_ids[14],
			{
				.velocity_drive_target = {},
				.horizontal_only = true,
				.requires_ground_contact = true,
				.max_force = gse::newtons(0.f),
			}
		);
	}
}

auto gs::world_training_setup(gse::engine& e, const std::size_t n_envs) -> gse::scene* {
	g_training_n_envs = n_envs;
	auto& w = e.world();
	auto& reg = e.registry();
	return gse::add_scene(w, reg, "Training", &training_scene_setup);
}

auto physics_parity_scene_setup(gse::scene& s) -> void {
	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(50.f), gse::meters(1.f), gse::meters(50.f));
	s.spawn(
		"Floor",
		gs::static_box(
			gse::vec3<gse::position>(0.f, -0.5f, 0.f),
			floor_size
		)
	);

	for (int i = 0; i < 8; ++i) {
		const auto offset = i % 2 == 0 ? 0.04f : -0.04f;
		const auto y = 1.0f + static_cast<float>(i) * 1.05f;
		s.spawn(
			std::format("Box_{}", i),
			gs::box(
				gse::vec3<gse::position>(gse::meters(offset), gse::meters(y), gse::meters(0.f)),
				gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f)),
				gse::kilograms(10.f)
			)
		);
	}
}

auto gs::physics_parity_world_setup(gse::engine& e) -> gse::scene* {
	auto& w = e.world();
	auto& reg = e.registry();
	return gse::add_scene(w, reg, "PhysicsParity", &physics_parity_scene_setup);
}

auto gs::physics_parity::run(gse::context& ctx, data& d, gse::shared_view<gse::world_system::data> world_d, gse::read<gse::physics::transform_component> transforms, gse::read<gse::physics::motion_component> motions) -> gse::async::task<> {
	if (d.done) {
		return {};
	}

	if (!d.scene_id.has_value()) {
		for (const auto& [scene_id, scene_ptr] : world_d.scenes) {
			if (scene_ptr && scene_ptr->id().tag() == std::string_view("PhysicsParity")) {
				d.scene_id = scene_id;
				break;
			}
		}
	}

	if (d.scene_id.has_value() && world_d.active_scene != d.scene_id) {
		ctx.channels.push<gse::activate_scene_request>({ .scene_id = *d.scene_id });
		return {};
	}

	if (motions.empty()) {
		return {};
	}

	const auto fold = [&](auto v) {
		d.hash ^= std::bit_cast<std::uint32_t>(v);
		d.hash *= 1099511628211ull;
	};

	const auto owners = motions.owner_ids();
	for (std::size_t i = 0; i < owners.size(); ++i) {
		const auto* tc = transforms.find(owners[i]);
		const auto* mc = motions.find(owners[i]);
		if (!tc || !mc) {
			continue;
		}
		fold(tc->position.x());
		fold(tc->position.y());
		fold(tc->position.z());
		fold(mc->current_velocity.x());
		fold(mc->current_velocity.y());
		fold(mc->current_velocity.z());
	}

	++d.steps_run;

	if (d.steps_run >= d.max_steps) {
		gse::log::println("physics_parity: steps={} bodies={} hash={:016x}", d.steps_run, owners.size(), d.hash);
		for (std::size_t i = 0; i < owners.size(); ++i) {
			const auto* tc = transforms.find(owners[i]);
			if (!tc) {
				continue;
			}
			gse::log::println("physics_parity: body[{}] pos={:.5f}", i, tc->position);
		}
		d.done = true;
		gse::shutdown();
	}

	return {};
}
