export module gse.network:notify_scene_change;

import std;

import :message;

export namespace gse::network {
	struct notify_scene_change {
		std::array<char, 64> scene_name{};
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
	s.write(std::as_bytes(std::span{ m.scene_name }));
}

auto gse::network::decode(bitstream& s, std::type_identity<notify_scene_change>) -> notify_scene_change {
	notify_scene_change m{};
	s.read(std::as_writable_bytes(std::span{ m.scene_name }));
	return m;
}
