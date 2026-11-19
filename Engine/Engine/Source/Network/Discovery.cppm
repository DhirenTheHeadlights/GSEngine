export module gse.network:discovery;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.time;
import std;

import :bitstream;
import :message;
import :packet_header;
import :server_info;
import :socket;

export namespace gse::network {
	struct discovery_result {
		address addr;
		std::string name;
		std::uint8_t players{};
		std::uint8_t max_players{};
		std::uint32_t build{};
	};

	struct discovery_provider {
		virtual ~discovery_provider() = default;

		virtual auto refresh(
			time
		) -> void = 0;

		virtual auto results() -> std::span<const discovery_result> = 0;
	};

	class wan_directory_provider : public discovery_provider {
	public:
		wan_directory_provider(
			std::vector<discovery_result> seed = {}
		);

		~wan_directory_provider();

		auto refresh(
			time
		) -> void override;

		auto results() -> std::span<const discovery_result> override;

	private:
		auto publish_finished() -> void;

		std::vector<discovery_result> m_seed;
		std::vector<discovery_result> m_published;
		task::pending<std::optional<std::vector<discovery_result>>> m_query;
		time m_last_refresh{ seconds(0.f) };
	};
}

namespace gse::network {
	auto query_servers(
		std::vector<discovery_result> servers,
		time timeout
	) -> std::optional<std::vector<discovery_result>>;
}

gse::network::wan_directory_provider::wan_directory_provider(std::vector<discovery_result> seed) : m_seed(seed), m_published(std::move(seed)) {}

gse::network::wan_directory_provider::~wan_directory_provider() {
	while (m_query.active() && !m_query.take()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

auto gse::network::wan_directory_provider::refresh(time timeout) -> void {
	m_last_refresh = system_clock::now();

	publish_finished();
	if (m_query.active()) {
		return;
	}

	m_query.start([servers = m_seed, timeout]() mutable {
		return query_servers(std::move(servers), timeout);
	}, trace_id<"network::discovery">(), task::lane::io);
}

auto gse::network::wan_directory_provider::results() -> std::span<const discovery_result> {
	publish_finished();
	return m_published;
}

auto gse::network::wan_directory_provider::publish_finished() -> void {
	if (auto finished = m_query.take(); finished && *finished) {
		m_published = std::move(**finished);
	}
}

auto gse::network::query_servers(std::vector<discovery_result> servers, const time timeout) -> std::optional<std::vector<discovery_result>> {
	if (servers.empty()) {
		return std::nullopt;
	}

	udp_socket socket;
	if (!socket.bind(address{
		.ip = "0.0.0.0",
		.port = 0
		})) {
		return std::nullopt;
	}

	std::array<std::byte, 64> request_buffer;
	write_bitstream request_stream(request_buffer);
	request_stream.write(packet_header{});
	write(
		request_stream,
		server_info_request{}
	);

	const packet request_pkt{
		.data = reinterpret_cast<std::uint8_t*>(request_buffer.data()),
		.size = request_stream.bytes_written()
	};

	for (auto& server : servers) {
		if (const auto resolved = resolve_address(server.addr)) {
			server.addr = *resolved;
		}
		socket.send_data(request_pkt, server.addr);
	}

	std::map<address, server_info_response> responses;
	clock timeout_clock;
	std::array<std::byte, 256> recv_buffer;

	while (timeout_clock.elapsed() < timeout && responses.size() < servers.size()) {
		if (socket.wait_readable(milliseconds(10.f)) != wait_result::ready) {
			continue;
		}

		while (const auto received = socket.receive_data(recv_buffer)) {
			const std::span data(recv_buffer.data(), received->bytes_read);
			read_bitstream response_stream(data);

			(void)response_stream.read<packet_header>();
			const auto msg_id = response_stream.read<std::uint64_t>();

			try_decode<server_info_response>(
				response_stream,
				msg_id,
				[&](const server_info_response& resp) {
					responses[received->from] = resp;
				}
			);
		}
	}

	for (auto& server : servers) {
		if (const auto it = responses.find(server.addr); it != responses.end()) {
			server.players = it->second.players;
			server.max_players = it->second.max_players;
		}
	}

	return servers;
}