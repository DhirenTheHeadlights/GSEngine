export module gse.network:registry_sync;

import std;

import :message;
import :bitstream;

import gse.physics;
import gse.graphics;
import gse.utility;

export namespace gse::network {
	template <typename T>
	struct message_tag;

	template <typename T>
	struct component_upsert {
		uuid owner_id;
		T data;
	};

	template <typename T>
	constexpr auto message_id(
		std::type_identity<component_upsert<T>>
	) -> std::uint16_t;

	template <typename T>
	auto encode(
		bitstream& s,
		const component_upsert<T>& m
	) -> void;

	template <typename T>
	auto decode(
		bitstream& s,
		std::type_identity<component_upsert<T>>
	) -> component_upsert<T>;

	template <typename T>
	struct component_remove {
		uuid& owner_id;
	};

	template <typename T>
	constexpr auto message_id(
		std::type_identity<component_remove<T>>
	) -> std::uint16_t;

	template <typename T>
	auto encode(
		bitstream& s,
		const component_remove<T>& m
	) -> void;

	template <typename T>
	auto decode(
		bitstream& s,
		std::type_identity<component_remove<T>>
	) -> component_remove<T>;

	template <typename Component, typename T>
	auto apply_upsert(
		registry& reg,
		const component_upsert<T>& msg
	) -> void;

	template <typename Component, typename T>
	auto apply_remove(
		registry& reg,
		const component_upsert<T>& msg
	) -> void;
}

template <typename T>
constexpr auto gse::network::message_id(std::type_identity<component_upsert<T>>) -> std::uint16_t {
	static_assert(std::is_trivially_copyable_v<T>, "Component must be trivially copyable to participate in network serialization");
	return message_tag<T>::upsert_id;
}

template <typename T>
auto gse::network::encode(bitstream& s, const component_upsert<T>& m) -> void {
	s.write(m.owner_id);
	s.write(std::as_bytes(std::span{ &m.data, 1 }));
}

template <typename T>
auto gse::network::decode(bitstream& s, std::type_identity<component_upsert<T>>) -> component_upsert<T> {
	return {
		.owner_id = s.read<uuid>(),
		.data = s.read<T>()
	};
}

template <typename T>
constexpr auto gse::network::message_id(std::type_identity<component_remove<T>>) -> std::uint16_t {
	return message_tag<T>::remove_id;
}

template <typename T>
auto gse::network::encode(bitstream& s, const component_remove<T>& m) -> void {
	s.write(m.owner_id);
}

template <typename T>
auto gse::network::decode(bitstream& s, std::type_identity<component_remove<T>>) -> component_remove<T> {
	return {
		.owner_id = s.read<uuid>()
	};
}

template <typename Component, typename T>
auto gse::network::apply_upsert(registry& reg, const component_upsert<T>& msg) -> void {
	static_assert(std::is_same_v<typename Component::params, T>, "Component::params must be T");
    const id local = find_or_generate_id(msg.owner_id);
    reg.ensure_active(local);

    if (auto* c = reg.try_linked_object_write<Component>(local)) {
        static_cast<T&>(*c) = msg.data;
    } else {
        reg.add_component<Component>(local, msg.data);
    }
}

template <typename Component, typename T>
auto gse::network::apply_remove(registry& reg, const component_upsert<T>& msg) -> void {
	if (auto maybe = try_find(msg.owner_id)) {
		reg.remove_link<Component>(*maybe);
	}
}








