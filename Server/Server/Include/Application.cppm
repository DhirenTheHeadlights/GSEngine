export module gse.server;

import :server;
import :input_source;

import std;
import gse;

export namespace gse {
	struct server_state {
		std::optional<server> srv;
	};

	struct server_system {
		static auto initialize(
			init_context& phase,
			server_state& s
		) -> void;

		static auto update(
			update_context& ctx,
			server_state& s
		) -> async::task<>;
	};

	struct server_app_state {
		std::uint32_t tick_count = 0;
		interval_timer<> timer{ seconds(5.f) };
	};

	struct server_app_system {
		static auto initialize(
			init_context& phase,
			server_app_state& s
		) -> void;

		static auto update(
			update_context& ctx,
			server_app_state& s,
			const server_state& srv
		) -> async::task<>;
	};

	auto server_app_setup(
		engine& e
	) -> void;
}

auto gse::server_system::initialize(init_context&, server_state&) -> void {}

auto gse::server_system::update(update_context&, server_state& s) -> async::task<> {
	if (s.srv) {
		s.srv->update();
	}
	co_return;
}

auto gse::server_app_system::initialize(init_context&, server_app_state&) -> void {}

auto gse::server_app_system::update(update_context&, server_app_state& s, const server_state& srv) -> async::task<> {
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

	co_return;
}

auto gse::server_app_setup(engine& e) -> void {
	set_ui_focus(true);
	set_networked(true);

	auto& srv_state = e.add_system<server_system, server_state>();
	srv_state.srv.emplace(9000);
	srv_state.srv->initialize();

	set_input_sampler(make_server_input_sampler(&srv_state.srv->clients()));

	e.add_system<server_app_system, server_app_state>();
}
