export module gse.network:message;

import std;

import :bitstream;

export namespace gse::network {
	struct message;

	template <typename T>
	constexpr auto message_id(
		std::type_identity<T>
	) -> std::uint8_t {
		return 0;
	}

	template <typename T>
	auto encode(
		bitstream&,
		const T&
	) -> void {}

	template <typename T>
	auto decode(
		bitstream&,
		std::type_identity<T>
	) -> void {}

	template <typename T>
	concept is_message = requires (bitstream& s, const std::remove_cvref_t<T>& t) {
		{ message_id(std::type_identity<std::remove_cvref_t<T>>{}) } -> std::convertible_to<std::uint16_t>;
		{ encode(s, t) } -> std::same_as<void>;
		{ decode(s, std::type_identity<std::remove_cvref_t<T>>{}) } -> std::same_as<std::remove_cvref_t<T>>;
	};

	template <is_message T>
	auto write(
		bitstream& s,
		const T& msg
	) -> void;

	auto message_id(
		bitstream& s
	) -> std::uint16_t;

	template <typename T, typename Fn>
	auto try_decode(
		bitstream& s,
		std::uint16_t id,
		Fn&& on_decode
	) -> bool;
}

template <gse::network::is_message T>
auto gse::network::write(bitstream& s, const T& msg) -> void {
	const auto id = message_id(std::type_identity<T>{});
	s.write(id);
	encode(s, msg);
}

auto gse::network::message_id(bitstream& s) -> std::uint16_t {
	return s.read<std::uint16_t>();
}

template <typename T, typename Fn>
auto gse::network::try_decode(bitstream& s, std::uint16_t id, Fn&& on_decode) -> bool {
	if (id == message_id(std::type_identity<T>{})) {
		T m = decode(s, std::type_identity<T>{});
		std::invoke(std::forward<Fn>(on_decode), m);
		return true;
	}
	return false;
}


