module gse.graphics;

import std;

import :mp4_muxer;
import :capture_ring;

import gse.math;
import gse.gpu;
import gse.core;
import gse.time;
import gse.log;

namespace gse::renderer::capture::mp4 {
	constexpr std::uint32_t timescale = 1'000'000;

	struct leb128_read {
		std::uint64_t value;
		std::size_t bytes_consumed;
	};

	struct av1_obu {
		std::uint8_t type;
		std::span<const std::byte> full;
	};

	struct h265_nalu {
		std::uint8_t type;
		std::span<const std::byte> payload;
	};

	struct h265_parameter_sets {
		std::vector<std::vector<std::byte>> vps;
		std::vector<std::vector<std::byte>> sps;
		std::vector<std::vector<std::byte>> pps;
	};

	constexpr std::uint8_t av1_obu_sequence_header = 1;
	constexpr std::uint8_t h265_nal_vps = 32;
	constexpr std::uint8_t h265_nal_sps = 33;
	constexpr std::uint8_t h265_nal_pps = 34;

	class box_scope : public non_copyable {
	public:
		box_scope(
			std::vector<std::byte>& out,
			std::array<char, 4> type
		);

		~box_scope() override;

	private:
		std::vector<std::byte>& m_out;
		std::size_t m_length_offset;
	};

	auto fourcc(
		char a,
		char b,
		char c,
		char d
	) -> std::array<char, 4>;

	auto push_u8(
		std::vector<std::byte>& out,
		std::uint8_t v
	) -> void;

	auto push_u16_be(
		std::vector<std::byte>& out,
		std::uint16_t v
	) -> void;

	auto push_u32_be(
		std::vector<std::byte>& out,
		std::uint32_t v
	) -> void;

	auto push_u64_be(
		std::vector<std::byte>& out,
		std::uint64_t v
	) -> void;

	auto push_bytes(
		std::vector<std::byte>& out,
		std::span<const std::byte> src
	) -> void;

	auto push_fourcc(
		std::vector<std::byte>& out,
		const std::array<char, 4>& cc
	) -> void;

	auto pts_to_timescale(
		time pts
	) -> std::uint64_t;

	auto read_leb128(
		std::span<const std::byte> data,
		std::size_t offset
	) -> std::optional<leb128_read>;

	auto split_av1_obus(
		std::span<const std::byte> bitstream
	) -> std::vector<av1_obu>;

	auto find_sequence_header_obu(
		std::span<const std::byte> bitstream
	) -> std::optional<std::span<const std::byte>>;

	auto build_av1c(
		std::span<const std::byte> sequence_header_obu
	) -> std::vector<std::byte>;

	auto split_h265_nalus(
		std::span<const std::byte> bitstream
	) -> std::vector<h265_nalu>;

	auto collect_h265_parameter_sets(
		std::span<const std::byte> bitstream
	) -> h265_parameter_sets;

	auto build_hvcc(
		const h265_parameter_sets& sets
	) -> std::vector<std::byte>;

	auto emit_ftyp(
		std::vector<std::byte>& out,
		gpu::video_codec codec
	) -> void;

	auto emit_mvhd(
		std::vector<std::byte>& out,
		std::uint64_t duration
	) -> void;

	auto emit_tkhd(
		std::vector<std::byte>& out,
		std::uint64_t duration,
		vec2u extent
	) -> void;

	auto emit_elst(
		std::vector<std::byte>& out,
		std::uint64_t duration,
		std::int64_t media_start
	) -> void;

	auto emit_mdhd(
		std::vector<std::byte>& out,
		std::uint64_t duration
	) -> void;

	auto emit_hdlr(
		std::vector<std::byte>& out
	) -> void;

	auto emit_vmhd(
		std::vector<std::byte>& out
	) -> void;

	auto emit_dinf(
		std::vector<std::byte>& out
	) -> void;

	auto emit_stsd(
		std::vector<std::byte>& out,
		vec2u extent,
		gpu::video_codec codec,
		std::span<const std::byte> codec_config
	) -> void;

	auto emit_stts(
		std::vector<std::byte>& out,
		const std::vector<std::uint32_t>& sample_durations
	) -> void;

	auto emit_stss(
		std::vector<std::byte>& out,
		std::span<const gpu::encoded_unit> units
	) -> void;

	auto emit_stsc(
		std::vector<std::byte>& out,
		std::uint32_t sample_count
	) -> void;

	auto emit_stsz(
		std::vector<std::byte>& out,
		std::span<const gpu::encoded_unit> units
	) -> void;

	auto emit_co64(
		std::vector<std::byte>& out,
		std::uint64_t mdat_payload_offset
	) -> void;

	auto rewrite_co64_offset(
		std::vector<std::byte>& moov,
		std::uint64_t payload_offset
	) -> void;

	auto emit_ftyp_fragmented(
		std::vector<std::byte>& out,
		gpu::video_codec codec
	) -> void;

	auto emit_mvhd_unknown(
		std::vector<std::byte>& out,
		std::size_t& duration_offset_out
	) -> void;

	auto emit_tkhd_unknown(
		std::vector<std::byte>& out,
		vec2u extent,
		std::size_t& duration_offset_out
	) -> void;

	auto emit_mdhd_unknown(
		std::vector<std::byte>& out,
		std::size_t& duration_offset_out
	) -> void;

	auto emit_stbl_empty(
		std::vector<std::byte>& out,
		vec2u extent,
		gpu::video_codec codec,
		std::span<const std::byte> codec_config
	) -> void;

	auto emit_mvex_trex(
		std::vector<std::byte>& out
	) -> void;

	auto emit_fragment_moof(
		std::vector<std::byte>& out,
		std::uint32_t sequence,
		std::uint64_t base_decode_time,
		std::span<const gpu::encoded_unit> samples,
		std::span<const std::uint32_t> durations,
		std::uint32_t per_sample_prefix_bytes,
		std::size_t& trun_data_offset_field
	) -> void;

	auto extract_codec_config(
		gpu::video_codec codec,
		std::span<const std::byte> keyframe_bytes
	) -> std::optional<std::vector<std::byte>>;

