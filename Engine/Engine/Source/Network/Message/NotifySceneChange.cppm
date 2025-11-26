export module gse.network:notify_scene_change;

import std;

import :message;

import gse.utility;

export namespace gse::network {
    struct notify_scene_change {
        id scene_id{};
    };

    constexpr auto message_id(
        std::type_identity<notify_scene_change>
    ) -> std::uint16_t;

    auto encode(
        bitstream& s,
        const notify_scene_change& m
    ) -> void;

    auto decode(
        bitstream& s,
        std::type_identity<notify_scene_change>
    ) -> notify_scene_change;
}

constexpr auto gse::network::message_id(std::type_identity<notify_scene_change>) -> std::uint16_t {
    return 0x0005;
}

auto gse::network::encode(bitstream& s, const notify_scene_change& m) -> void {
    s.write(m.scene_id);
}

auto gse::network::decode(bitstream& s, std::type_identity<notify_scene_change>) -> notify_scene_change {
    return {
        .scene_id = s.read<id>()
    };
}
