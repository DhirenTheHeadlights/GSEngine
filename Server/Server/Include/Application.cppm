export module gse.server;

import :server;
import :input_source;

import std;
import gse;

export namespace gse {
	struct server_system {
		struct state {
			std::optional<server> srv;
		};

		static auto run(
			run_context& ctx,
			state& s
		) -> async::task<>;
	};

	struct server_app_system {
		struct state {
			std::uint32_t tick_count = 0;
			interval_timer<> timer{ seconds(5.f) };
		};

		static auto run(
			run_context& ctx,
			state& s,
			const server_system::state& srv
		) -> async::task<>;
	};

	auto server_app_setup(
		engine& e
	) -> void;
}

auto gse::server_system::run(run_context& ctx, state& s) -> async::task<> {
	while (true) {
		if (s.srv) {
			s.srv->update();
		}
		co_await ctx.next_tick();
	}
}

auto gse::server_app_system::run(run_context& ctx, state& s, const server_system::state& srv) -> async::task<> {
	while (true) {
		if (s.timer.tick()) {
			++s.tick_count;
		}

		if (keyboard::pressed(key::escape)) {
			shutdown();
		}

		gui::panel("Server Control", [&](gui::builder& ui) {
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
		});

		co_await ctx.next_tick();
	}
}

auto gse::server_app_setup(engine& e) -> void {
	set_ui_focus(true);
	set_networked(true);

	auto& srv_state = e.add_system<server_system>();
	srv_state.srv.emplace(9000);
	srv_state.srv->initialize();

	set_input_sampler(make_server_input_sampler(&srv_state.srv->clients()));

	e.add_system<server_app_system>();
}
