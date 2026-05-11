export module gse.server;

import :server;

import std;
import gse;

export namespace gse {
	template <typename... Components>
	struct server_system {
		struct state {
			std::optional<server<Components...>> srv;
			gse::world* world_ptr = nullptr;
		};

		static auto run(
			run_context& ctx,
			state& s,
			const actions::system::state& actions_s
		) -> async::task<>;
	};

	template <typename Pack>
	using server_system_for = typename Pack::template apply<server_system>;

	template <typename ServerSystem>
	struct server_app_system {
		struct state {
			std::uint32_t tick_count = 0;
			interval_timer<> timer{ seconds(5.f) };
		};

		static auto run(
			run_context& ctx,
			state& s,
			const input::system::state& input_s,
			const typename ServerSystem::state& srv
		) -> async::task<>;
	};

	template <typename ServerSystem>
	auto server_app_setup(
		engine& e
	) -> void;
}

template <typename... Components>
auto gse::server_system<Components...>::run(run_context& ctx, state& s, const actions::system::state& actions_s) -> async::task<> {
	while (true) {
		if (s.srv && s.world_ptr) {
			s.srv->update(*s.world_ptr, ctx.channels, actions_s);
		}
		co_await ctx.next_tick();
	}
}

template <typename ServerSystem>
auto gse::server_app_system<ServerSystem>::run(run_context& ctx, state& s, const input::system::state& input_s, const typename ServerSystem::state& srv) -> async::task<> {
	while (true) {
		if (s.timer.tick()) {
			++s.tick_count;
		}

		if (input::system::current_state(input_s).key_pressed(key::escape)) {
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
					.val = s.tick_count,
				});
			},
		});

		co_await ctx.next_tick();
	}
}

template <typename ServerSystem>
auto gse::server_app_setup(engine& e) -> void {
	auto channels = e.make_channel_writer();
	channels.push<ui_focus_request>({ .focus = true });
	e.world().set_networked(true);

	auto& srv_state = e.add_system<ServerSystem>();
	srv_state.srv.emplace(9000);
	srv_state.srv->initialize();
	srv_state.world_ptr = &e.world();

	e.add_system<server_app_system<ServerSystem>>();
}
