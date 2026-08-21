export module gse.server;

export import :server;

import std;
import gse;
import gse.system_manifest;

export namespace gse::server {
	template <typename MessagePack, typename... Components>
	struct [[= gse::system_state<"Server">{}]] data {
		[[= gse::shared]] std::optional<host<MessagePack, Components...>> srv;
	};

	template <typename MessagePack, typename... Components>
	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		data<MessagePack, Components...>& d,
		const network::config& net_cfg
	) -> async::task<>;

	template <typename MessagePack, typename... Components>
	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data<MessagePack, Components...>& d,
		channel_write<activate_scene_request> scene_out,
		network::inbound_channel_t<MessagePack> messages_out,
		network::outbound_channel_t<MessagePack> messages_in,
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
	template <typename MessagePack, typename... Components>
	auto server_app_setup(
		engine& e,
		type_pack<Components...> components = {},
		MessagePack messages = {}
	) -> void;
}

template <typename MessagePack, typename... Components>
auto gse::server::init(context& ctx, data<MessagePack, Components...>& d, const network::config& net_cfg) -> async::task<> {
	d.srv.emplace(net_cfg);
	d.srv->initialize();
	return {};
}

template <typename MessagePack, typename... Components>
auto gse::server::run(context& ctx, data<MessagePack, Components...>& d, const channel_write<activate_scene_request> scene_out, network::inbound_channel_t<MessagePack> messages_out, network::outbound_channel_t<MessagePack> messages_in, const shared_view<world_system::data> world_d, const shared_view<actions::data> actions_d, structural<player_controller> controller_auth, entities ents, write<Components>... comps) -> async::task<> {
	if (!d.srv) {
		return {};
	}

	network::drain_outbound<MessagePack>(
		messages_in,
		[&d](const auto& msg, const std::optional<network::address>& to, const bool reliable) {
			if (to) {
				d.srv->send(msg, *to, reliable);
			}
			else {
				d.srv->broadcast(msg, reliable);
			}
		}
	);

	d.srv->update(world_d, controller_auth, ents, scene_out, messages_out, actions_d, comps...);

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

template <typename MessagePack, typename... Components>
auto gse::server_app_setup(engine& e, type_pack<Components...>, MessagePack) -> void {
	auto channels = e.make_channel_writer();
	channels.push<ui_focus_request>({
		.focus = true
	});
	e.world().networked = true;

	gse::system_manifest<
		^^gse::server::data<MessagePack, Components...>,
		^^gse::server::init<MessagePack, Components...>,
		^^gse::server::run<MessagePack, Components...>
	>{}.register_with(e);

	gse::system_manifest<
		^^gse::server_app::data,
		^^gse::server_app::run<gse::server::data<MessagePack, Components...>>
	>{}.register_with(e);
}
