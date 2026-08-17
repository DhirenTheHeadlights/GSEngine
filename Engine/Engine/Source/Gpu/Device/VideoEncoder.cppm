export module gse.gpu:video_encoder;

import std;

import gse.gpu_backend;
import gse.core;
import gse.math;

export namespace gse::gpu {
	struct video_encoder_backend;

	class video_encoder final : public non_copyable {
	public:
		video_encoder() noexcept;
		~video_encoder();

		video_encoder(
			video_encoder&& other
		) noexcept;

		auto operator=(
			video_encoder&& other
		) noexcept -> video_encoder&;

		explicit video_encoder(
			std::unique_ptr<video_encoder_backend> impl
		) noexcept;

		[[nodiscard]] auto begin_capture(
			time pts
		) -> encode_source;

		[[nodiscard]] auto take_bitstream() -> std::optional<encoded_unit>;

		auto submit_ready() -> void;

		[[nodiscard]] auto stream_header() const -> std::span<const std::byte>;

		[[nodiscard]] auto codec() const -> video_codec;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto valid() const -> bool;

	private:
		std::unique_ptr<video_encoder_backend> m_impl;
	};
}
