export module gse.network:message;

import std;
import gse.std_meta;
import gse.meta;
import gse.core;

import :bitstream;

namespace gse::network {
	template <typename T>
	concept dynamic_field = std::ranges::contiguous_range<T>
		&& std::ranges::sized_range<T>
		&& requires (T v, std::size_t n) { v.resize(n); }
		&& is_trivially_copyable<std::ranges::range_value_t<T>>;

	template <typename T>
	auto encode_field(
		write_bitstream& s,
		const T& v
	) -> void;

	template <typename T>
	auto decode_field(
		read_bitstream& s
	) -> T;
}

export namespace gse::network {
	struct network_message {};

	template <typename T>
	concept is_network_message = has_annotation<network_message>(^^T);

	template <is_network_message T>
	constexpr std::uint64_t message_id_v = stable_id(type_tag<T>());

	template <is_network_message T>
	auto encode(
		write_bitstream& s,
		const T& msg
	) -> void;

	template <is_network_message T>
	auto decode(
		read_bitstream& s,
		std::type_identity<T>
	) -> T;

	template <is_network_message T>
	auto write(
		write_bitstream& s,
		const T& msg
	) -> void;

	template <is_network_message T, typename Fn>
	auto try_decode(
		read_bitstream& s,
		std::uint64_t id,
		Fn&& on_decode
	) -> bool;
}

template <typename T>
auto gse::network::encode_field(write_bitstream& s, const T& v) -> void {
	if constexpr (dynamic_field<T>) {
		s.write(static_cast<std::uint32_t>(v.size()));
		s.write(std::as_bytes(std::span(v.data(), v.size())));
	}
	else {
		s.write(v);
	}
}

template <typename T>
auto gse::network::decode_field(read_bitstream& s) -> T {
	if constexpr (dynamic_field<T>) {
		T v;
		v.resize(s.read<std::uint32_t>());
		s.read(std::as_writable_bytes(std::span(v.data(), v.size())));
		return v;
	}
	else {
		return s.read<T>();
	}
}

template <gse::network::is_network_message T>
auto gse::network::encode(write_bitstream& s, const T& msg) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		encode_field(s, msg.[:m:]);
	}
}

template <gse::network::is_network_message T>
auto gse::network::decode(read_bitstream& s, std::type_identity<T>) -> T {
	T msg{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		using field_type = typename [: std::meta::type_of(m) :];
		msg.[:m:] = decode_field<field_type>(s);
	}
	return msg;
}

template <gse::network::is_network_message T>
auto gse::network::write(write_bitstream& s, const T& msg) -> void {
	s.write(message_id_v<T>);
	encode(s, msg);
}

template <gse::network::is_network_message T, typename Fn>
auto gse::network::try_decode(read_bitstream& s, std::uint64_t id, Fn&& on_decode) -> bool {
	if (id == message_id_v<T>) {
		T m = decode(s, std::type_identity<T>{});
		std::invoke(std::forward<Fn>(on_decode), m);
		return true;
	}
	return false;
}
