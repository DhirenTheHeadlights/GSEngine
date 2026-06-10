import std;

import gse;
import gs;

namespace gs::startup {
	struct config {
		gse::engine_config engine;
		bool locomotion_smoke = false;
		gs::locomotion::smoke_config smoke;
	};

	auto run_game(
		const gse::engine_config& engine
	) -> void;
	auto run_locomotion_smoke(
		gs::locomotion::smoke_config smoke
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

auto gs::startup::run_locomotion_smoke(gs::locomotion::smoke_config smoke) -> void {
	gse::system_clock::set_fixed_step_override(1);
	gse::start(
		[smoke](gse::engine& e) -> void {
			e.add_system<gs::locomotion::state_estimator>();
			e.add_system<gs::locomotion::gait_scheduler>();
			e.add_system<gs::locomotion::footstep_planner>();
			e.add_system<gs::locomotion::balance_controller>();
			e.add_system<gs::locomotion::leg_controller>();
			e.add_system<gs::pose_driver::system>();
			auto* sandbox = gs::world_loader_setup(e);
			if (sandbox) {
				gse::activate_scene(e.world(), sandbox->id());
			}
			e.add_system<gs::locomotion::smoke_test>(smoke);
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

auto main(int argc, char** argv) -> int {
	const auto cfg = gse::parse_args<gs::startup::config>(argc, argv);
	if (cfg.locomotion_smoke) {
		gs::startup::run_locomotion_smoke(cfg.smoke);
	}
	else {
		gs::startup::run_game(cfg.engine);
	}
	return 0;
}
