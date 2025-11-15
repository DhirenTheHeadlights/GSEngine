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
		udp_socket& socket,
		remote_peer& peer,
		std::array<std::byte, max_packet_size>& buffer,
		std::uint32_t input_sequence,
		std::span<const std::uint16_t> axes1_ids,
		std::span<const std::uint16_t> axes2_ids
	) -> void;
}

auto gse::network::send_input_frame(udp_socket& socket, remote_peer& peer, std::array<std::byte, max_packet_size>& buffer, const std::uint32_t input_sequence, std::span<const std::uint16_t> axes1_ids, std::span<const std::uint16_t> axes2_ids) -> void {
	const auto& pm = actions::pressed_mask();
	const auto& rm = actions::released_mask();
	const auto wc = std::max(pm.word_count(), rm.word_count());

	std::vector<axes1_pair> a1;
	a1.reserve(axes1_ids.size());
	for (const auto id : axes1_ids) {
		if (const float v = actions::axis1(id); v != 0.f) {
			a1.push_back({ id, v });
		}
	}

	std::vector<axes2_pair> a2;
	a2.reserve(axes2_ids.size());
	for (const auto id : axes2_ids) {
		if (const auto v = actions::axis2(id); v.x() != 0.f || v.y() != 0.f) {
			a2.push_back({ id, v.x(), v.y() });
		}
	}

	bitstream s(buffer);

	const packet_header ph{
		.sequence = peer.sequence()++,
		.ack = peer.remote_ack_sequence(),
		.ack_bits = peer.remote_ack_bitfield()
	};
	s.write(ph);

	const input_frame_header hdr{
		.input_sequence = input_sequence,
		.client_time_ms = system_clock::now<time_t<std::uint32_t>>().as<milliseconds>(),
		.action_word_count = static_cast<std::uint16_t>(wc),
		.axes1_count = static_cast<std::uint16_t>(a1.size()),
		.axes2_count = static_cast<std::uint16_t>(a2.size())
	};

	write(s, hdr);

	std::vector<std::uint64_t> temp;

	const auto& pw = pm.words();
	const auto& rw = rm.words();

	std::ranges::copy(pw, temp.begin());

	for (auto w : pw) {
		s.write(w);
	}

	std::ranges::fill(temp, 0);
	std::ranges::copy(rw, temp.begin());

	for (auto w : rw) {
		s.write(w);
	}
}
