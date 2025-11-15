export module gse.server:server;

import std;

import gse.assert;
import gse.network;
import gse.utility;
import gse.platform;
import gse.runtime;

template <>
struct std::hash<gse::network::address> {
	auto operator()(const gse::network::address& addr) const noexcept -> std::size_t {
		const std::size_t h1 = std::hash<std::string>{}(addr.ip);
		const std::size_t h2 = std::hash<std::uint16_t>{}(addr.port);
		return h1 ^ (h2 << 1);
	}
};

export namespace gse {
	struct client_data {
		id entity_id;
		actions::state latest_input;
		std::uint32_t last_input_sequence = 0;
		bool have_seq = false;
	};

	class server {
	public:
		explicit server(
			std::uint16_t port
		);

		auto update(
			world& world
		) -> void;

		template <typename T>
		auto send(
			const T& msg,
			const network::address& to
		) -> void;

		auto peers(
		) const -> const std::unordered_map<network::address, network::remote_peer>&;

		auto clients(
		) const -> const std::unordered_map<network::address, client_data>&;

		auto host_entity(
		) const -> std::optional<id>;

		auto host_address(
		) const -> std::optional<network::address>;
	private:
		static auto process_header(
			network::bitstream& stream,
			network::remote_peer& peer
		) -> packet_header;

		network::udp_socket m_socket;
		std::unordered_map<network::address, network::remote_peer> m_peers;
		std::unordered_map<network::address, client_data> m_clients;
		std::optional<id> m_host_entity;
		std::optional<network::address> m_host_addr;
	};
}

gse::server::server(const std::uint16_t port) {
	m_socket.bind(network::address{
		.ip = "0.0.0.0",
		.port = port
		});

	std::println("Server started on port {}", port);
}

template <typename T>
auto gse::server::send(const T& msg, const network::address& to) -> void {
	const auto it = m_peers.find(to);
	if (it == m_peers.end()) {
		return;
	}

	auto& peer = it->second;

	std::array<std::byte, max_packet_size> buffer;
	const packet_header header{
		.sequence = ++peer.sequence(),
		.ack = peer.remote_ack_sequence(),
		.ack_bits = peer.remote_ack_bitfield()
	};

	network::bitstream stream(buffer);
	stream.write(header);
	network::write(stream, msg);

	const network::packet packet_to_send{
		.data = reinterpret_cast<std::uint8_t*>(buffer.data()),
		.size = stream.bytes_written()
	};

	assert(
		m_socket.send_data(packet_to_send, to) != network::socket_state::error,
		std::source_location::current(),
		"Failed to send packet from server."
	);
}

