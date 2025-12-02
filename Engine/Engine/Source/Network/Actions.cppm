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

	std::vector<axes1_pair> a1;
	a1.reserve(axes1_ids.size());
	for (const auto id : axes1_ids) {
		if (const float v = state.axis1(id); v != 0.f) {
			a1.push_back({ id, v });
		}
	}

	std::vector<axes2_pair> a2;
	a2.reserve(axes2_ids.size());
	for (const auto id : axes2_ids) {
		if (const auto v = state.axis2_v(id); v.x() != 0.f || v.y() != 0.f) {
			a2.push_back({ id, v.x(), v.y() });
		}
	}

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
