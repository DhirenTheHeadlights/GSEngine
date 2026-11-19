export module gse.audio:wav;

import std;

export namespace gse::audio {
	struct wav_layout {
		std::uint32_t channels = 0;
		std::uint32_t frame_rate = 0;
	};

	auto write_wav(
		const std::filesystem::path& path,
		std::span<const float> samples,
		wav_layout layout
	) -> std::expected<void, std::string>;
}
