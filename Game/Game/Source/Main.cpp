import std;

import gse;
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

	template <typename... Components>
	consteval auto network_run_hook(gse::type_pack<Components...>) -> std::meta::info {
		return ^^gse::network::run<Components...>;
	}
}

auto gs::startup::run_game(const gse::engine_config& engine) -> void {
	gse::start(
		[](gse::engine& e) -> void {
			constexpr auto network_run = network_run_hook(gs::networked_components{});
			static_assert(gse::meta::find_system_hook_anno(network_run) != std::meta::info{}, "network run hook annotation not visible on the templated instantiation");
			gse::system_manifest<^^gse::network::data, network_run, ^^gse::network::shutdown>{}.register_with(e);
			gse::system_manifest<^^gs::crosshair::data, ^^gs::crosshair::draw_settings>{}.register_with(e);
			gse::register_systems<^^gs::client_system>(e);
			gse::system_manifest<^^gs::client_ui::data, ^^gs::client_ui::run, ^^gs::pause_menu::data, ^^gs::pause_menu::run>{}.register_with(e);
			gse::system_manifest<^^gs::dev_spawn::data, ^^gs::dev_spawn::run>{}.register_with(e);
			gse::register_systems<^^gse::gui::popout_system>(e);
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

auto main(int argc, char** argv) -> int {
	const auto cfg = gse::parse_args<gs::startup::config>(argc, argv);
	if (cfg.locomotion_selftest) {
		return gs::locomotion::locomotion_selftest() ? 0 : 1;
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
