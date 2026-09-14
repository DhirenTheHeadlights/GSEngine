export module gse.server:server;

import std;

import gse.assert;
import gse.network;
import gse.physics;
import gse.graphics;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.log;
import gse.ecs;
import gse.os;
import gse.win32;
import gse.assets;
import gse.gpu;
import gse.runtime;

export namespace gse::server {
	struct client_data {
		static constexpr std::size_t max_pending_inputs = 4;
		static constexpr std::size_t max_stepped_history = 16;

		struct stepped_point {
			std::uint64_t step = 0;
			std::uint32_t sequence = 0;
		};

		id controller_id;
		actions::state latest_input;
		std::uint32_t last_input_sequence = 0;
		std::uint32_t applied_sequence = 0;
		std::uint32_t stepped_sequence = 0;
		std::vector<network::input_frame> pending_inputs;
		std::vector<stepped_point> stepped_history;
	};

	template <typename MessagePack, typename... Components>
	class host {
	public:
		explicit host(
			network::config cfg
		);

		~host();

		auto initialize() -> void;

		auto apply_catalog(
			const world_system::scene_catalog& catalog
		) -> void;

		auto update(
			const structural<player_controller>& controller_auth,
			const structural<player_input>& input_auth,
			const entities& ents,
			channel_write<activate_scene_request> channels,
			const network::inbound_channel_t<MessagePack>& messages_out,
			shared_view<actions::data> actions_s,
			shared_view<physics::data> phys_s,
			write<player_input>& inputs,
			write<Components>&... comps
		) -> void;

		template <network::is_network_message T>
		auto send(
			const T& msg,
			const network::address& to,
			bool reliable = false
		) -> void;

		template <network::is_network_message T>
		auto send_reliable(
			const T& msg,
			const network::address& to
		) -> void;

		template <network::is_network_message T>
		auto broadcast(
			const T& msg,
			bool reliable = false
		) -> void;

		auto peers() const -> const std::unordered_map<network::address, network::remote_peer>&;

		auto clients() const -> const std::unordered_map<network::address, client_data>&;

		auto host_entity() const -> std::optional<id>;

	private:
		auto accept_connection(
			const structural<player_controller>& controller_auth,
			const structural<player_input>& input_auth,
			const entities& ents,
			const network::address& addr
		) -> void;

		auto drop_client(
			write<player_controller>& controllers,
			const entities& ents,
			const network::address& addr
		) -> void;

		auto draw_dashboard(
			const shared_view<physics::data>& phys_s
		) -> void;

		network::config m_config;
		network::endpoint m_endpoint;
		std::vector<id> m_scene_ids;
		std::vector<trigger> m_triggers;
		std::optional<id> m_active_scene;
		std::optional<id> m_requested_scene;
		std::unordered_map<network::address, client_data> m_clients;
		std::unordered_set<network::address> m_pending_snapshots;
		std::optional<id> m_host_entity;
		std::optional<network::address> m_host_addr;
		interval_timer<> m_health_timer{ seconds(2.f) };
		std::uint32_t m_health_frames = 0;
		std::uint64_t m_health_last_step = 0;
		time m_health_longest_frame;
		std::uint32_t m_window_frames = 0;
		std::uint64_t m_window_steps = 0;
		time m_window_longest_frame;
		clock m_uptime;
		std::uint16_t m_port = 0;
		bool m_dashboard_started = false;
	};
}

template <typename MessagePack, typename... Components>
gse::server::host<MessagePack, Components...>::host(network::config cfg) : m_config(std::move(cfg)) {
}

