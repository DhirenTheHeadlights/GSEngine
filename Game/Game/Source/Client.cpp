module gs:client_impl;

import std;
import gse;

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
	ctx.add_system<gs::player::system>();
	ctx.add_system<gs::orbit_camera::system>();
	ctx.add_system<gs::tumbler::system>();
	ctx.add_system<gs::piston::system>();
	ctx.add_system<gs::locomotion::state_estimator>();
	ctx.add_system<gs::locomotion::gait_scheduler>();
	ctx.add_system<gs::locomotion::footstep_planner>();
	ctx.add_system<gs::locomotion::balance_controller>();
	ctx.add_system<gs::locomotion::leg_controller>();
	ctx.add_system<gs::pose_driver::system>();
	ctx.add_system<gse::free_camera::system>();

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
