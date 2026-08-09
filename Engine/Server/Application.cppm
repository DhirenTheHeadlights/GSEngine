export module gse.server;

export import :server;

import std;
import gse;
import gse.system_manifest;

export namespace gse::server {
	template <typename... Components>
	struct [[= gse::system_state<"Server">{}]] data {
		[[= gse::shared]] std::optional<host<Components...>> srv;
	};

	template <typename... Components>
	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		data<Components...>& d
	) -> async::task<>;

	template <typename... Components>
	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data<Components...>& d,
		channel_write<activate_scene_request> scene_out,
		shared_view<world_system::data> world_d,
		shared_view<actions::data> actions_d,
		structural<player_controller> controller_auth,
		entities ents,
		write<Components>... comps
	) -> async::task<>;
}

export namespace gse::server_app {
	struct [[= gse::system_state<"ServerApp">{}]] data {
		std::uint32_t tick_count = 0;
		interval_timer<> timer{ seconds(5.f) };
	};

	template <typename ServerData>
	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_write<gui::menu_content> ui_out,
		shared_view<input::data> input_d,
		shared_view<ServerData> srv
	) -> async::task<>;
}

export namespace gse {
	template <typename... Components>
	auto server_app_setup(
		engine& e,
		type_pack<Components...> components = {}
	) -> void;

	auto server_app_setup(
		engine& e
	) -> void;
}

template <typename... Components>
auto gse::server::init(context& ctx, data<Components...>& d) -> async::task<> {
	d.srv.emplace(9000);
	d.srv->initialize();
	return {};
}

template <typename... Components>
auto gse::server::run(context& ctx, data<Components...>& d, const channel_write<activate_scene_request> scene_out, const shared_view<world_system::data> world_d, const shared_view<actions::data> actions_d, structural<player_controller> controller_auth, entities ents, write<Components>... comps) -> async::task<> {
	if (d.srv) {
		d.srv->update(world_d, controller_auth, ents, scene_out, actions_d, comps...);
	}
	return {};
}

template <typename ServerData>
auto gse::server_app::run(context& ctx, data& d, const channel_write<gui::menu_content> ui_out, const shared_view<input::data> input_d, const shared_view<ServerData> srv) -> async::task<> {
	if (d.timer.tick()) {
		++d.tick_count;
	}

	if (input::current_state(input_d).key_pressed(key::escape)) {
		shutdown();
	}

	ui_out.push<gui::menu_content>({
		.menu = "Server Control",
		.build = [&d, srv](gui::builder& ui) {
			ui.draw<gui::text>({
				.content = "This is a simple server application.",
			});

			if (!srv.srv) {
				return;
			}

			ui.draw<gui::value<std::uint32_t>>({
				.name = "Peers",
				.val = static_cast<std::uint32_t>(srv.srv->peers().size()),
			});
			ui.draw<gui::value<std::uint32_t>>({
				.name = "Clients",
				.val = static_cast<std::uint32_t>(srv.srv->clients().size()),
			});
			if (const auto h = srv.srv->host_entity()) {
				ui.draw<gui::text>({
					.content = std::format("Host entity: {}", *h),
				});
			}
			else {
				ui.draw<gui::text>({
					.content = "Host entity: <none>",
				});
			}
			for (const auto& [ip, port] : srv.srv->peers() | std::views::keys) {
				ui.draw<gui::text>({
					.content = std::format("Peer: {}:{}", ip, port),
				});
			}
			ui.draw<gui::value<std::uint32_t>>({
				.name = "Ticks",
				.val = d.tick_count,
			});
		},
	});

	return {};
}

template <typename... Components>
auto gse::server_app_setup(engine& e, type_pack<Components...>) -> void {
	auto channels = e.make_channel_writer();
	channels.push<ui_focus_request>({
		.focus = true
	});
	e.world().networked = true;

	gse::system_manifest<
		^^gse::server::data<Components...>,
		^^gse::server::init<Components...>,
		^^gse::server::run<Components...>
	>{}.register_with(e);

	gse::system_manifest<
		^^gse::server_app::data,
		^^gse::server_app::run<gse::server::data<Components...>>
	>{}.register_with(e);
}

auto gse::server_app_setup(engine& e) -> void {
	server_app_setup(e, engine_networked_components{});
}