	auto sample_flags_for(
		bool keyframe
	) -> std::uint32_t;

	auto compute_sample_durations(
		std::span<const gpu::encoded_unit> samples,
		std::uint32_t trailing_default
	) -> std::vector<std::uint32_t>;
}

gse::renderer::capture::mp4::box_scope::box_scope(std::vector<std::byte>& out, const std::array<char, 4> type)
	: m_out(out), m_length_offset(out.size()) {
	push_u32_be(m_out, 0);
	push_fourcc(m_out, type);
}

gse::renderer::capture::mp4::box_scope::~box_scope() {
	const auto size = m_out.size() - m_length_offset;
	const auto v = static_cast<std::uint32_t>(size);
	m_out[m_length_offset + 0] = std::byte{ static_cast<std::uint8_t>(v >> 24) };
	m_out[m_length_offset + 1] = std::byte{ static_cast<std::uint8_t>(v >> 16) };
	m_out[m_length_offset + 2] = std::byte{ static_cast<std::uint8_t>(v >> 8) };
	m_out[m_length_offset + 3] = std::byte{ static_cast<std::uint8_t>(v) };
}

auto gse::renderer::capture::mp4::fourcc(const char a, const char b, const char c, const char d) -> std::array<char, 4> {
	return { a, b, c, d };
}

auto gse::renderer::capture::mp4::push_u8(std::vector<std::byte>& out, const std::uint8_t v) -> void {
	out.push_back(std::byte{ v });
}

auto gse::renderer::capture::mp4::push_u16_be(std::vector<std::byte>& out, const std::uint16_t v) -> void {
	out.push_back(std::byte{ static_cast<std::uint8_t>(v >> 8) });
	out.push_back(std::byte{ static_cast<std::uint8_t>(v) });
}

auto gse::renderer::capture::mp4::push_u32_be(std::vector<std::byte>& out, const std::uint32_t v) -> void {
	out.push_back(std::byte{ static_cast<std::uint8_t>(v >> 24) });
	out.push_back(std::byte{ static_cast<std::uint8_t>(v >> 16) });
	out.push_back(std::byte{ static_cast<std::uint8_t>(v >> 8) });
	out.push_back(std::byte{ static_cast<std::uint8_t>(v) });
}

auto gse::renderer::capture::mp4::push_u64_be(std::vector<std::byte>& out, const std::uint64_t v) -> void {
	for (int i = 7; i >= 0; --i) {
		out.push_back(std::byte{ static_cast<std::uint8_t>(v >> (i * 8)) });
	}
}

auto gse::renderer::capture::mp4::push_bytes(std::vector<std::byte>& out, std::span<const std::byte> src) -> void {
	out.insert(out.end(), src.begin(), src.end());
}

auto gse::renderer::capture::mp4::push_fourcc(std::vector<std::byte>& out, const std::array<char, 4>& cc) -> void {
	for (const auto c : cc) {
		out.push_back(std::byte{ static_cast<std::uint8_t>(c) });
	}
}

auto gse::renderer::capture::mp4::pts_to_timescale(const time pts) -> std::uint64_t {
	const double us = static_cast<double>(pts.as<microseconds>());
	return static_cast<std::uint64_t>(us < 0.0 ? 0.0 : us);
}

auto gse::renderer::capture::mp4::read_leb128(std::span<const std::byte> data, std::size_t offset) -> std::optional<leb128_read> {
	std::uint64_t value = 0;
	std::size_t consumed = 0;
	for (std::size_t i = 0; i < 8; ++i) {
		if (offset + i >= data.size()) {
			return std::nullopt;
		}
		const auto byte = static_cast<std::uint8_t>(data[offset + i]);
		value |= static_cast<std::uint64_t>(byte & 0x7F) << (i * 7);
		++consumed;
		if ((byte & 0x80) == 0) {
			return leb128_read{ value, consumed };
		}
	}
	return std::nullopt;
}

auto gse::renderer::capture::mp4::split_av1_obus(std::span<const std::byte> bitstream) -> std::vector<av1_obu> {
	std::vector<av1_obu> obus;
	std::size_t i = 0;
	while (i < bitstream.size()) {
		const auto header = static_cast<std::uint8_t>(bitstream[i]);
		const auto type = static_cast<std::uint8_t>((header >> 3) & 0x0F);
		const auto extension_flag = (header >> 2) & 0x01;
		const auto has_size = (header >> 1) & 0x01;
		std::size_t pos = i + 1;
		if (extension_flag) {
			++pos;
		}
		std::uint64_t payload_size = 0;
		std::size_t size_bytes = 0;
		if (has_size) {
			const auto leb = read_leb128(bitstream, pos);
			if (!leb) {
				break;
			}
			payload_size = leb->value;
			size_bytes = leb->bytes_consumed;
		}
		else {
			payload_size = bitstream.size() - pos;
		}
		const auto header_bytes = pos - i + size_bytes;
		const auto total = header_bytes + payload_size;
		if (i + total > bitstream.size()) {
			break;
		}
		obus.push_back({ type, bitstream.subspan(i, total) });
		i += total;
	}
	return obus;
}

auto gse::renderer::capture::mp4::find_sequence_header_obu(std::span<const std::byte> bitstream) -> std::optional<std::span<const std::byte>> {
	for (const auto& obu : split_av1_obus(bitstream)) {
		if (obu.type == av1_obu_sequence_header) {
			return obu.full;
		}
	}
	return std::nullopt;
}

