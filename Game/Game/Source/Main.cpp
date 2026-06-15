import std;

import gse;
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
}

auto gs::startup::run_game(const gse::engine_config& engine) -> void {
	gse::start(
		[](gse::engine& e) -> void {
			e.add_system<gse::network::system_for<gs::networked_components>>();
			e.add_system<gs::crosshair_system>();
			e.add_system<gs::client_system>();
			e.add_system<gs::client_ui_system>();
			e.add_system<gs::pause_menu_system>();
			e.add_system<gs::dev_spawn_system>();
			e.add_system<gse::gui::popout_system>();
			gs::world_loader_setup(e);
		},
		engine
	);
}

auto gs::startup::run_locomotion_smoke(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	gse::start(
		[cfg](gse::engine& e) -> void {
			e.add_system<gs::locomotion::state_estimator>();
			e.add_system<gs::locomotion::gait_scheduler>();
			e.add_system<gs::locomotion::footstep_planner>();
			e.add_system<gs::locomotion::balance_controller>();
			e.add_system<gs::locomotion::leg_controller>();
			e.add_system<gs::locomotion::recorder>(gs::locomotion::recorder_config{
				.enabled = cfg.locomotion_record,
				.path = cfg.locomotion_record_path,
			});
			e.add_system<gs::pose_driver::system>();
			auto* sandbox = gs::world_loader_setup(e);
			if (sandbox) {
				gse::activate_scene(e.world(), sandbox->id());
			}
			e.add_system<gs::locomotion::smoke_test>(cfg.smoke);
		},
		{
			.title = "GoonSquad Locomotion Smoke",
			.create_window = false,
			.render = false,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto gs::startup::run_locomotion_train(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	gse::start(
		[cfg](gse::engine& e) -> void {
			e.add_system<gs::locomotion::state_estimator>();
			e.add_system<gs::locomotion::trainer>(cfg.ppo);
			e.add_system<gs::pose_driver::system>();
			auto* training = gs::world_training_setup(e, cfg.ppo.n_envs);
			if (training) {
				gse::activate_scene(e.world(), training->id());
			}
		},
		{
			.title = "GoonSquad Locomotion Train",
			.create_window = false,
			.render = false,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto main(int argc, char** argv) -> int {
	const auto cfg = gse::parse_args<gs::startup::config>(argc, argv);
	if (cfg.locomotion_selftest) {
		return gs::locomotion::locomotion_selftest() ? 0 : 1;
	}
	if (cfg.locomotion_train) {
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