auto gse::server::update(world& world) -> void {
	std::array<std::byte, max_packet_size> buffer;

	while (auto received = m_socket.receive_data(buffer)) {
		const std::span data(buffer.data(), received->bytes_read);
		network::bitstream stream(data);

		const auto it = m_peers.find(received->from);
		if (it == m_peers.end()) {
			const auto message_id = network::message_id(stream);

			bool handled = false;
			handled |= network::try_decode<network::connection_request>(stream, message_id, [&](const auto&) {
				std::println("Server: ConnectionRequest from {}:{}", received->from.ip, received->from.port);

				auto [ins_it, _] = m_peers.emplace(received->from, network::remote_peer(received->from));

				if (auto* scene = world.current_scene()) {
					auto entity_id = scene->registry().create("Player");
					scene->registry().activate(entity_id);
					m_clients[received->from] = client_data{ .entity_id = entity_id };
					if (!m_host_entity.has_value()) {
						m_host_entity = entity_id;
						m_host_addr = received->from;
						std::println("Server: Host set to {}:{} (entity {})", received->from.ip, received->from.port, entity_id);
					}
					std::println("Server: Created entity {} for {}:{}", entity_id, received->from.ip, received->from.port);
				}

				send(network::connection_accepted{}, received->from);

				if (const auto* active = world.current_scene()) {
					const auto tag_sv = std::string_view(active->id().tag());
					network::notify_scene_change msg{};
					std::ranges::fill(msg.scene_id, '\0');
					const auto n = std::min(tag_sv.size(), msg.scene_id.size() - 1);
					std::ranges::copy_n(tag_sv.data(), n, msg.scene_id.begin());
					send(msg, received->from);
				}
				});

			if (!handled) {
				std::println("Server: dropped pre-handshake msg {} from {}:{}", message_id, received->from.ip, received->from.port);
			}

			continue;
		}

		auto& peer = it->second;
		process_header(stream, peer);

		const auto id = network::message_id(stream);

		network::match_message(stream, id)
			.if_is([&](const network::ping& m) {
				send(network::pong{ .sequence = m.sequence }, received->from);
			})
			.else_if_is([&](const network::pong&) {
				std::println("Server: Received Pong from {}:{}", received->from.ip, received->from.port);
			})
			.else_if_is([&](const network::input_frame_header& fh) {
				const std::size_t wc = fh.action_word_count;

				std::vector<std::uint64_t> pressed(wc), released(wc);

				for (std::size_t i = 0; i < wc; ++i) {
					pressed[i] = stream.read<std::uint64_t>();
				}

				for (std::size_t i = 0; i < wc; ++i) {
					released[i] = stream.read<std::uint64_t>();
				}

				std::vector<network::axes1_pair> a1(fh.axes1_count);
				for (auto& p : a1) {
					p = stream.read<network::axes1_pair>();
				}

				std::vector<network::axes2_pair> a2(fh.axes2_count);
				for (auto& p : a2) {
					p = stream.read<network::axes2_pair>();
				}

				auto& cd = m_clients[received->from];
				if (fh.input_sequence <= cd.last_input_sequence) {
					return;
				}

				cd.last_input_sequence = fh.input_sequence;

				cd.latest_input.begin_frame();
				cd.latest_input.ensure_capacity();
				cd.latest_input.load_transients(pressed, released);

				for (auto& [id, value] : a1) {
					cd.latest_input.set_axis1(id, value);
				}

				for (auto& [id, x, y] : a2) {
					cd.latest_input.set_axis2(id, actions::axis{ x, y });
				}

				auto log_pressed_released = [&](const actions::state& st) {
					auto dump = [&](const std::span<const actions::word> words, std::string_view kind) {
						for (std::size_t wi = 0; wi < words.size(); ++wi) {
							auto w = words[wi];
							while (w) {
								const unsigned long b = std::countr_zero(w);
								const std::size_t idx = wi * 64 + b;
								std::println("Server: {} action {} from {}:{}",
									kind, idx, received->from.ip, received->from.port);
								w &= (w - 1); // clear lowest set bit
							}
						}
						};

					dump(st.pressed_mask().words(), "pressed");
					dump(st.released_mask().words(), "released");
				};

				log_pressed_released(cd.latest_input);

				for (const auto& [axis_id, v] : a1) {
					if (std::abs(v) > 1e-4f) {
						std::println("Server: axis1 {} = {:.3f} from {}:{}",
							axis_id, v, received->from.ip, received->from.port);
					}
				}

				for (const auto& [axis_id, x, y] : a2) {
					if (std::abs(x) > 1e-4f || std::abs(y) > 1e-4f) {
						std::println("Server: axis2 {} = ({:.3f}, {:.3f}) from {}:{}",
							axis_id, x, y, received->from.ip, received->from.port);
					}
				}
			});
	}
}

auto gse::server::peers() const -> const std::unordered_map<network::address, network::remote_peer>& {
	return m_peers;
}

auto gse::server::clients() const -> const std::unordered_map<network::address, client_data>& {
	return m_clients;
}

auto gse::server::host_entity() const -> std::optional<id> {
	return m_host_entity;
}

auto gse::server::host_address() const -> std::optional<network::address> {
	return m_host_addr;
}

auto gse::server::process_header(network::bitstream& stream, network::remote_peer& peer) -> packet_header {
	const auto header = stream.read<packet_header>();

	if (header.sequence > peer.remote_ack_sequence()) {
		if (const std::uint32_t diff = header.sequence - peer.remote_ack_sequence(); diff < 32) {
			peer.remote_ack_bitfield() <<= diff;
			peer.remote_ack_bitfield() |= (1 << (diff - 1));
		}
		else {
			peer.remote_ack_bitfield() = 1;
		}
		peer.remote_ack_sequence() = header.sequence;
	}
	else if (header.sequence < peer.remote_ack_sequence()) {
		if (const std::uint32_t diff = peer.remote_ack_sequence() - header.sequence; diff < 32) {
			peer.remote_ack_bitfield() |= (1 << (diff - 1));
		}
	}

	return header;
}