auto gse::renderer::capture::mp4::build_av1c(std::span<const std::byte> sequence_header_obu) -> std::vector<std::byte> {
	struct av1_seq_info {
		std::uint8_t seq_profile = 0;
		std::uint8_t seq_level_idx_0 = 0;
		std::uint8_t seq_tier_0 = 0;
		std::uint8_t high_bitdepth = 0;
		std::uint8_t twelve_bit = 0;
		std::uint8_t monochrome = 0;
		std::uint8_t chroma_subsampling_x = 1;
		std::uint8_t chroma_subsampling_y = 1;
		std::uint8_t chroma_sample_position = 0;
	};

	av1_seq_info info;

	if (sequence_header_obu.size() >= 2) {
		const auto obu_header = static_cast<std::uint8_t>(sequence_header_obu[0]);
		const auto has_size = (obu_header >> 1) & 0x01;
		std::size_t payload_offset = 1;
		if (has_size) {
			const auto leb = read_leb128(sequence_header_obu, 1);
			if (leb) {
				payload_offset = 1 + leb->bytes_consumed;
			}
		}

		const auto payload_bytes = sequence_header_obu.subspan(payload_offset);

		std::size_t bit_pos = 0;
		const auto read_bits = [&](const int n) -> std::uint32_t {
			std::uint32_t v = 0;
			for (int i = 0; i < n; ++i) {
				const auto byte_idx = bit_pos / 8;
				const auto bit_idx = 7 - (bit_pos % 8);
				if (byte_idx >= payload_bytes.size()) {
					return v;
				}
				const auto bit = (static_cast<std::uint8_t>(payload_bytes[byte_idx]) >> bit_idx) & 1u;
				v = (v << 1) | bit;
				++bit_pos;
			}
			return v;
		};

		info.seq_profile = static_cast<std::uint8_t>(read_bits(3));
		(void)read_bits(1);
		const auto reduced_still = read_bits(1);

		if (reduced_still) {
			info.seq_level_idx_0 = static_cast<std::uint8_t>(read_bits(5));
		}
		else {
			const auto timing_present = read_bits(1);
			if (timing_present == 0) {
				(void)read_bits(1);
				(void)read_bits(5);
				(void)read_bits(12);
				info.seq_level_idx_0 = static_cast<std::uint8_t>(read_bits(5));
				if (info.seq_level_idx_0 > 7) {
					info.seq_tier_0 = static_cast<std::uint8_t>(read_bits(1));
				}
			}
		}
	}

	std::vector<std::byte> result;
	result.reserve(4 + sequence_header_obu.size());
	push_u8(result, 0x81);
	push_u8(result, static_cast<std::uint8_t>((info.seq_profile << 5) | (info.seq_level_idx_0 & 0x1Fu)));
	push_u8(
		result,
		static_cast<std::uint8_t>((info.seq_tier_0 << 7) | (info.high_bitdepth << 6) | (info.twelve_bit << 5) | (info.monochrome << 4) | (info.chroma_subsampling_x << 3) | (info.chroma_subsampling_y << 2) | (info.chroma_sample_position & 0x03u))
	);
	push_u8(result, 0x00);
	push_bytes(result, sequence_header_obu);
	return result;
}

auto gse::renderer::capture::mp4::split_h265_nalus(std::span<const std::byte> bitstream) -> std::vector<h265_nalu> {
	std::vector<h265_nalu> nalus;

	const auto find_start = [&](const std::size_t from) -> std::optional<std::pair<std::size_t, std::size_t>> {
		for (std::size_t i = from; i + 3 < bitstream.size(); ++i) {
			if (bitstream[i] == std::byte{ 0x00 } && bitstream[i + 1] == std::byte{ 0x00 }) {
				if (bitstream[i + 2] == std::byte{ 0x01 }) {
					return std::make_pair(i, i + 3);
				}
				if (bitstream[i + 2] == std::byte{ 0x00 } && bitstream[i + 3] == std::byte{ 0x01 }) {
					return std::make_pair(i, i + 4);
				}
			}
		}
		return std::nullopt;
	};

	auto current = find_start(0);
	while (current) {
		const auto payload_start = current->second;
		const auto next = find_start(payload_start);
		const auto payload_end = next ? next->first : bitstream.size();
		if (payload_start >= bitstream.size() || payload_start >= payload_end) {
			break;
		}
		const auto header = static_cast<std::uint8_t>(bitstream[payload_start]);
		const auto type = static_cast<std::uint8_t>((header >> 1) & 0x3F);
		nalus.push_back({ type, bitstream.subspan(payload_start, payload_end - payload_start) });
		current = next;
	}

	return nalus;
}

auto gse::renderer::capture::mp4::collect_h265_parameter_sets(std::span<const std::byte> bitstream) -> h265_parameter_sets {
	h265_parameter_sets sets;
	for (const auto& nalu : split_h265_nalus(bitstream)) {
		std::vector<std::byte> copy(nalu.payload.begin(), nalu.payload.end());
		if (nalu.type == h265_nal_vps) {
			sets.vps.push_back(std::move(copy));
		}
		else if (nalu.type == h265_nal_sps) {
			sets.sps.push_back(std::move(copy));
		}
		else if (nalu.type == h265_nal_pps) {
			sets.pps.push_back(std::move(copy));
		}
	}
	return sets;
}

auto gse::renderer::capture::mp4::build_hvcc(const h265_parameter_sets& sets) -> std::vector<std::byte> {
	std::vector<std::byte> payload;
	push_u8(payload, 0x01);
	push_u8(payload, 0x01);
	push_u32_be(payload, 0x60000000);
	push_u32_be(payload, 0xB0000000);
	push_u16_be(payload, 0x0000);
	push_u8(payload, 0x5D);
	push_u16_be(payload, 0xF000);
	push_u8(payload, 0xFC);
	push_u8(payload, 0xFC);
	push_u8(payload, 0xF8);
	push_u8(payload, 0xF8);
	push_u16_be(payload, 0x0000);
	push_u8(payload, 0x0F);

	std::uint8_t array_count = 0;
	if (!sets.vps.empty()) {
		++array_count;
	}
	if (!sets.sps.empty()) {
		++array_count;
	}
	if (!sets.pps.empty()) {
		++array_count;
	}
	push_u8(payload, array_count);

	const auto emit_array = [&](const std::uint8_t nal_type, const std::vector<std::vector<std::byte>>& units) {
		if (units.empty()) {
			return;
		}
		push_u8(payload, static_cast<std::uint8_t>(0x80 | nal_type));
		push_u16_be(payload, static_cast<std::uint16_t>(units.size()));
		for (const auto& unit : units) {
			push_u16_be(payload, static_cast<std::uint16_t>(unit.size()));
			push_bytes(payload, unit);
		}
	};

	emit_array(h265_nal_vps, sets.vps);
	emit_array(h265_nal_sps, sets.sps);
	emit_array(h265_nal_pps, sets.pps);

	return payload;
}