template <typename MessagePack, typename... Components>
gse::server::host<MessagePack, Components...>::~host() {
	if (m_dashboard_started) {
		std::print("\x1b[?25h\x1b[0m\n");
		std::cout.flush();
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::initialize() -> void {
	if (!m_endpoint.bind(network::address{
		.ip = "0.0.0.0",
		.port = m_config.listen_port
		})) {
		log::println(log::level::error, log::category::network, "Server: Failed to bind socket to port {}", m_config.listen_port);
		return;
	}

	if (const auto local = m_endpoint.local_address()) {
		m_port = local->port;
		log::println(log::category::network, "Server: Listening on port {}", local->port);
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::draw_dashboard(const shared_view<physics::data>& phys_s) -> void {
	if (!m_dashboard_started) {
		const auto out = win32::GetStdHandle(win32::std_output_handle);
		win32::DWORD mode = 0;
		if (win32::GetConsoleMode(out, &mode)) {
			win32::SetConsoleMode(out, mode | win32::enable_virtual_terminal_processing);
		}
		const auto in = win32::GetStdHandle(win32::std_input_handle);
		win32::DWORD in_mode = 0;
		if (win32::GetConsoleMode(in, &in_mode)) {
			win32::SetConsoleMode(in, (in_mode & ~win32::enable_quick_edit_mode) | win32::enable_extended_flags);
		}
		std::print("\x1b[?25l\x1b[2J");
		m_dashboard_started = true;
	}

	const auto up = static_cast<std::uint64_t>(m_uptime.elapsed<double>().as<seconds>());
	std::string frame;
	frame += "\x1b[H";
	frame += std::format("\x1b[1mGSEngine dedicated server\x1b[0m   up {:02}:{:02}:{:02}   port {}\x1b[K\n", up / 3600, (up / 60) % 60, up % 60, m_port);
	if (m_active_scene) {
		frame += std::format("scene {}   clients {}/{}   physics step {}\x1b[K\n", *m_active_scene, m_clients.size(), m_config.max_players, phys_s.step_index);
	}
	else {
		frame += std::format("scene <none>   clients {}/{}   physics step {}\x1b[K\n", m_clients.size(), m_config.max_players, phys_s.step_index);
	}
	frame += std::format("last 2 s: {} frames, {} physics steps, longest frame {:.1f} ms   socket drops {}\x1b[K\n", m_window_frames, m_window_steps, m_window_longest_frame.as<milliseconds>(), m_endpoint.dropped());
	frame += "\x1b[K\n";
	frame += std::format("\x1b[1m{:<22} {:>12} {:>6} {:>10} {:>10}\x1b[0m\x1b[K\n", "client", "controller", "queue", "last seq", "applied");
	if (m_clients.empty()) {
		frame += "  (no clients connected)\x1b[K\n";
	}
	for (const auto& [addr, cd] : m_clients) {
		const bool is_host = m_host_addr == addr;
		frame += std::format(
			"{:<22} {:>12} {:>6} {:>10} {:>10}{}\x1b[K\n",
			std::format("{}:{}", addr.ip, addr.port),
			cd.controller_id.number(),
			cd.pending_inputs.size(),
			cd.last_input_sequence,
			cd.applied_sequence,
			is_host ? "  host" : ""
		);
	}
	frame += "\x1b[K\n";
	frame += "\x1b[90mVerbose output is in the log file. Ctrl+C to stop.\x1b[0m\x1b[K\n";
	frame += "\x1b[J";
	std::print("{}", frame);
	std::cout.flush();
}

template <typename MessagePack, typename... Components>
template <gse::network::is_network_message T>
auto gse::server::host<MessagePack, Components...>::send_reliable(const T& msg, const network::address& to) -> void {
	send(msg, to, true);
}

template <typename MessagePack, typename... Components>
template <gse::network::is_network_message T>
auto gse::server::host<MessagePack, Components...>::send(const T& msg, const network::address& to, const bool reliable) -> void {
	m_endpoint.send(msg, to, reliable);
}

template <typename MessagePack, typename... Components>
template <gse::network::is_network_message T>
auto gse::server::host<MessagePack, Components...>::broadcast(const T& msg, const bool reliable) -> void {
	for (const auto& addr : m_clients | std::views::keys) {
		m_endpoint.send(msg, addr, reliable);
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::apply_catalog(const world_system::scene_catalog& catalog) -> void {
	m_scene_ids = catalog.scene_ids;
	m_triggers = catalog.triggers;
	m_active_scene = catalog.active_scene != nullptr
		? std::optional(catalog.active_scene->id())
		: std::nullopt;
	if (m_active_scene.has_value()) {
		m_requested_scene.reset();
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::update(const structural<player_controller>& controller_auth, const structural<player_input>& input_auth, const entities& ents, const channel_write<activate_scene_request> channels, const network::inbound_channel_t<MessagePack>& messages_out, const shared_view<actions::data> actions_s, const shared_view<physics::data> phys_s, write<player_input>& inputs, write<Components>&... comps) -> void {
	auto& controllers = std::get<write<player_controller>&>(std::tie(comps...));
	const bool has_active_scene = m_active_scene.has_value();

	++m_health_frames;
	m_health_longest_frame = std::max(m_health_longest_frame, system_clock::dt<time>());
	if (m_health_timer.tick()) {
		std::size_t deepest_queue = 0;
		for (const auto& cd : m_clients | std::views::values) {
			deepest_queue = std::max(deepest_queue, cd.pending_inputs.size());
		}
		log::println(
			log::category::network,
			"server: {} frames and {} physics steps in the last window, longest frame {}, {} client(s), deepest input queue {}",
			m_health_frames,
			phys_s.step_index - m_health_last_step,
			m_health_longest_frame,
			m_clients.size(),
			deepest_queue
		);
		m_window_frames = m_health_frames;
		m_window_steps = phys_s.step_index - m_health_last_step;
		m_window_longest_frame = m_health_longest_frame;
		m_health_frames = 0;
		m_health_last_step = phys_s.step_index;
		m_health_longest_frame = {};
		if (m_config.dashboard) {
			draw_dashboard(phys_s);
		}
	}

	if (!has_active_scene && !m_scene_ids.empty() && m_requested_scene != m_scene_ids.front()) {
		m_requested_scene = m_scene_ids.front();
		channels.push<activate_scene_request>({
			.scene_id = *m_requested_scene,
		});
	}

	m_endpoint.poll([&](network::inbound_message& msg) {
		network::read_bitstream stream(msg.payload);

		if (!m_endpoint.find_peer(msg.from)) {
			if (network::try_decode<network::server_info_request>(stream, msg.id, [&](const auto&) {
				m_endpoint.send(
					network::server_info_response{
						.players = static_cast<std::uint8_t>(m_clients.size()),
						.max_players = m_config.max_players,
					},
					msg.from
					);
			})) {
				return;
			}

			network::try_decode<network::connection_request>(
				stream,
				msg.id,
				[&](const auto&) {
					if (!has_active_scene) {
						return;
					}

					const std::uint8_t max_players = m_config.max_players;
					if (m_clients.size() >= max_players) {
						log::println(
							log::level::warning,
							log::category::network,
							"Client [{}:{}] failed to connect (server full: {}/{})",
							msg.from.ip,
							msg.from.port,
							m_clients.size(),
							max_players
						);
						return;
					}

					m_endpoint.ensure_peer(msg.from);
					accept_connection(controller_auth, input_auth, ents, msg.from);
					log::println(
						log::category::network,
						"Client [{}:{}] connected ({}/{})",
						msg.from.ip,
						msg.from.port,
						m_clients.size(),
						max_players
					);
				}
			);

			return;
		}

		if (network::try_decode<network::connection_request>(stream, msg.id, [&](const auto&) {
			if (m_clients.contains(msg.from)) {
				log::println(log::category::network, "Client [{}:{}] reconnecting", msg.from.ip, msg.from.port);
				drop_client(controllers, ents, msg.from);
			}

			accept_connection(controller_auth, input_auth, ents, msg.from);
		})) {
			return;
		}

		if (network::try_decode<network::disconnect_notice>(stream, msg.id, [&](const auto&) {
			drop_client(controllers, ents, msg.from);
			m_endpoint.remove_peer(msg.from);
			log::println(log::category::network, "Client [{}:{}] disconnected ({}/{})", msg.from.ip, msg.from.port, m_clients.size(), m_config.max_players);
		})) {
			return;
		}

		const bool handled = network::try_decode<network::ping>(
			stream,
			msg.id,
			[&](const auto& m) {
				send(
					network::pong{
						.sequence = m.sequence,
					},
					msg.from
				);
			}
		) ||
			network::try_decode<network::server_info_request>(
				stream,
				msg.id,
				[&](const auto&) {
					send(
						network::server_info_response{
							.players = static_cast<std::uint8_t>(m_clients.size()),
							.max_players = m_config.max_players,
						},
						msg.from
					);
				}
			) ||
			network::try_decode<network::input_frame>(
				stream,
				msg.id,
				[&](const auto& m) {
					const auto client_it = m_clients.find(msg.from);
					if (client_it == m_clients.end()) {
						return;
					}

					auto& cd = client_it->second;
					if (m.input_sequence > cd.last_input_sequence) {
						cd.last_input_sequence = m.input_sequence;
						network::apply_input_frame(cd.latest_input, m);
					}
					if (m.input_sequence <= cd.applied_sequence) {
						return;
					}

					const auto at = std::ranges::lower_bound(cd.pending_inputs, m.input_sequence, std::ranges::less{}, &network::input_frame::input_sequence);
					if (at != cd.pending_inputs.end() && at->input_sequence == m.input_sequence) {
						return;
					}
					cd.pending_inputs.insert(at, m);
					if (cd.pending_inputs.size() > client_data::max_pending_inputs) {
						cd.pending_inputs.erase(cd.pending_inputs.begin());
					}
				}
			);

		if (!handled) {
			const auto client_it = m_clients.find(msg.from);
			network::route_inbound<MessagePack>(stream, msg, messages_out, client_it != m_clients.end() ? client_it->second.controller_id : id{});
		}
	});

	const auto client_timeout = milliseconds(std::uint64_t{ 10000 });
	for (const auto& addr : m_endpoint.silent_peers(client_timeout)) {
		drop_client(controllers, ents, addr);
		m_endpoint.remove_peer(addr);
		log::println(log::level::warning, log::category::network, "Client [{}:{}] timed out ({}/{})", addr.ip, addr.port, m_clients.size(), m_config.max_players);
	}

	std::optional<id> scene_requested_id;

	if (has_active_scene) {
		for (const auto& [scene_id, condition] : m_triggers) {
			for (const auto& cd : m_clients | std::views::values) {
				const auto* pc = controllers.find(cd.controller_id);
				const auto controlled_id = pc ? pc->controlled_entity_id : id{};

				evaluation_context ctx{
					.client_id = controlled_id,
					.input = &cd.latest_input,
				};
				if (condition(ctx)) {
					scene_requested_id = scene_id;
				}
			}
		}
	}

	if (scene_requested_id) {
		m_requested_scene = *scene_requested_id;
		channels.push<activate_scene_request>({
			.scene_id = *scene_requested_id,
		});

		const network::notify_scene_change msg{
			.scene_id = *scene_requested_id,
		};

		for (const auto& addr : m_clients | std::views::keys) {
			send_reliable(msg, addr);
		}
	}

	m_endpoint.resend_reliable();

	if (has_active_scene) {
		auto send_all = [this](const auto& msg, const network::address& to) {
			this->send(msg, to);
		};

		if (!m_pending_snapshots.empty()) {
			for (const auto& addr : m_pending_snapshots) {
				network::replicate_snapshot_to(send_all, addr, comps...);
			}
			m_pending_snapshots.clear();
		}

		network::replicate_deltas(send_all, m_endpoint.peers(), comps...);

		const int stepped = phys_s.interpolation.steps;

		for (auto& cd : m_clients | std::views::values) {
			if (stepped > 0) {
				cd.stepped_sequence = cd.applied_sequence;
				cd.stepped_history.push_back({
					.step = phys_s.step_index,
					.sequence = cd.stepped_sequence,
				});
				if (cd.stepped_history.size() > client_data::max_stepped_history) {
					cd.stepped_history.erase(cd.stepped_history.begin());
				}
			}

			auto* input = inputs.find(cd.controller_id);
			if (input) {
				for (const auto& point : cd.stepped_history) {
					if (point.step > phys_s.observed_step) {
						break;
					}
					input->acked_sequence = point.sequence;
					input->acked_step = point.step;
				}
			}
			for (int i = 0; i < stepped && input && !cd.pending_inputs.empty(); ++i) {
				const auto& frame = cd.pending_inputs.front();
				network::apply_input_frame(input->state, frame);
				input->sequence = frame.input_sequence;
				cd.applied_sequence = frame.input_sequence;
				cd.pending_inputs.erase(cd.pending_inputs.begin());
			}
		}
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::peers() const -> const std::unordered_map<network::address, network::remote_peer>& {
	return m_endpoint.peers();
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::clients() const -> const std::unordered_map<network::address, client_data>& {
	return m_clients;
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::host_entity() const -> std::optional<id> {
	return m_host_entity;
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::drop_client(write<player_controller>& controllers, const entities& ents, const network::address& addr) -> void {
	const auto it = m_clients.find(addr);
	if (it == m_clients.end()) {
		return;
	}

	if (m_active_scene.has_value()) {
		if (const auto* pc = controllers.find(it->second.controller_id)) {
			if (pc->controlled_entity_id.exists()) {
				ents.remove(pc->controlled_entity_id);
			}
		}
		ents.remove(it->second.controller_id);
	}

	m_clients.erase(it);
	m_pending_snapshots.erase(addr);

	if (m_host_addr == addr) {
		m_host_addr.reset();
		m_host_entity.reset();
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::accept_connection(const structural<player_controller>& controller_auth, const structural<player_input>& input_auth, const entities& ents, const network::address& addr) -> void {
	if (!m_active_scene.has_value()) {
		return;
	}

	const auto controller_name = std::format("PlayerController_{}:{}", addr.ip, addr.port);
	const auto controller_id = generate_id(controller_name);
	ents.ensure_active(controller_id);
	controller_auth.add(controller_id);
	input_auth.add(controller_id);
	m_clients.emplace(
		addr,
		client_data{
			.controller_id = controller_id,
		}
	);

	if (!m_host_entity.has_value()) {
		m_host_entity = controller_id;
		m_host_addr = addr;
	}
	else if (m_host_addr == addr) {
		m_host_entity = controller_id;
	}

	send_reliable(
		network::connection_accepted{
			.controller_id = controller_id,
		},
		addr
	);

	send_reliable(
		network::notify_scene_change{
			.scene_id = *m_active_scene,
		},
		addr
	);

	m_pending_snapshots.insert(addr);
}
