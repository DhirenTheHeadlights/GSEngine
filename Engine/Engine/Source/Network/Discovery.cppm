export module gse.network:discovery;

import std;

import :socket;
import :bitstream;
import :packet_header;
import :message;
import :server_info;

import gse.math;
import gse.utility;

export namespace gse::network {
    struct discovery_result {
        address addr;
        std::string name;
        std::string map;
        std::uint8_t players{};
        std::uint8_t max_players{};
        std::uint32_t build{};
    };

    struct discovery_provider {
    	virtual ~discovery_provider() = default;

        virtual auto refresh(
            time_t<std::uint32_t>
        ) -> void = 0;

        virtual auto results(
	    ) const -> std::span<const discovery_result> = 0;
    };

    class wan_directory_provider : public discovery_provider {
    public:
        wan_directory_provider(
            std::vector<discovery_result> seed = {}
        );

        ~wan_directory_provider();

        auto refresh(
            time_t<std::uint32_t>
        ) -> void override;

        auto results(
        ) const -> std::span<const discovery_result> override;

        auto set_seed(
            std::vector<discovery_result> seed
        ) -> void;
    private:
        auto query_servers_async(
            time_t<std::uint32_t> timeout
        ) -> void;

        std::vector<discovery_result> m_seed;
        mutable std::vector<discovery_result> m_published;  
        std::vector<discovery_result> m_pending;            
        mutable std::mutex m_mutex;
        std::atomic<bool> m_querying{ false };
        mutable std::atomic<bool> m_has_pending{ false };
        time_t<std::uint32_t> m_last_refresh{ seconds(0) };
    };
}

gse::network::wan_directory_provider::wan_directory_provider(std::vector<gse::network::discovery_result> seed)
    : m_seed(seed), m_published(seed), m_pending(std::move(seed)) {}

gse::network::wan_directory_provider::~wan_directory_provider() {
    while (m_querying.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

auto gse::network::wan_directory_provider::refresh(time_t<std::uint32_t> timeout) -> void {
    m_last_refresh = system_clock::now();

    if (m_querying.exchange(true)) {
        return;
    }

    std::thread([this, timeout] {
        query_servers_async(timeout);
        m_querying.store(false);
    }).detach();
}

auto gse::network::wan_directory_provider::query_servers_async(time_t<std::uint32_t> timeout) -> void {
    std::vector<discovery_result> local_copy;
    {
        std::lock_guard lock(m_mutex);
        local_copy = m_seed;
    }

    if (local_copy.empty()) {
        return;
    }

    udp_socket socket;
    if (!socket.bind(address{ .ip = "0.0.0.0", .port = 0 })) {
        return;
    }

    std::array<std::byte, 64> request_buffer;
    bitstream request_stream(request_buffer);
    request_stream.write(packet_header{});
    write(request_stream, server_info_request{});

    const packet request_pkt{
        .data = reinterpret_cast<std::uint8_t*>(request_buffer.data()),
        .size = request_stream.bytes_written()
    };

    for (const auto& server : local_copy) {
        socket.send_data(request_pkt, server.addr);
    }

    std::unordered_map<std::string, server_info_response> responses;
    const auto start_time = std::chrono::steady_clock::now();
    const auto timeout_duration = std::chrono::milliseconds(timeout.as<milliseconds>());
    std::array<std::byte, 256> recv_buffer;

    while (std::chrono::steady_clock::now() - start_time < timeout_duration) {
        if (socket.wait_readable(milliseconds(10)) != wait_result::ready) {
            continue;
        }

        while (const auto received = socket.receive_data(recv_buffer)) {
            const std::string key = received->from.ip + ":" + std::to_string(received->from.port);

            const std::span data(recv_buffer.data(), received->bytes_read);
            bitstream response_stream(data);

            (void)response_stream.read<packet_header>();
            const auto msg_id = message_id(response_stream);

            try_decode<server_info_response>(response_stream, msg_id, [&](const server_info_response& resp) {
                responses[key] = resp;
            });
        }
    }

    for (auto& server : local_copy) {
        const std::string key = server.addr.ip + ":" + std::to_string(server.addr.port);
        if (const auto it = responses.find(key); it != responses.end()) {
            server.players = it->second.players;
            server.max_players = it->second.max_players;
        }
    }

    {
        std::lock_guard lock(m_mutex);
        m_pending = std::move(local_copy);
    }
    m_has_pending.store(true, std::memory_order_release);
}

auto gse::network::wan_directory_provider::results() const -> std::span<const discovery_result> {
    if (m_has_pending.load(std::memory_order_acquire)) {
        std::lock_guard lock(m_mutex);
        m_published = m_pending;
        m_has_pending.store(false, std::memory_order_release);
    }
    return m_published;
}

auto gse::network::wan_directory_provider::set_seed(std::vector<discovery_result> seed) -> void {
    std::lock_guard lock(m_mutex);
    m_seed = seed;
    m_pending = seed;
    m_published = std::move(seed);
    m_has_pending.store(false, std::memory_order_release);
}