auto gse::renderer::capture::mp4::emit_ftyp(std::vector<std::byte>& out, const gpu::video_codec codec) -> void {
	box_scope ftyp(out, fourcc('f', 't', 'y', 'p'));
	push_fourcc(out, fourcc('i', 's', 'o', 'm'));
	push_u32_be(out, 0x200);
	push_fourcc(out, fourcc('i', 's', 'o', 'm'));
	push_fourcc(out, fourcc('i', 's', 'o', '6'));
	push_fourcc(out, fourcc('m', 'p', '4', '1'));
	if (codec == gpu::video_codec::av1) {
		push_fourcc(out, fourcc('a', 'v', '0', '1'));
	}
	else {
		push_fourcc(out, fourcc('h', 'v', 'c', '1'));
	}
}

auto gse::renderer::capture::mp4::emit_mvhd(std::vector<std::byte>& out, const std::uint64_t duration) -> void {
	box_scope mvhd(out, fourcc('m', 'v', 'h', 'd'));
	push_u32_be(out, 0x01000000);
	push_u64_be(out, 0);
	push_u64_be(out, 0);
	push_u32_be(out, timescale);
	push_u64_be(out, duration);
	push_u32_be(out, 0x00010000);
	push_u16_be(out, 0x0100);
	push_u16_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x40000000);
	for (int i = 0; i < 6; ++i) {
		push_u32_be(out, 0);
	}
	push_u32_be(out, 2);
}

auto gse::renderer::capture::mp4::emit_tkhd(std::vector<std::byte>& out, const std::uint64_t duration, const vec2u extent) -> void {
	box_scope tkhd(out, fourcc('t', 'k', 'h', 'd'));
	push_u32_be(out, 0x01000007);
	push_u64_be(out, 0);
	push_u64_be(out, 0);
	push_u32_be(out, 1);
	push_u32_be(out, 0);
	push_u64_be(out, duration);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x40000000);
	push_u32_be(out, extent.x() << 16);
	push_u32_be(out, extent.y() << 16);
}

auto gse::renderer::capture::mp4::emit_elst(std::vector<std::byte>& out, const std::uint64_t duration, const std::int64_t media_start) -> void {
	box_scope edts(out, fourcc('e', 'd', 't', 's'));
	box_scope elst(out, fourcc('e', 'l', 's', 't'));
	push_u32_be(out, 0x01000000);
	push_u32_be(out, 1);
	push_u64_be(out, duration);
	push_u64_be(out, static_cast<std::uint64_t>(media_start));
	push_u16_be(out, 1);
	push_u16_be(out, 0);
}

auto gse::renderer::capture::mp4::emit_mdhd(std::vector<std::byte>& out, const std::uint64_t duration) -> void {
	box_scope mdhd(out, fourcc('m', 'd', 'h', 'd'));
	push_u32_be(out, 0x01000000);
	push_u64_be(out, 0);
	push_u64_be(out, 0);
	push_u32_be(out, timescale);
	push_u64_be(out, duration);
	push_u16_be(out, 0x55C4);
	push_u16_be(out, 0);
}

auto gse::renderer::capture::mp4::emit_hdlr(std::vector<std::byte>& out) -> void {
	box_scope hdlr(out, fourcc('h', 'd', 'l', 'r'));
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_fourcc(out, fourcc('v', 'i', 'd', 'e'));
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	const std::string_view name = "VideoHandler";
	for (const char c : name) {
		push_u8(out, static_cast<std::uint8_t>(c));
	}
	push_u8(out, 0);
}

auto gse::renderer::capture::mp4::emit_vmhd(std::vector<std::byte>& out) -> void {
	box_scope vmhd(out, fourcc('v', 'm', 'h', 'd'));
	push_u32_be(out, 0x00000001);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
}

auto gse::renderer::capture::mp4::emit_dinf(std::vector<std::byte>& out) -> void {
	box_scope dinf(out, fourcc('d', 'i', 'n', 'f'));
	box_scope dref(out, fourcc('d', 'r', 'e', 'f'));
	push_u32_be(out, 0);
	push_u32_be(out, 1);
	box_scope url_(out, fourcc('u', 'r', 'l', ' '));
	push_u32_be(out, 0x00000001);
}

auto gse::renderer::capture::mp4::emit_stsd(std::vector<std::byte>& out, const vec2u extent, const gpu::video_codec codec, std::span<const std::byte> codec_config) -> void {
	box_scope stsd(out, fourcc('s', 't', 's', 'd'));
	push_u32_be(out, 0);
	push_u32_be(out, 1);

	const auto sample_entry_type = codec == gpu::video_codec::av1 ? fourcc('a', 'v', '0', '1') : fourcc('h', 'v', 'c', '1');

	box_scope visual(out, sample_entry_type);
	for (int i = 0; i < 6; ++i) {
		push_u8(out, 0);
	}
	push_u16_be(out, 1);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	for (int i = 0; i < 3; ++i) {
		push_u32_be(out, 0);
	}
	push_u16_be(out, static_cast<std::uint16_t>(extent.x()));
	push_u16_be(out, static_cast<std::uint16_t>(extent.y()));
	push_u32_be(out, 0x00480000);
	push_u32_be(out, 0x00480000);
	push_u32_be(out, 0);
	push_u16_be(out, 1);
	for (int i = 0; i < 32; ++i) {
		push_u8(out, 0);
	}
	push_u16_be(out, 0x0018);
	push_u16_be(out, 0xFFFF);

	const auto config_type = codec == gpu::video_codec::av1 ? fourcc('a', 'v', '1', 'C') : fourcc('h', 'v', 'c', 'C');
	box_scope config(out, config_type);
	push_bytes(out, codec_config);
}

