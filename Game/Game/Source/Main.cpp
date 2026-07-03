import std;

import gse;
import gse.config;
import gse.system_manifest;
import gs;

namespace gs::startup {
	struct config {
		gse::engine_config engine;
		bool locomotion_smoke = false;
		gs::locomotion::smoke_config smoke;
		bool locomotion_record = false;
		std::string locomotion_record_path;
		bool locomotion_train = false;
		gs::locomotion::ppo_config ppo;
		bool locomotion_selftest = false;
		bool use_gpu_solver = false;
		bool physics_parity = false;
		std::size_t physics_parity_envs = 1;
		float action_scale = 1.0f;
		bool locomotion_clip_info = false;
		std::string reference_clip_path;
		bool locomotion_play = false;
	};

	auto run_game(
		const gse::engine_config& engine
	) -> void;

	auto run_locomotion_smoke(
		const config& cfg
	) -> void;

	auto run_locomotion_train(
		const config& cfg
	) -> void;

	auto run_physics_parity(
		const config& cfg
	) -> void;

	auto run_locomotion_clip_info(
		const config& cfg
	) -> void;

	auto run_locomotion_play(
		const config& cfg
	) -> void;

	auto locomotion_artifact(
		std::string_view name
	) -> std::string;

	template <typename... Components>
	consteval auto network_run_hook(gse::type_pack<Components...>) -> std::meta::info {
		return ^^gse::network::run<Components...>;
	}
}

auto gs::startup::locomotion_artifact(const std::string_view name) -> std::string {
	return (gse::config::resource_path / "Misc" / name).string();
}

auto gs::startup::run_game(const gse::engine_config& engine) -> void {
	auto play_cfg = gs::locomotion::ppo_config{};
	play_cfg.checkpoint_path = locomotion_artifact("locomotion_checkpoint.bin");
	play_cfg.reference_clip_path = locomotion_artifact("ref_walk.bin");
	gse::start(
		[&play_cfg](gse::engine& e) -> void {
			constexpr auto network_run = network_run_hook(gs::networked_components{});
			static_assert(gse::meta::find_system_hook_anno(network_run) != std::meta::info{}, "network run hook annotation not visible on the templated instantiation");
			gse::system_manifest<^^gse::network::data, network_run, ^^gse::network::shutdown>{}.register_with(e);
			gse::system_manifest<^^gs::crosshair::data, ^^gs::crosshair::draw_settings>{}.register_with(e);
			gse::register_systems<^^gs::client_system>(e);
			gse::system_manifest<^^gs::client_ui::data, ^^gs::client_ui::run, ^^gs::pause_menu::data, ^^gs::pause_menu::run>{}.register_with(e);
			gse::system_manifest<^^gs::dev_spawn::data, ^^gs::dev_spawn::run>{}.register_with(e);
			gse::register_systems<^^gse::gui::popout_system>(e);
			gse::register_systems<^^gs::locomotion::pose_player>(e);
			e.register_external_resource<gs::locomotion::ppo_config>(&play_cfg);
			gs::world_loader_setup(e);
		},
		engine
	);
}

