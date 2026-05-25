export module gse.graphics:mp4_muxer;

import std;

import gse.math;
import gse.gpu;

export namespace gse::renderer::capture::mp4 {
	struct track_info {
		gpu::video_codec codec;
		vec2u extent;
	};

	[[nodiscard]]
	auto mux(
		std::span<const gpu::encoded_unit> units,
		const track_info& track,
		std::span<const std::byte> stream_header,
		const std::filesystem::path& out
	) -> bool;

	class live_muxer {
	public:
		live_muxer() = default;

		live_muxer(
			live_muxer&&
		) = default;

		auto operator=(
			live_muxer&&
		) -> live_muxer& = default;

		~live_muxer();

		[[nodiscard]] static auto open(
			const std::filesystem::path& path,
			const track_info& track,
			std::span<const std::byte> stream_header
		) -> std::optional<live_muxer>;

		auto append(
			gpu::encoded_unit unit
		) -> void;

		auto close() -> void;

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto frame_count() const -> std::size_t;

	private:
		std::ofstream m_file;
		std::filesystem::path m_path;
		track_info m_track{};
		std::uint32_t m_sequence = 0;
		std::uint64_t m_decode_time = 0;
		std::vector<gpu::encoded_unit> m_pending;
		std::uint32_t m_last_default_duration = 16'667;
		std::size_t m_frames_total = 0;
		bool m_init_written = false;
		bool m_first_keyframe_seen = false;
		std::uint64_t m_mvhd_duration_offset = 0;
		std::uint64_t m_tkhd_duration_offset = 0;
		std::uint64_t m_mdhd_duration_offset = 0;

		auto write_init(
			std::span<const std::byte> stream_header
		) -> bool;

		auto flush_fragment() -> void;
	};
}