auto gse::renderer::capture::mp4::emit_stts(std::vector<std::byte>& out, const std::vector<std::uint32_t>& sample_durations) -> void {
	box_scope stts(out, fourcc('s', 't', 't', 's'));
	push_u32_be(out, 0);

	std::vector<std::pair<std::uint32_t, std::uint32_t>> runs;
	for (const auto d : sample_durations) {
		if (!runs.empty() && runs.back().second == d) {
			++runs.back().first;
		}
		else {
			runs.emplace_back(1u, d);
		}
	}

	push_u32_be(out, static_cast<std::uint32_t>(runs.size()));
	for (const auto& [count, duration] : runs) {
		push_u32_be(out, count);
		push_u32_be(out, duration);
	}
}

auto gse::renderer::capture::mp4::emit_stss(std::vector<std::byte>& out, std::span<const gpu::encoded_unit> units) -> void {
	std::vector<std::uint32_t> keyframe_indices;
	for (std::size_t i = 0; i < units.size(); ++i) {
		if (units[i].keyframe) {
			keyframe_indices.push_back(static_cast<std::uint32_t>(i + 1));
		}
	}
	if (keyframe_indices.empty()) {
		return;
	}
	box_scope stss(out, fourcc('s', 't', 's', 's'));
	push_u32_be(out, 0);
	push_u32_be(out, static_cast<std::uint32_t>(keyframe_indices.size()));
	for (const auto idx : keyframe_indices) {
		push_u32_be(out, idx);
	}
}

auto gse::renderer::capture::mp4::emit_stsc(std::vector<std::byte>& out, const std::uint32_t sample_count) -> void {
	box_scope stsc(out, fourcc('s', 't', 's', 'c'));
	push_u32_be(out, 0);
	push_u32_be(out, 1);
	push_u32_be(out, 1);
	push_u32_be(out, sample_count);
	push_u32_be(out, 1);
}

auto gse::renderer::capture::mp4::emit_stsz(std::vector<std::byte>& out, std::span<const gpu::encoded_unit> units) -> void {
	box_scope stsz(out, fourcc('s', 't', 's', 'z'));
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, static_cast<std::uint32_t>(units.size()));
	for (const auto& u : units) {
		push_u32_be(out, static_cast<std::uint32_t>(u.bytes.size()));
	}
}

auto gse::renderer::capture::mp4::emit_co64(std::vector<std::byte>& out, const std::uint64_t mdat_payload_offset) -> void {
	box_scope co64(out, fourcc('c', 'o', '6', '4'));
	push_u32_be(out, 0);
	push_u32_be(out, 1);
	push_u64_be(out, mdat_payload_offset);
}

auto gse::renderer::capture::mp4::rewrite_co64_offset(std::vector<std::byte>& moov, const std::uint64_t payload_offset) -> void {
	const std::array<std::byte, 4> marker = {
		std::byte{ 'c' },
		std::byte{ 'o' },
		std::byte{ '6' },
		std::byte{ '4' },
	};
	for (std::size_t i = 0; i + 4 <= moov.size(); ++i) {
		if (moov[i] == marker[0] && moov[i + 1] == marker[1] && moov[i + 2] == marker[2] && moov[i + 3] == marker[3]) {
			const auto entry = i + 4 + 4 + 4;
			if (entry + 8 <= moov.size()) {
				moov[entry + 0] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 56) };
				moov[entry + 1] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 48) };
				moov[entry + 2] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 40) };
				moov[entry + 3] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 32) };
				moov[entry + 4] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 24) };
				moov[entry + 5] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 16) };
				moov[entry + 6] = std::byte{ static_cast<std::uint8_t>(payload_offset >> 8) };
				moov[entry + 7] = std::byte{ static_cast<std::uint8_t>(payload_offset) };
			}
			return;
		}
	}
}

auto gse::renderer::capture::mp4::mux(const std::span<const gpu::encoded_unit> units, const track_info& track, std::span<const std::byte> stream_header, const std::filesystem::path& out) -> bool {
	if (units.empty()) {
		log::println(log::level::warning, log::category::render, "mp4::mux called with empty units");
		return false;
	}

	if (!units.front().keyframe) {
		log::println(log::level::warning, log::category::render, "mp4::mux snapshot does not begin with a keyframe");
		return false;
	}

	const auto codec_config_opt = extract_codec_config(track.codec, stream_header);
	if (!codec_config_opt) {
		log::println(log::level::warning, log::category::render, "mp4::mux stream_header missing codec parameters");
		return false;
	}
	const auto& codec_config = *codec_config_opt;

	std::vector<std::uint32_t> sample_durations;
	sample_durations.reserve(units.size());
	for (std::size_t i = 0; i + 1 < units.size(); ++i) {
		const auto delta = units[i + 1].pts - units[i].pts;
		const double us = static_cast<double>(delta.as<microseconds>());
		const auto ticks = us < 1.0 ? 1u : static_cast<std::uint32_t>(us);
		sample_durations.push_back(ticks);
	}
	const auto trailing = sample_durations.empty() ? static_cast<std::uint32_t>(timescale / 60) : sample_durations.back();
	sample_durations.push_back(trailing);

	std::uint64_t media_duration = 0;
	for (const auto d : sample_durations) {
		media_duration += d;
	}

	const auto first_pts_us = pts_to_timescale(units.front().pts);

	std::vector<std::byte> moov_bytes;
	{
		box_scope moov(moov_bytes, fourcc('m', 'o', 'o', 'v'));
		emit_mvhd(moov_bytes, media_duration);
		{
			box_scope trak(moov_bytes, fourcc('t', 'r', 'a', 'k'));
			emit_tkhd(moov_bytes, media_duration, track.extent);
			emit_elst(moov_bytes, media_duration, static_cast<std::int64_t>(first_pts_us));
			{
				box_scope mdia(moov_bytes, fourcc('m', 'd', 'i', 'a'));
				emit_mdhd(moov_bytes, media_duration);
				emit_hdlr(moov_bytes);
				{
					box_scope minf(moov_bytes, fourcc('m', 'i', 'n', 'f'));
					emit_vmhd(moov_bytes);
					emit_dinf(moov_bytes);
					{
						box_scope stbl(moov_bytes, fourcc('s', 't', 'b', 'l'));
						emit_stsd(moov_bytes, track.extent, track.codec, codec_config);
						emit_stts(moov_bytes, sample_durations);
						emit_stss(moov_bytes, units);
						emit_stsc(moov_bytes, static_cast<std::uint32_t>(units.size()));
						emit_stsz(moov_bytes, units);
						emit_co64(moov_bytes, 0);
					}
				}
			}
		}
	}

	std::vector<std::byte> ftyp_bytes;
	emit_ftyp(ftyp_bytes, track.codec);

	std::size_t mdat_size = 16;
	for (const auto& u : units) {
		mdat_size += u.bytes.size();
	}

	const std::uint64_t mdat_header_offset = ftyp_bytes.size() + moov_bytes.size();
	const std::uint64_t mdat_payload_offset = mdat_header_offset + 16;

	rewrite_co64_offset(moov_bytes, mdat_payload_offset);

	std::ofstream file(out, std::ios::binary);
	if (!file) {
		log::println(log::level::warning, log::category::render, "mp4::mux could not open output file {}",
					 out.string());
		return false;
	}

	file.write(reinterpret_cast<const char*>(ftyp_bytes.data()), static_cast<std::streamsize>(ftyp_bytes.size()));
	file.write(reinterpret_cast<const char*>(moov_bytes.data()), static_cast<std::streamsize>(moov_bytes.size()));

	std::array<std::byte, 16> mdat_header{};
	mdat_header[0] = std::byte{ 0 };
	mdat_header[1] = std::byte{ 0 };
	mdat_header[2] = std::byte{ 0 };
	mdat_header[3] = std::byte{ 1 };
	mdat_header[4] = std::byte{ 'm' };
	mdat_header[5] = std::byte{ 'd' };
	mdat_header[6] = std::byte{ 'a' };
	mdat_header[7] = std::byte{ 't' };
	const auto mdat_total = static_cast<std::uint64_t>(mdat_size);
	for (int i = 0; i < 8; ++i) {
		mdat_header[8 + i] = std::byte{ static_cast<std::uint8_t>(mdat_total >> ((7 - i) * 8)) };
	}
	file.write(reinterpret_cast<const char*>(mdat_header.data()), static_cast<std::streamsize>(mdat_header.size()));

	for (const auto& u : units) {
		file.write(reinterpret_cast<const char*>(u.bytes.data()), static_cast<std::streamsize>(u.bytes.size()));
	}

	return static_cast<bool>(file);
}

