export module gse.network:registry_sync;

import std;

import :message;
import :bitstream;

import gse.physics;
import gse.graphics;
import gse.platform;
import gse.utility;

export namespace gse::network {
	consteval auto stable_code(
		std::string_view s
	) -> std::uint16_t;

	template <typename T>
	consteval auto component_name(
	) -> std::string_view;

	template <>
	consteval auto component_name<physics::motion_component>(
	) -> std::string_view;

	template <>
	consteval auto component_name<render_component>(
	) -> std::string_view;

	template <typename T>
	consteval auto component_code_upsert(
	) -> std::uint16_t;

	template <typename T>
	consteval auto component_code_remove(
	) -> std::uint16_t;

	template <typename T>
	struct message_tag {
		static constexpr std::uint16_t upsert_id = component_code_upsert<T>();
		static constexpr std::uint16_t remove_id = component_code_remove<T>();
	};

	template <typename T>
	struct component_upsert {
		id owner_id;
		T::network_data_t data;
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
		id owner_id;
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

	auto match_and_apply_components(
		bitstream& s,
		std::uint16_t id,
		auto&& on_upsert, 
		auto&& on_remove  
	) -> bool;

	constexpr auto networked_types = std::tuple<
		std::type_identity<physics::motion_component>,
		std::type_identity<render_component>,
		std::type_identity<player_controller>
	>{};

	template <typename F>
	auto for_each_networked_component(
		F&& f
	) -> void;
}

consteval auto gse::network::stable_code(std::string_view s) -> std::uint16_t {
	std::uint32_t h = 0x811C9DC5u;
	for (const unsigned char c : s) {
		h ^= c; h *= 16777619u;
	}
	return static_cast<std::uint16_t>((h ^ (h >> 16)) & 0xFFFF);
}

template <>
consteval auto gse::network::component_name<gse::physics::motion_component>() -> std::string_view {
	return "gse.physics.motion_component";
}

template <>
consteval auto gse::network::component_name<gse::player_controller>() -> std::string_view {
	return "gse.player_controller";
}

template <>
consteval auto gse::network::component_name<gse::render_component>() -> std::string_view {
	return "gse.render_component";
}

namespace gse::network {
	template <typename T> struct always_false : std::false_type {
	};
}

template <typename C>
consteval auto gse::network::component_name() -> std::string_view {
	static_assert(always_false<C>::value, "component_name<C> not specialized for this component type");
	return {};
}

template <typename T>
consteval auto gse::network::component_code_upsert() -> std::uint16_t {
	return stable_code(component_name<T>());
}

template <typename T>
consteval auto gse::network::component_code_remove() -> std::uint16_t {
	return static_cast<std::uint16_t>(stable_code(component_name<T>()) ^ 0x5A5Au);
}

template <typename T>
constexpr auto gse::network::message_id(std::type_identity<component_upsert<T>>) -> std::uint16_t {
	static_assert(std::is_trivially_copyable_v<typename T::network_data_t>);
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
		.owner_id = s.read<id>(),
		.data = s.read<typename T::network_data_t>()
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
		.owner_id = s.read<id>()
	};
}

template <typename F>
auto gse::network::for_each_networked_component(F&& f) -> void {
	std::apply(
		[&]<typename... T0>(T0... ti) {
		(f.template operator() < typename T0::type > (), ...);
	},
		networked_types
	);
}

auto gse::network::match_and_apply_components(bitstream& s, std::uint16_t id, auto&& on_upsert, auto&& on_remove) -> bool {
	bool handled = false;

	auto handle = [&]<class Ti>(Ti) {
		using c = Ti::type;
		using p = c::network_data_t;
		static_assert(std::is_trivially_copyable_v<p>);
		
		if (!handled) handled |= try_decode<component_upsert<c>>(s, id, on_upsert);
		if (!handled) handled |= try_decode<component_remove<c>>(s, id, on_remove);
	};

	std::apply(
		[&](auto... ti) {
		(handle(ti), ...);
	},
		networked_types
	);

	return handled;
}