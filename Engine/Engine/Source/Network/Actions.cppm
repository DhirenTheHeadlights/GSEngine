export module gse.network:actions;

import std;

import gse.platform;

import :socket;
import :remote_peer;
import :packet_header;
import :bitstream;
import :message;
import :input_frame;

export namespace gse::network {
	struct axes1_pair {
		std::uint16_t id;
		float value;
	};

	struct axes2_pair {
		std::uint16_t id;
		float x, y;
	};

	auto send_input_frame(
		const udp_socket& socket,
		remote_peer& peer,
		std::array<std::byte, max_packet_size>& buffer,
		std::uint32_t input_sequence,
		const actions::state& state,
		std::span<const std::uint16_t> axes1_ids,
		std::span<const std::uint16_t> axes2_ids
	) -> void;
}

auto gse::network::send_input_frame(const udp_socket& socket, remote_peer& peer, std::array<std::byte, max_packet_size>& buffer, const std::uint32_t input_sequence, const actions::state& state, const std::span<const std::uint16_t> axes1_ids, const std::span<const std::uint16_t> axes2_ids) -> void {
	const auto& pm = state.pressed_mask();
	const auto& rm = state.released_mask();
	const auto wc = static_cast<std::uint16_t>(std::max(pm.word_count(), rm.word_count()));

	auto a1 = axes1_ids
		| std::views::transform([&](auto id) { return axes1_pair{ id, state.axis1(id) }; })
		| std::views::filter([](const auto& p) { return p.value != 0.f; })
		| std::ranges::to<std::vector>();

	auto a2 = axes2_ids
		| std::views::transform([&](auto id) -> axes2_pair {
			const auto v = state.axis2_v(id);
			return { id, v.x(), v.y() };
		})
		| std::views::filter([](const auto& p) { return p.x != 0.f || p.y != 0.f; })
		| std::ranges::to<std::vector>();

	bitstream s(buffer);

	const packet_header ph{
		.sequence = ++peer.sequence(),
		.ack = peer.remote_ack_sequence(),
		.ack_bits = peer.remote_ack_bitfield()
	};
	s.write(ph);

	const input_frame_header hdr{
		.input_sequence = input_sequence,
		.client_time_ms = system_clock::now<time_t<std::uint32_t>>().as<milliseconds>(),
		.action_word_count = wc,
		.axes1_count = static_cast<std::uint16_t>(a1.size()),
		.axes2_count = static_cast<std::uint16_t>(a2.size())
	};
	write(s, hdr);

	const auto& pw = pm.words();
	const auto& rw = rm.words();

	for (std::size_t i = 0; i < wc; ++i) {
		const std::uint64_t w = (i < pw.size()) ? pw[i] : 0ull;
		s.write(w);
	}
	for (std::size_t i = 0; i < wc; ++i) {
		const std::uint64_t w = (i < rw.size()) ? rw[i] : 0ull;
		s.write(w);
	}

	if (!a1.empty()) {
		s.write(std::as_bytes(std::span{ a1.data(), a1.size() }));
	}
	if (!a2.empty()) {
		s.write(std::as_bytes(std::span{ a2.data(), a2.size() }));
	}

	const packet pkt{
		.data = reinterpret_cast<std::uint8_t*>(buffer.data()),
		.size = s.bytes_written()
	};

	assert(
		socket.send_data(pkt, peer.addr()) != socket_state::error,
		std::source_location::current(),
		"Failed to send input frame from client."
	);
}
