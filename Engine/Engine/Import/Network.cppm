export module gse.network;

import std;

import gse.core;
import gse.math;
import gse.concurrency;
import gse.ecs;
import gse.assets;
import gse.os;
import gse.graphics;

export import :actions;
export import :remote_peer;
export import :socket;
export import :bitstream;
export import :packet_header;
export import :message;
export import :connection;
export import :ping_pong;
export import :input_frame;
export import :notify_scene_change;
export import :client;
export import :discovery;
export import :registry_sync;
export import :replication;
export import :server_info;

export namespace gse::network {
    struct connection_options {
        address addr;
        std::optional<address> local_bind;
        time_t<std::uint32_t> timeout{ seconds(5) };
        time_t<std::uint32_t> retry{ seconds(1) };
        bool allow_handoff = false;
    };

    struct connect_request {
        using result_type = bool;
        connection_options options;
        channel_promise<bool> promise;
    };

    struct disconnect_request {};

    struct add_provider_request {
        std::shared_ptr<discovery_provider> provider;
    };

    struct clear_providers_request {};

    struct refresh_servers_request {
        time_t<std::uint32_t> timeout = milliseconds(350);
    };

    struct send_request {
        std::function<void(client&)> action;
    };

    struct key_hash {
        auto operator()(
            const address& a
        ) const noexcept -> std::size_t;
    };

    struct key_eq {
        auto operator()(
            const address& a,
            const address& b
        ) const noexcept -> bool;
    };

    struct system {
        struct state {
            client::state connection_state = client::state::disconnected;
            std::vector<discovery_result> available_servers;
            std::vector<inbox_message> user_inbox;
        };

        struct resources {
            std::unique_ptr<client> client_ptr;
            std::vector<std::shared_ptr<discovery_provider>> providers;
            std::vector<gse::move_only_function<void(run_context&)>> deferred;
        };

        static auto run(
            run_context& ctx,
            const asset::state& assets_s,
            resources& r,
            state& s,
            const actions::system::state& actions_state,
            const camera::system::state& cam_state
        ) -> async::task<>;

        static auto shutdown(
            shutdown_context& phase,
            resources& r,
            state& s
        ) -> void;
    };
}
