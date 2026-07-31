module gs:client_impl;

import std;
import gse;
import gse.system_manifest;

import :client;
import :balance_controller;
import :footstep_planner;
import :gait_scheduler;
import :leg_controller;
import :orbit_camera;
import :piston;
import :player;
import :pose_driver;
import :state_estimator;
import :tumbler;

auto gs::client_system::init(gse::context& ctx) -> gse::async::task<> {
	gse::system_manifest<
		^^gs::player::data, ^^gs::player::attach, ^^gs::player::update,
		^^gs::orbit_camera::data, ^^gs::orbit_camera::attach, ^^gs::orbit_camera::update,
		^^gs::tumbler::data, ^^gs::tumbler::run,
		^^gs::piston::data, ^^gs::piston::run
	>{}.register_with(ctx);
	gse::system_manifest<
		^^gs::locomotion::state_estimator::data, ^^gs::locomotion::state_estimator::run,
		^^gs::locomotion::gait_scheduler::data, ^^gs::locomotion::gait_scheduler::run,
		^^gs::locomotion::footstep_planner::data, ^^gs::locomotion::footstep_planner::run,
		^^gs::locomotion::balance_controller::data, ^^gs::locomotion::balance_controller::run,
		^^gs::locomotion::leg_controller::data, ^^gs::locomotion::leg_controller::run
	>{}.register_with(ctx);
	gse::system_manifest<^^gs::pose_driver::data, ^^gs::pose_driver::run>{}.register_with(ctx);
	gse::register_systems<^^gse::free_camera::system>(ctx);

	ctx.channels.push<gse::network::clear_providers_request>({});
	std::vector seed{
		gse::network::discovery_result{
			.addr =
				gse::network::address{
					.ip = "192.168.1.156",
					.port = 9000,
				},
			.name = "GoonSquad Server",
			.map = "dev_map",
			.players = 0,
			.max_players = 8,
			.build = 1,
		},
		gse::network::discovery_result{
			.addr =
				gse::network::address{
					.ip = "127.0.0.1",
					.port = 9000,
				},
			.name = "Local",
			.map = "dev_map",
			.players = 0,
			.max_players = 8,
			.build = 1,
		},
	};
	ctx.channels.push<gse::network::add_provider_request>({
		.provider = std::shared_ptr<gse::network::wan_directory_provider>(new gse::network::wan_directory_provider(std::move(seed))),
	});
	ctx.channels.push<gse::network::refresh_servers_request>({
		.timeout = gse::milliseconds(200),
	});

	return {};
}
