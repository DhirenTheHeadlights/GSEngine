export module gse.network:connection;

import :message;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::network {
    struct connection_request {
    };

    constexpr auto message_id(
        std::type_identity<connection_request>
    ) -> std::uint16_t;

    auto encode(
        bitstream& stream,
        const connection_request&
    ) -> void;

    auto decode(
        bitstream& stream,
        std::type_identity<connection_request>
    ) -> connection_request;

    struct connection_accepted {
        id controller_id{};
    };

    constexpr auto message_id(
        std::type_identity<connection_accepted>
    ) -> std::uint16_t;

    auto encode(
        bitstream& stream,
        const connection_accepted&
    ) -> void;

    auto decode(
        bitstream& stream,
        std::type_identity<connection_accepted>
    ) -> connection_accepted;
}

constexpr auto gse::network::message_id(std::type_identity<connection_accepted>) -> std::uint16_t {
    return 0x0001;
}

auto gse::network::encode(bitstream& stream, const connection_accepted& m) -> void {
    stream.write(m.controller_id);
}

auto gse::network::decode(bitstream& stream, std::type_identity<connection_accepted>) -> connection_accepted {
    return {
        .controller_id = stream.read<id>()
    };
}

constexpr auto gse::network::message_id(std::type_identity<connection_request>) -> std::uint16_t {
    return 0x0002;
}

auto gse::network::encode(bitstream& stream, const connection_request&) -> void {
}

auto gse::network::decode(bitstream& stream, std::type_identity<connection_request>) -> connection_request {
    return {};
}
