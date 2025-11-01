export module gse.network:message;

import std;

export namespace gse::network {
	enum class message_type : std::uint8_t {
		connection_request,
		connection_accepted,
		ping,
		pong,
		notify_scene_change,
		input_frame
	};

	struct input_frame_header {
		std::uint32_t input_sequence;
		std::uint32_t client_time_ms;
		std::uint16_t action_word_count;
		std::uint16_t axes1_count;
		std::uint16_t axes2_count;
	};

	struct connection_request_message {};
	struct connection_accepted_message {};

	struct ping_message {
		std::uint32_t sequence;
	};

	struct pong_message {
		std::uint32_t sequence;
	};

	struct notify_scene_change_message {
		std::array<char, 64> scene_name; 
	};

	using message = std::variant<
		connection_request_message,
		connection_accepted_message,
		ping_message,
		pong_message,
		notify_scene_change_message
	>;

	template <class T>
	struct message_tag {
		static_assert(!std::is_same_v<T, T>, "No message_tag specialization for this type");
	};

	template <> struct message_tag<connection_request_message> {
		static constexpr auto value = message_type::connection_request;
	};

	template <> struct message_tag<connection_accepted_message> {
		static constexpr auto value = message_type::connection_accepted;
	};

	template <> struct message_tag<ping_message> {
		static constexpr auto value = message_type::ping;
	};

	template <> struct message_tag<pong_message> {
		static constexpr auto value = message_type::pong;
	};

	template <> struct message_tag<notify_scene_change_message> {
		static constexpr auto value = message_type::notify_scene_change;
	};

	template <> struct message_tag<input_frame_header> {
		static constexpr auto value = message_type::input_frame;
	};

	template <typename T>
	constexpr message_type message_type_v = message_tag<std::remove_cvref_t<T>>::value;
}