auto gs::startup::run_locomotion_smoke(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	auto smoke = cfg.smoke;
	auto record_cfg = gs::locomotion::recorder_config{
		.enabled = cfg.locomotion_record,
		.path = cfg.locomotion_record_path,
	};
	gse::start(
		[&smoke, &record_cfg](gse::engine& e) -> void {
			gse::system_manifest<
				^^gs::locomotion::state_estimator::data, ^^gs::locomotion::state_estimator::run,
				^^gs::locomotion::gait_scheduler::data, ^^gs::locomotion::gait_scheduler::run,
				^^gs::locomotion::footstep_planner::data, ^^gs::locomotion::footstep_planner::run,
				^^gs::locomotion::balance_controller::data, ^^gs::locomotion::balance_controller::run,
				^^gs::locomotion::leg_controller::data, ^^gs::locomotion::leg_controller::run
			>{}.register_with(e);
			gse::system_manifest<^^gs::pose_driver::data, ^^gs::pose_driver::run>{}.register_with(e);
			auto* sandbox = gs::world_loader_setup(e);
			if (sandbox) {
				gse::activate_scene(e.world(), sandbox->id());
			}
			gse::register_systems<^^gs::locomotion::smoke_test>(e);
			e.register_external_resource<gs::locomotion::smoke_config>(&smoke);
			gse::register_systems<^^gs::locomotion::recorder>(e);
			e.register_external_resource<gs::locomotion::recorder_config>(&record_cfg);
		},
		{
			.title = "GoonSquad Locomotion Smoke",
			.create_window = false,
			.render = false,
			.use_gpu_solver = cfg.use_gpu_solver,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto gs::startup::run_locomotion_train(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	auto ppo = cfg.ppo;
	ppo.action_scale = cfg.action_scale;
	ppo.checkpoint_path = locomotion_artifact("locomotion_checkpoint.bin");
	ppo.reference_clip_path = cfg.reference_clip_path.empty() ? locomotion_artifact("ref_walk.bin") : cfg.reference_clip_path;
	gse::start(
		[&ppo](gse::engine& e) -> void {
			gse::system_manifest<^^gs::locomotion::state_estimator::data, ^^gs::locomotion::state_estimator::run>{}.register_with(e);
			gse::system_manifest<^^gs::pose_driver::data, ^^gs::pose_driver::run>{}.register_with(e);
			gse::register_systems<^^gs::locomotion::trainer>(e);
			e.register_external_resource<gs::locomotion::ppo_config>(&ppo);
			auto* training = gs::world_training_setup(e, ppo.n_envs);
			if (training) {
				gse::activate_scene(e.world(), training->id());
			}
		},
		{
			.title = "GoonSquad Locomotion Train",
			.create_window = false,
			.render = false,
			.use_gpu_solver = cfg.use_gpu_solver,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto gs::startup::run_physics_parity(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	gse::start(
		[n_envs = cfg.physics_parity_envs](gse::engine& e) -> void {
			gse::register_systems<^^gs::physics_parity>(e);
			auto* scene = gs::physics_parity_world_setup(e, n_envs);
			if (scene) {
				gse::activate_scene(e.world(), scene->id());
			}
		},
		{
			.title = "GoonSquad Physics Parity",
			.create_window = false,
			.render = false,
			.use_gpu_solver = cfg.use_gpu_solver,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto gs::startup::run_locomotion_clip_info(const config& cfg) -> void {
	const auto clip = gs::locomotion::load_reference_clip(cfg.locomotion_record_path);
	if (!clip) {
		gse::log::println("clip_info: FAILED to load '{}' (missing, bad magic/version, or layout-hash mismatch)", cfg.locomotion_record_path);
		return;
	}
	const auto& frames = clip->frames;
	const auto n = frames.size();
	auto phi_min = frames[0].kin.phi;
	auto phi_max = frames[0].kin.phi;
	auto y_min = frames[0].kin.pelvis_position.y();
	auto y_max = frames[0].kin.pelvis_position.y();
	auto fwd_sum = gse::meters_per_second(0.f);
	auto fwd_max = gse::meters_per_second(0.f);
	auto moving = 0;
	for (const auto& f : frames) {
		phi_min = std::min(phi_min, f.kin.phi);
		phi_max = std::max(phi_max, f.kin.phi);
		y_min = std::min(y_min, f.kin.pelvis_position.y());
		y_max = std::max(y_max, f.kin.pelvis_position.y());
		const auto fwd = -f.obs.velocity_body.z();
		fwd_sum += fwd;
		fwd_max = std::max(fwd_max, fwd);
		if (fwd > gse::meters_per_second(0.15f)) {
			++moving;
		}
	}
	gse::log::println("clip_info: frames={} dt={:.4f} bone_count={}", n, clip->dt, clip->bone_count);
	gse::log::println("clip_info: phi=[{:.3f},{:.3f}] pelvis_y=[{:.3f},{:.3f}] fwd_speed mean={:.3f} max={:.3f} moving(>0.15)={}/{}",
		phi_min, phi_max, y_min, y_max, fwd_sum / static_cast<float>(n), fwd_max, moving, n);
	if (!frames[0].bones.empty()) {
		gse::log::println("clip_info: frame0 kin.pelvis={:.3f} bones[0].pos={:.3f} (should match)",
			frames[0].kin.pelvis_position, frames[0].bones[0].position);
	}
}

auto gs::startup::run_locomotion_play(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	auto play_cfg = cfg.ppo;
	play_cfg.action_scale = cfg.action_scale;
	play_cfg.checkpoint_path = locomotion_artifact("locomotion_checkpoint.bin");
	play_cfg.reference_clip_path = cfg.reference_clip_path.empty() ? locomotion_artifact("ref_walk.bin") : cfg.reference_clip_path;
	gse::start(
		[&play_cfg](gse::engine& e) -> void {
			gse::system_manifest<
				^^gs::locomotion::state_estimator::data, ^^gs::locomotion::state_estimator::run,
				^^gs::locomotion::gait_scheduler::data, ^^gs::locomotion::gait_scheduler::run,
				^^gs::locomotion::footstep_planner::data, ^^gs::locomotion::footstep_planner::run,
				^^gs::locomotion::balance_controller::data, ^^gs::locomotion::balance_controller::run,
				^^gs::locomotion::leg_controller::data, ^^gs::locomotion::leg_controller::run
			>{}.register_with(e);
			gse::register_systems<^^gs::locomotion::pose_player>(e);
			e.register_external_resource<gs::locomotion::ppo_config>(&play_cfg);
			auto& w = e.world();
			auto& reg = e.registry();
			auto* play = gse::add_scene(w, reg, "Locomotion Play", &gs::locomotion_play_scene_setup);
			gse::add_scene(w, reg, "Sandbox", &gs::sandbox_scene_setup);
			if (play) {
				gse::activate_scene(w, play->id());
			}
		},
		{
			.title = "GoonSquad Locomotion Play",
			.create_window = false,
			.render = false,
			.use_gpu_solver = cfg.use_gpu_solver,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto main(int argc, char** argv) -> int {
	const auto cfg = gse::parse_args<gs::startup::config>(argc, argv);
	if (cfg.locomotion_selftest) {
		const auto mdp_ok = gs::locomotion::locomotion_selftest();
		const auto amp_ok = gs::locomotion::amp_selftest();
		const auto disc_ok = gs::locomotion::discriminator_selftest();
		return mdp_ok && amp_ok && disc_ok ? 0 : 1;
	}
	if (cfg.locomotion_clip_info) {
		gs::startup::run_locomotion_clip_info(cfg);
		return 0;
	}
	if (cfg.locomotion_play) {
		gs::startup::run_locomotion_play(cfg);
		return 0;
	}
	if (cfg.physics_parity) {
		gs::startup::run_physics_parity(cfg);
	}
	else if (cfg.locomotion_train) {
		gs::startup::run_locomotion_train(cfg);
	}
	else if (cfg.locomotion_smoke) {
		gs::startup::run_locomotion_smoke(cfg);
	}
	else {
		gs::startup::run_game(cfg.engine);
	}
	return 0;
}