auto gse::renderer::capture::mp4::sample_flags_for(const bool keyframe) -> std::uint32_t {
	if (keyframe) {
		return 0x02000000u;
	}
	return 0x01010000u;
}

auto gse::renderer::capture::mp4::compute_sample_durations(std::span<const gpu::encoded_unit> samples, const std::uint32_t trailing_default) -> std::vector<std::uint32_t> {
	std::vector<std::uint32_t> durations;
	durations.reserve(samples.size());
	for (std::size_t i = 0; i + 1 < samples.size(); ++i) {
		const auto delta = samples[i + 1].pts - samples[i].pts;
		const double us = static_cast<double>(delta.as<microseconds>());
		const auto ticks = us < 1.0 ? 1u : static_cast<std::uint32_t>(us);
		durations.push_back(ticks);
	}
	const auto trailing = durations.empty() ? trailing_default : durations.back();
	durations.push_back(trailing);
	return durations;
}

auto gse::renderer::capture::mp4::extract_codec_config(const gpu::video_codec codec, std::span<const std::byte> keyframe_bytes) -> std::optional<std::vector<std::byte>> {
	if (codec == gpu::video_codec::av1) {
		const auto seq = find_sequence_header_obu(keyframe_bytes);
		if (!seq) {
			log::println(
				log::level::warning,
				log::category::render,
				"live_muxer could not locate AV1 sequence header OBU in first keyframe"
			);
			return std::nullopt;
		}
		return build_av1c(*seq);
	}
	const auto sets = collect_h265_parameter_sets(keyframe_bytes);
	if (sets.sps.empty() || sets.pps.empty()) {
		log::println(
			log::level::warning,
			log::category::render,
			"live_muxer could not locate H.265 SPS/PPS in first keyframe"
		);
		return std::nullopt;
	}
	return build_hvcc(sets);
}

auto gse::renderer::capture::mp4::emit_ftyp_fragmented(std::vector<std::byte>& out, const gpu::video_codec codec) -> void {
	box_scope ftyp(out, fourcc('f', 't', 'y', 'p'));
	push_fourcc(out, fourcc('i', 's', 'o', '6'));
	push_u32_be(out, 0x200);
	push_fourcc(out, fourcc('i', 's', 'o', 'm'));
	push_fourcc(out, fourcc('i', 's', 'o', '6'));
	push_fourcc(out, fourcc('m', 's', 'd', 'h'));
	push_fourcc(out, fourcc('m', 's', 'i', 'x'));
	if (codec == gpu::video_codec::av1) {
		push_fourcc(out, fourcc('a', 'v', '0', '1'));
	}
	else {
		push_fourcc(out, fourcc('h', 'v', 'c', '1'));
	}
}

auto gse::renderer::capture::mp4::emit_mvhd_unknown(std::vector<std::byte>& out, std::size_t& duration_offset_out) -> void {
	box_scope mvhd(out, fourcc('m', 'v', 'h', 'd'));
	push_u32_be(out, 0x01000000);
	push_u64_be(out, 0);
	push_u64_be(out, 0);
	push_u32_be(out, timescale);
	duration_offset_out = out.size();
	push_u64_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u16_be(out, 0x0100);
	push_u16_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x40000000);
	for (int i = 0; i < 6; ++i) {
		push_u32_be(out, 0);
	}
	push_u32_be(out, 2);
}

auto gse::renderer::capture::mp4::emit_tkhd_unknown(std::vector<std::byte>& out, const vec2u extent, std::size_t& duration_offset_out) -> void {
	box_scope tkhd(out, fourcc('t', 'k', 'h', 'd'));
	push_u32_be(out, 0x01000007);
	push_u64_be(out, 0);
	push_u64_be(out, 0);
	push_u32_be(out, 1);
	push_u32_be(out, 0);
	duration_offset_out = out.size();
	push_u64_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u16_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x00010000);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0x40000000);
	push_u32_be(out, extent.x() << 16);
	push_u32_be(out, extent.y() << 16);
}

