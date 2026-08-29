export module gse.network:dispatch;

import std;

import gse.containers;
import gse.ecs;

import :socket;
import :message;
import :bitstream;
import :endpoint;

export namespace gse::network {
	template <is_network_message T>
	struct received {
		address from;
		T message;
	};

	template <is_network_message T>
	struct send_request {
		T message;
		std::optional<address> to;
		bool reliable = false;
	};

	template <typename Pack>
	struct inbound_channel;

	template <typename... Messages>
	struct inbound_channel<type_pack<Messages...>> {
		using type = channel_write<received<Messages>...>;
	};

	template <typename Pack>
	using inbound_channel_t = typename inbound_channel<Pack>::type;

	template <typename Pack, typename... Extra>
	struct outbound_channel;

	template <typename... Messages, typename... Extra>
	struct outbound_channel<type_pack<Messages...>, Extra...> {
		using type = channel_read<Extra..., send_request<Messages>...>;
	};

	template <typename Pack, typename... Extra>
	using outbound_channel_t = typename outbound_channel<Pack, Extra...>::type;

	template <typename Pack>
	auto route_inbound(
		read_bitstream& s,
		const inbound_message& msg,
		const inbound_channel_t<Pack>& out
	) -> bool;

	template <typename Pack, typename Requests, typename Sink>
	auto drain_outbound(
		const Requests& requests,
		Sink&& sink
	) -> void;
}

template <typename Pack>
auto gse::network::route_inbound(read_bitstream& s, const inbound_message& msg, const inbound_channel_t<Pack>& out) -> bool {
	return [&]<typename... Messages>(type_pack<Messages...>) {
		return (
			try_decode<Messages>(
				s,
				msg.id,
				[&](const Messages& decoded) {
					out.template push<received<Messages>>({
						.from = msg.from,
						.message = decoded,
					});
				}
			) ||
			...
		);
	}(Pack{});
}

template <typename Pack, typename Requests, typename Sink>
auto gse::network::drain_outbound(const Requests& requests, Sink&& sink) -> void {
	[&]<typename... Messages>(type_pack<Messages...>) {
		(
			[&] {
				for (const auto& req : requests.template of<send_request<Messages>>()) {
					sink(req.message, req.to, req.reliable);
				}
			}(),
			...
		);
	}(Pack{});
}
