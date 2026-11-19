module gse.audio:wav_impl;

import std;

import :wav;

namespace gse::audio {
	constexpr std::uint16_t wav_format_ieee_float = 3;

	struct wav_header {
		std::array<char, 4> riff{ 'R', 'I', 'F', 'F' };
		std::uint32_t riff_size = 0;
		std::array<char, 4> wave{ 'W', 'A', 'V', 'E' };
		std::array<char, 4> fmt{ 'f', 'm', 't', ' ' };
		std::uint32_t fmt_size = 16;
		std::uint16_t format = wav_format_ieee_float;
		std::uint16_t channels = 0;
		std::uint32_t frame_rate = 0;
		std::uint32_t byte_rate = 0;
		std::uint16_t block_align = 0;
		std::uint16_t bits_per_sample = 8 * sizeof(float);
		std::array<char, 4> data{ 'd', 'a', 't', 'a' };
		std::uint32_t data_size = 0;
	};

	static_assert(sizeof(wav_header) == 44);
}

auto gse::audio::write_wav(const std::filesystem::path& path, const std::span<const float> samples, const wav_layout layout) -> std::expected<void, std::string> {
	const std::size_t data_bytes = samples.size_bytes();
	if (!std::in_range<std::uint32_t>(data_bytes + sizeof(wav_header))) {
		return std::unexpected(std::format("{} bytes of samples do not fit a WAV file", data_bytes));
	}

	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		return std::unexpected(ec.message());
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		return std::unexpected(std::string("could not open the file for writing"));
	}

	const std::uint32_t frame_bytes = layout.channels * sizeof(float);
	const wav_header header{
		.riff_size = static_cast<std::uint32_t>(data_bytes + sizeof(wav_header) - 8),
		.channels = static_cast<std::uint16_t>(layout.channels),
		.frame_rate = layout.frame_rate,
		.byte_rate = layout.frame_rate * frame_bytes,
		.block_align = static_cast<std::uint16_t>(frame_bytes),
		.data_size = static_cast<std::uint32_t>(data_bytes),
	};

	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	file.write(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(data_bytes));
	if (!file.good()) {
		return std::unexpected(std::string("the write failed part way through"));
	}
	return {};
}
