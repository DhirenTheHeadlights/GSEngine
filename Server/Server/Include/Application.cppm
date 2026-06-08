export module gse.server;

import :server;

import std;
import gse;

export namespace gse {
	template <typename... Components>
	struct server_system {
		struct data {
			std::optional<server<Components...>> srv;
		};

		static auto run(
			run_context& ctx,
			data& d,
			world_system::data& world_d,
			const actions::system::data& actions_d
		) -> async::task<>;
	};

	template <typename Pack>
	using server_system_for = typename Pack::template apply<server_system>;

	template <typename ServerSystem>
	struct server_app_system {
		struct data {
			std::uint32_t tick_count = 0;
			interval_timer<> timer{ seconds(5.f) };
		};

		static auto run(
			run_context& ctx,
			data& d,
			const input::system::data& input_d,
			const typename ServerSystem::data& srv
		) -> async::task<>;
	};

	template <typename ServerSystem>
	auto server_app_setup(
		engine& e
	) -> void;
}

template <typename... Components>
auto gse::server_system<Components...>::run(run_context& ctx, data& d, world_system::data& world_d, const actions::system::data& actions_d) -> async::task<> {
	if (d.srv) {
		d.srv->update(world_d, ctx.registry(), ctx.channels, actions_d);
	}
	return {};
}

template <typename ServerSystem>
auto gse::server_app_system<ServerSystem>::run(run_context& ctx, data& d, const input::system::data& input_d, const typename ServerSystem::data& srv) -> async::task<> {
	if (d.timer.tick()) {
		++d.tick_count;
	}

	if (input::system::current_state(input_d).key_pressed(key::escape)) {
		shutdown();
	}

	ctx.channels.push<gui::menu_content>({
		.menu = "Server Control",
		.build = [&](gui::builder& ui) {
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

template <typename ServerSystem>
auto gse::server_app_setup(engine& e) -> void {
	auto channels = e.make_channel_writer();
	channels.push<ui_focus_request>({
		.focus = true
	});
	e.world().networked = true;

	auto& srv_data = e.add_system<ServerSystem>();
	srv_data.srv.emplace(9000);
	srv_data.srv->initialize();

	e.add_system<server_app_system<ServerSystem>>();
}