auto gse::renderer::capture::mp4::emit_mdhd_unknown(std::vector<std::byte>& out, std::size_t& duration_offset_out) -> void {
	box_scope mdhd(out, fourcc('m', 'd', 'h', 'd'));
	push_u32_be(out, 0x01000000);
	push_u64_be(out, 0);
	push_u64_be(out, 0);
	push_u32_be(out, timescale);
	duration_offset_out = out.size();
	push_u64_be(out, 0);
	push_u16_be(out, 0x55C4);
	push_u16_be(out, 0);
}

auto gse::renderer::capture::mp4::emit_stbl_empty(std::vector<std::byte>& out, const vec2u extent, const gpu::video_codec codec, std::span<const std::byte> codec_config) -> void {
	box_scope stbl(out, fourcc('s', 't', 'b', 'l'));
	emit_stsd(out, extent, codec, codec_config);
	{
		box_scope stts(out, fourcc('s', 't', 't', 's'));
		push_u32_be(out, 0);
		push_u32_be(out, 0);
	}
	{
		box_scope stsc(out, fourcc('s', 't', 's', 'c'));
		push_u32_be(out, 0);
		push_u32_be(out, 0);
	}
	{
		box_scope stsz(out, fourcc('s', 't', 's', 'z'));
		push_u32_be(out, 0);
		push_u32_be(out, 0);
		push_u32_be(out, 0);
	}
	{
		box_scope stco(out, fourcc('s', 't', 'c', 'o'));
		push_u32_be(out, 0);
		push_u32_be(out, 0);
	}
}

auto gse::renderer::capture::mp4::emit_mvex_trex(std::vector<std::byte>& out) -> void {
	box_scope mvex(out, fourcc('m', 'v', 'e', 'x'));
	box_scope trex(out, fourcc('t', 'r', 'e', 'x'));
	push_u32_be(out, 0);
	push_u32_be(out, 1);
	push_u32_be(out, 1);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
	push_u32_be(out, 0);
}

auto gse::renderer::capture::mp4::emit_fragment_moof(std::vector<std::byte>& out, const std::uint32_t sequence, const std::uint64_t base_decode_time, std::span<const gpu::encoded_unit> samples, std::span<const std::uint32_t> durations, const std::uint32_t per_sample_prefix_bytes, std::size_t& trun_data_offset_field) -> void {
	box_scope moof(out, fourcc('m', 'o', 'o', 'f'));
	{
		box_scope mfhd(out, fourcc('m', 'f', 'h', 'd'));
		push_u32_be(out, 0);
		push_u32_be(out, sequence);
	}
	{
		box_scope traf(out, fourcc('t', 'r', 'a', 'f'));
		{
			box_scope tfhd(out, fourcc('t', 'f', 'h', 'd'));
			push_u32_be(out, 0x00020000);
			push_u32_be(out, 1);
		}
		{
			box_scope tfdt(out, fourcc('t', 'f', 'd', 't'));
			push_u32_be(out, 0x01000000);
			push_u64_be(out, base_decode_time);
		}
		{
			box_scope trun(out, fourcc('t', 'r', 'u', 'n'));
			push_u32_be(out, 0x00000701);
			push_u32_be(out, static_cast<std::uint32_t>(samples.size()));
			trun_data_offset_field = out.size();
			push_u32_be(out, 0);
			for (std::size_t i = 0; i < samples.size(); ++i) {
				push_u32_be(out, durations[i]);
				push_u32_be(out, static_cast<std::uint32_t>(samples[i].bytes.size()) + per_sample_prefix_bytes);
				push_u32_be(out, sample_flags_for(samples[i].keyframe));
			}
		}
	}
}

gse::renderer::capture::mp4::live_muxer::~live_muxer() {
	close();
}

auto gse::renderer::capture::mp4::live_muxer::open(const std::filesystem::path& path, const track_info& track, std::span<const std::byte> stream_header) -> std::optional<live_muxer> {
	if (stream_header.empty()) {
		log::println(log::level::warning, log::category::render, "live_muxer::open called with empty stream_header");
		return std::nullopt;
	}

	live_muxer m;
	m.m_file.open(path, std::ios::binary);
	if (!m.m_file) {
		log::println(log::level::warning,
					 log::category::render,
					 "live_muxer::open could not open file {}",
					 path.string());
		return std::nullopt;
	}
	m.m_path = path;
	m.m_track = track;

	if (!m.write_init(stream_header)) {
		m.m_file.close();
		return std::nullopt;
	}

	return m;
}

auto gse::renderer::capture::mp4::live_muxer::valid() const -> bool {
	return m_file.is_open() && m_file.good();
}

auto gse::renderer::capture::mp4::live_muxer::frame_count() const -> std::size_t {
	return m_frames_total;
}

auto gse::renderer::capture::mp4::live_muxer::append(gpu::encoded_unit unit) -> void {
	if (!valid()) {
		return;
	}

	if (!m_first_keyframe_seen) {
		if (!unit.keyframe) {
			return;
		}
		m_first_keyframe_seen = true;
		log::println(log::category::render, "live_muxer::append first keyframe ({} bytes)", unit.bytes.size());
	}

	if (unit.keyframe && !m_pending.empty()) {
		flush_fragment();
	}

	m_pending.push_back(std::move(unit));
	++m_frames_total;
}

auto gse::renderer::capture::mp4::live_muxer::close() -> void {
	if (!m_pending.empty()) {
		flush_fragment();
	}
	if (m_init_written && m_sequence > 0 && m_file.is_open()) {
		const auto write_u64_at = [&](const std::uint64_t offset, const std::uint64_t value) {
			m_file.seekp(static_cast<std::streamoff>(offset));
			std::array<std::byte, 8> bytes{};
			for (int i = 0; i < 8; ++i) {
				bytes[i] = std::byte{ static_cast<std::uint8_t>(value >> ((7 - i) * 8)) };
			}
			m_file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		};
		write_u64_at(m_mvhd_duration_offset, m_decode_time);
		write_u64_at(m_tkhd_duration_offset, m_decode_time);
		write_u64_at(m_mdhd_duration_offset, m_decode_time);
		log::println(
			log::category::render,
			"live_muxer: patched duration={}us into mvhd/tkhd/mdhd in {}",
			m_decode_time,
			m_path.string()
		);
	}
	if (m_file.is_open()) {
		m_file.close();
	}
	if (m_sequence == 0 && !m_path.empty()) {
		std::error_code ec;
		std::filesystem::remove(m_path, ec);
		log::println(
			log::category::render,
			"live_muxer: discarded empty file {} (no fragments written)",
			m_path.string()
		);
		m_path.clear();
	}
}

