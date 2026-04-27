export module gse.network:message;

import std;
import gse.std_meta;

import :bitstream;

export namespace gse::network {
	struct network_message {};

	template <typename T>
	consteval auto is_network_message(
	) -> bool;

	consteval auto stable_code(
		std::string_view s
	) -> std::uint16_t;

	template <typename T>
		requires (gse::network::is_network_message<T>())
	constexpr auto message_id(
		std::type_identity<T>
	) -> std::uint16_t;

	template <typename T>
		requires (gse::network::is_network_message<T>())
	auto encode(
		bitstream& s,
		const T& msg
	) -> void;

	template <typename T>
		requires (gse::network::is_network_message<T>())
	auto decode(
		bitstream& s,
		std::type_identity<T>
	) -> T;

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

	class message_switch {
	public:
		message_switch(
			bitstream& s,
			std::uint16_t id
		);

		template <typename T, typename F>
		auto if_is(
			F&& f
		) & -> message_switch&;

		template <typename T, typename F>
		auto else_if_is(
			F&& f
		) & -> message_switch&;

		template <typename F>
		auto if_is(
			F&& f
		) & -> message_switch&;

		template <typename F>
		auto else_if_is(
			F&& f
		) & -> message_switch&;

		template <typename F>
		auto otherwise(
			F&& f
		) & -> void;

		template <typename T, typename F>
		auto if_is(
			F&& f
		) && -> message_switch&&;

		template <typename T, typename F>
		auto else_if_is(
			F&& f
		) && -> message_switch&&;

		template <typename F>
		auto if_is(
			F&& f
		) && -> message_switch&&;

		template <typename F>
		auto else_if_is(
			F&& f
		) && -> message_switch&&;

		template <typename F>
		auto otherwise(
			F&& f
		) && -> void;
	private:
		template <typename T, typename F>
		auto handle(
			F&& f
		) -> message_switch&;

		bitstream* m_stream;
		std::uint16_t m_id{};
		bool m_handled{ false };
	};

	auto match_message(
		bitstream& s,
		std::uint16_t id
	) -> message_switch;

	auto match_message(
		bitstream& s
	) -> message_switch;
}

auto gse::network::match_message(bitstream& s, const std::uint16_t id) -> message_switch {
	return message_switch{ s, id };
}

auto gse::network::match_message(bitstream& s) -> message_switch {
	return message_switch{ s, message_id(s) };
}

template <typename T>
consteval auto gse::network::is_network_message() -> bool {
	return std::ranges::any_of(
		std::define_static_array(std::meta::annotations_of(^^T)),
		[](std::meta::info ann) {
			return std::meta::remove_const(std::meta::type_of(std::meta::constant_of(ann))) == ^^network_message;
		}
	);
}

consteval auto gse::network::stable_code(std::string_view s) -> std::uint16_t {
	std::uint32_t h = 0x811C9DC5u;
	for (const unsigned char c : s) {
		h ^= c;
		h *= 16777619u;
	}
	return static_cast<std::uint16_t>((h ^ (h >> 16)) & 0xFFFF);
}

template <typename T>
	requires (gse::network::is_network_message<T>())
constexpr auto gse::network::message_id(std::type_identity<T>) -> std::uint16_t {
	return stable_code(std::meta::display_string_of(^^T));
}

template <typename T>
	requires (gse::network::is_network_message<T>())
auto gse::network::encode(bitstream& s, const T& msg) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		s.write(msg.[:m:]);
	}
}

template <typename T>
	requires (gse::network::is_network_message<T>())
auto gse::network::decode(bitstream& s, std::type_identity<T>) -> T {
	T msg{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		msg.[:m:] = s.read<typename [: std::meta::type_of(m) :]>();
	}
	return msg;
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


gse::network::message_switch::message_switch(bitstream& s, const std::uint16_t id) : m_stream(&s), m_id(id) {}

template <typename T, typename F>
auto gse::network::message_switch::if_is(F&& f) & -> message_switch& {
	return handle<T>(std::forward<F>(f));
}

template <typename T, typename F>
auto gse::network::message_switch::else_if_is(F&& f) & -> message_switch& {
	return handle<T>(std::forward<F>(f));
}

template <typename F>
auto gse::network::message_switch::if_is(F&& f) & -> message_switch& {
	using t = std::remove_cvref_t<typename [: std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<F>::operator())[0]) :]>;
	return handle<t>(std::forward<F>(f));
}

template <typename F>
auto gse::network::message_switch::else_if_is(F&& f) & -> message_switch& {
	using t = std::remove_cvref_t<typename [: std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<F>::operator())[0]) :]>;
	return handle<t>(std::forward<F>(f));
}

template <typename F>
auto gse::network::message_switch::otherwise(F&& f) & -> void {
	if (!m_handled) {
		if constexpr (std::is_invocable_v<F&>) {
			std::invoke(std::forward<F>(f));
		}
		else if constexpr (std::is_invocable_v<F&, bitstream&>) {
			std::invoke(std::forward<F>(f), *m_stream);
		}
		m_handled = true;
	}
}

template <typename T, typename F>
auto gse::network::message_switch::if_is(F&& f) && -> message_switch&& {
	(void)static_cast<message_switch&>(*this).if_is<T>(std::forward<F>(f));
	return std::move(*this);
}

template <typename T, typename F>
auto gse::network::message_switch::else_if_is(F&& f) && -> message_switch&& {
	(void)static_cast<message_switch&>(*this).else_if_is<T>(std::forward<F>(f));
	return std::move(*this);
}

template <typename F>
auto gse::network::message_switch::if_is(F&& f) && -> message_switch&& {
	(void)static_cast<message_switch&>(*this).if_is(std::forward<F>(f));
	return std::move(*this);
}

template <typename F>
auto gse::network::message_switch::else_if_is(F&& f) && -> message_switch&& {
	(void)static_cast<message_switch&>(*this).else_if_is(std::forward<F>(f));
	return std::move(*this);
}

template <typename F>
auto gse::network::message_switch::otherwise(F&& f) && -> void {
	static_cast<message_switch&>(*this).otherwise(std::forward<F>(f));
}

template <typename T, typename F>
auto gse::network::message_switch::handle(F&& f) -> message_switch& {
	if (m_handled) {
		return *this;
	}
	if constexpr (is_message<T>) {
		if (m_id == message_id(std::type_identity<T>{})) {
			T msg = decode(*m_stream, std::type_identity<T>{});
			std::invoke(std::forward<F>(f), msg);
			m_handled = true;
		}
	}
	return *this;
}


