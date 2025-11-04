export module gse.network:actions;

import std;

import gse.platform;

import :socket;
import :remote_peer;
import :packet_header;
import :bitstream;
import :message;

export namespace gse::network {
	struct input_frame_header {
		std::uint32_t input_sequence = 0;
		std::uint32_t client_time_ms = 0;
		std::uint16_t action_word_count = 0;
		std::uint16_t axes1_count = 0;
		std::uint16_t axes2_count = 0;
	};

	constexpr auto message_id(
		std::type_identity<input_frame_header>
	) -> std::uint16_t;

	auto encode(
		bitstream& s,
		const input_frame_header& header
	) -> void;

	auto decode(
		bitstream& s,
		std::type_identity<input_frame_header>
	) -> input_frame_header;

	auto send_input_frame(
		udp_socket& socket,
		remote_peer& peer,
		std::array<std::byte, max_packet_size>& buffer,
		std::uint32_t input_sequence,
		std::span<const std::uint16_t> axes1_ids,
		std::span<const std::uint16_t> axes2_ids
	) -> void;
}

constexpr auto gse::network::message_id(std::type_identity<input_frame_header>) -> std::uint16_t {
	return 0x0006;
}

auto gse::network::encode(bitstream& s, const input_frame_header& header) -> void {
	s.write(header);
}

auto gse::network::decode(bitstream& s, std::type_identity<input_frame_header>) -> input_frame_header {
	return s.read<input_frame_header>();
}

auto gse::network::send_input_frame(udp_socket& socket, remote_peer& peer, std::array<std::byte, max_packet_size>& buffer, const std::uint32_t input_sequence, std::span<const std::uint16_t> axes1_ids, std::span<const std::uint16_t> axes2_ids) -> void {
	const auto& pm = actions::pressed_mask();
	const auto& rm = actions::released_mask();
	const auto wc = std::max(pm.word_count(), rm.word_count());

	struct axes1_pair {
		std::uint16_t id;
		float value;
	};

	std::vector<axes1_pair> a1;
	a1.reserve(axes1_ids.size());
	for (const auto id : axes1_ids) {
		if (const float v = actions::axis1(id); v != 0.f) {
			a1.push_back({ id, v });
		}
	}

	struct axis2_pair {
		std::uint16_t id;
		float x, y;
	};

	std::vector<axis2_pair> a2;
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
		.client_time_ms = system_clock::now<nanoseconds, std::uint32_t>().as<nanoseconds>(),
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