auto gse::renderer::capture::mp4::live_muxer::write_init(std::span<const std::byte> stream_header) -> bool {
	const auto codec_config = extract_codec_config(m_track.codec, stream_header);
	if (!codec_config) {
		return false;
	}

	std::vector<std::byte> ftyp_bytes;
	emit_ftyp_fragmented(ftyp_bytes, m_track.codec);

	std::vector<std::byte> moov_bytes;
	std::size_t mvhd_dur_offset = 0;
	std::size_t tkhd_dur_offset = 0;
	std::size_t mdhd_dur_offset = 0;
	{
		box_scope moov(moov_bytes, fourcc('m', 'o', 'o', 'v'));
		emit_mvhd_unknown(moov_bytes, mvhd_dur_offset);
		{
			box_scope trak(moov_bytes, fourcc('t', 'r', 'a', 'k'));
			emit_tkhd_unknown(moov_bytes, m_track.extent, tkhd_dur_offset);
			{
				box_scope mdia(moov_bytes, fourcc('m', 'd', 'i', 'a'));
				emit_mdhd_unknown(moov_bytes, mdhd_dur_offset);
				emit_hdlr(moov_bytes);
				{
					box_scope minf(moov_bytes, fourcc('m', 'i', 'n', 'f'));
					emit_vmhd(moov_bytes);
					emit_dinf(moov_bytes);
					emit_stbl_empty(moov_bytes, m_track.extent, m_track.codec, *codec_config);
				}
			}
		}
		emit_mvex_trex(moov_bytes);
	}

	m_mvhd_duration_offset = ftyp_bytes.size() + mvhd_dur_offset;
	m_tkhd_duration_offset = ftyp_bytes.size() + tkhd_dur_offset;
	m_mdhd_duration_offset = ftyp_bytes.size() + mdhd_dur_offset;

	m_file.write(reinterpret_cast<const char*>(ftyp_bytes.data()), static_cast<std::streamsize>(ftyp_bytes.size()));
	m_file.write(reinterpret_cast<const char*>(moov_bytes.data()), static_cast<std::streamsize>(moov_bytes.size()));

	if (!m_file) {
		log::println(log::level::warning, log::category::render, "live_muxer::write_init failed writing ftyp/moov");
		return false;
	}

	m_init_written = true;
	return true;
}

auto gse::renderer::capture::mp4::live_muxer::flush_fragment() -> void {
	if (m_pending.empty() || !m_file.is_open()) {
		return;
	}

	const auto durations = compute_sample_durations(m_pending, m_last_default_duration);
	if (durations.size() >= 2) {
		m_last_default_duration = durations[durations.size() - 2];
	}

	++m_sequence;

	const std::uint32_t per_sample_prefix = m_track.codec == gpu::video_codec::av1 ? 2u : 0u;

	std::vector<std::byte> moof_bytes;
	std::size_t data_offset_field = 0;
	emit_fragment_moof(moof_bytes, m_sequence, m_decode_time, m_pending, durations,
					   per_sample_prefix,
					   data_offset_field);

	std::uint32_t mdat_payload_size = 0;
	for (const auto& s : m_pending) {
		mdat_payload_size += static_cast<std::uint32_t>(s.bytes.size()) + per_sample_prefix;
	}
	const auto mdat_total = mdat_payload_size + 8u;

	const auto data_offset = static_cast<std::int32_t>(moof_bytes.size() + 8u);
	moof_bytes[data_offset_field + 0] = std::byte{ static_cast<std::uint8_t>(data_offset >> 24) };
	moof_bytes[data_offset_field + 1] = std::byte{ static_cast<std::uint8_t>(data_offset >> 16) };
	moof_bytes[data_offset_field + 2] = std::byte{ static_cast<std::uint8_t>(data_offset >> 8) };
	moof_bytes[data_offset_field + 3] = std::byte{ static_cast<std::uint8_t>(data_offset) };

	m_file.write(reinterpret_cast<const char*>(moof_bytes.data()), static_cast<std::streamsize>(moof_bytes.size()));

	std::array<std::byte, 8> mdat_header{};
	mdat_header[0] = std::byte{ static_cast<std::uint8_t>(mdat_total >> 24) };
	mdat_header[1] = std::byte{ static_cast<std::uint8_t>(mdat_total >> 16) };
	mdat_header[2] = std::byte{ static_cast<std::uint8_t>(mdat_total >> 8) };
	mdat_header[3] = std::byte{ static_cast<std::uint8_t>(mdat_total) };
	mdat_header[4] = std::byte{ 'm' };
	mdat_header[5] = std::byte{ 'd' };
	mdat_header[6] = std::byte{ 'a' };
	mdat_header[7] = std::byte{ 't' };
	m_file.write(reinterpret_cast<const char*>(mdat_header.data()), static_cast<std::streamsize>(mdat_header.size()));

	constexpr std::array<std::byte, 2> temporal_delimiter{ std::byte{ 0x12 }, std::byte{ 0x00 } };
	for (const auto& s : m_pending) {
		if (m_track.codec == gpu::video_codec::av1) {
			m_file.write(
				reinterpret_cast<const char*>(temporal_delimiter.data()),
				static_cast<std::streamsize>(temporal_delimiter.size())
			);
		}
		m_file.write(reinterpret_cast<const char*>(s.bytes.data()), static_cast<std::streamsize>(s.bytes.size()));
	}

	m_file.flush();

	for (const auto d : durations) {
		m_decode_time += d;
	}

	m_pending.clear();
}
