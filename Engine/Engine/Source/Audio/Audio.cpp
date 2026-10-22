module gse.audio;

import std;

import gse.core;
import gse.log;
import gse.math;

auto gse::bake(const std::filesystem::path& src, audio_clip::baked& out) -> bool {
	std::ifstream file(src, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		log::println(log::level::warning, log::category::assets, "Failed to open audio source: {}", src.generic_display_string());
		return false;
	}
	const std::streamoff size = file.tellg();
	if (!std::in_range<std::size_t>(size)) {
		log::println(log::level::warning, log::category::assets, "Audio source {} is {} bytes, which does not fit this build's address space", src.generic_display_string(), size);
		return false;
	}
	file.seekg(0);
	out.bytes.storage.resize(static_cast<std::size_t>(size));
	file.read(reinterpret_cast<char*>(out.bytes.storage.data()), size);
	return true;
}

auto gse::audio_clip::data() const -> const std::vector<std::byte>& {
	return m_bytes;
}

auto gse::audio_clip::sample_rate() const -> std::uint32_t {
	return m_sample_rate;
}

auto gse::audio_clip::channels() const -> std::uint32_t {
	return m_channels;
}

auto gse::audio_clip::frame_count() const -> std::uint64_t {
	return m_frame_count;
}

auto gse::audio_clip::duration() const -> time_t<float, seconds> {
	return m_duration;
}
