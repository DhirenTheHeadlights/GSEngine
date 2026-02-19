module;

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef assert

export module gse.audio;

import std;

import gse.assert;
import gse.utility;
import gse.math;
import gse.platform;

export namespace gse {
	class audio_clip : public identifiable {
	public:
		explicit audio_clip(
			const std::filesystem::path& filepath
		);

		auto load(
			const gpu::context& context
		) -> void;

		auto unload(
		) -> void;

		auto data(
		) const -> const std::vector<std::byte>&;

		auto sample_rate(
		) const -> std::uint32_t;

		auto channels(
		) const -> std::uint32_t;

		auto frame_count(
		) const -> std::uint64_t;

		auto duration(
		) const -> time_t<float, seconds>;
	private:
		std::filesystem::path m_path;
		std::vector<std::byte> m_bytes;
		std::uint32_t m_sample_rate = 0;
		std::uint32_t m_channels = 0;
		std::uint64_t m_frame_count = 0;
		time_t<float, seconds> m_duration{};
	};
}

export template<>
struct gse::asset_compiler<gse::audio_clip> {
	static auto source_extensions() -> std::vector<std::string> {
		return { ".wav", ".mp3", ".ogg", ".flac" };
	}

	static auto baked_extension() -> std::string {
		return ".gaud";
	}

	static auto source_directory() -> std::string {
		return "Audio";
	}

	static auto baked_directory() -> std::string {
		return "Audio";
	}

	static auto compile_one(
		const std::filesystem::path& source,
		const std::filesystem::path& destination
	) -> bool {
		std::filesystem::create_directories(destination.parent_path());
		std::error_code ec;
		std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec) {
			std::println("Warning: Failed to compile audio '{}': {}", source.string(), ec.message());
			return false;
		}
		std::println("Audio compiled: {}", destination.filename().string());
		return true;
	}

	static auto needs_recompile(
		const std::filesystem::path& source,
		const std::filesystem::path& destination
	) -> bool {
		if (!std::filesystem::exists(destination)) {
			return true;
		}
		return std::filesystem::last_write_time(source) >
			   std::filesystem::last_write_time(destination);
	}

	static auto dependencies(
		const std::filesystem::path&
	) -> std::vector<std::filesystem::path> {
		return {};
	}
};

export namespace gse {
	struct voice_handle {
		std::uint32_t index = 0;
		std::uint32_t generation = 0;
	};
}

export namespace gse::audio {
	struct voice_slot {
		ma_decoder decoder{};
		ma_sound sound{};
		std::uint32_t generation = 0;
		bool active = false;
	};

	struct state {
		ma_engine engine{};
		bool engine_initialized = false;
		gpu::context* ctx = nullptr;

		std::vector<std::unique_ptr<voice_slot>> voices;
		std::vector<std::uint32_t> free_list;
		percentage<float> master_vol = percentage<float>::one();

		explicit state(gpu::context& c) : ctx(std::addressof(c)) {}
		state() = default;

		auto play(
			const audio_clip& clip,
			bool loop = false
		) -> voice_handle;

		auto stop(
			voice_handle handle
		) -> void;

		auto pause(
			voice_handle handle
		) const -> void;

		auto resume(
			voice_handle handle
		) const -> void;

		auto set_volume(
			voice_handle handle,
			percentage<float> vol
		) const -> void;

		auto set_master_volume(
			percentage<float> vol
		) -> void;

		auto master_volume(
		) const -> percentage<float>;

		auto cleanup_finished(
		) -> void;

		auto stop_all(
		) -> void;
	private:
		auto valid_voice(
			voice_handle handle
		) const -> bool;
	};

	struct system {
		static auto initialize(
			const initialize_phase& phase,
			state& s
		) -> void;

		static auto update(
			update_phase& phase,
			state& s
		) -> void;

		static auto shutdown(
			shutdown_phase& phase,
			state& s
		) -> void;
	};
}

gse::audio_clip::audio_clip(const std::filesystem::path& filepath) : identifiable(filepath, config::baked_resource_path), m_path(filepath) {}

auto gse::audio_clip::load(const gpu::context&) -> void {
	std::ifstream file(m_path, std::ios::binary | std::ios::ate);
	assert(
		file.is_open(),
		std::source_location::current(),
		"Failed to open baked audio file: {}",
		m_path.string()
	);

	const auto size = file.tellg();
	file.seekg(0);
	m_bytes.resize(size);
	file.read(reinterpret_cast<char*>(m_bytes.data()), size);

	const ma_decoder_config cfg = ma_decoder_config_init(ma_format_unknown, 0, 0);
	ma_decoder decoder;
	if (const auto result = ma_decoder_init_memory(m_bytes.data(), m_bytes.size(), &cfg, &decoder); result == MA_SUCCESS) {
		ma_uint64 length = 0;
		ma_decoder_get_length_in_pcm_frames(&decoder, &length);
		m_sample_rate = decoder.outputSampleRate;
		m_channels = decoder.outputChannels;
		m_frame_count = length;
		m_duration = (length > 0 && m_sample_rate > 0) ? seconds(static_cast<float>(length) / static_cast<float>(m_sample_rate)) : 0;
		ma_decoder_uninit(&decoder);
	}
}

auto gse::audio_clip::unload() -> void {
	m_bytes.clear();
	m_bytes.shrink_to_fit();
	m_sample_rate = 0;
	m_channels = 0;
	m_frame_count = 0;
	m_duration = {};
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

auto gse::audio::state::play(const audio_clip& clip, const bool loop) -> voice_handle {
	std::uint32_t index;
	if (!free_list.empty()) {
		index = free_list.back();
		free_list.pop_back();
	} else {
		index = static_cast<std::uint32_t>(voices.size());
		voices.push_back(std::make_unique<voice_slot>());
	}

	auto& [decoder, sound, generation, active] = *voices[index];
	generation++;
	active = true;

	const ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
	auto result = ma_decoder_init_memory(clip.data().data(), clip.data().size(), &cfg, &decoder);
	assert(result == MA_SUCCESS, std::source_location::current(), "Failed to init audio decoder");

	result = ma_sound_init_from_data_source(&engine, &decoder, 0, nullptr, &sound);
	assert(result == MA_SUCCESS, std::source_location::current(), "Failed to init audio sound");

	ma_sound_set_looping(&sound, loop ? MA_TRUE : MA_FALSE);
	ma_sound_start(&sound);

	return voice_handle{ .index = index, .generation = generation };
}

auto gse::audio::state::stop(const voice_handle handle) -> void {
	if (!valid_voice(handle)) return;
	auto& slot = *voices[handle.index];
	ma_sound_stop(&slot.sound);
	ma_sound_uninit(&slot.sound);
	ma_decoder_uninit(&slot.decoder);
	slot.active = false;
	free_list.push_back(handle.index);
}

auto gse::audio::state::pause(const voice_handle handle) const -> void {
	if (!valid_voice(handle)) return;
	ma_sound_stop(&voices[handle.index]->sound);
}

auto gse::audio::state::resume(const voice_handle handle) const -> void {
	if (!valid_voice(handle)) return;
	ma_sound_start(&voices[handle.index]->sound);
}

auto gse::audio::state::set_volume(const voice_handle handle, const percentage<float> vol) const -> void {
	if (!valid_voice(handle)) return;
	ma_sound_set_volume(&voices[handle.index]->sound, vol.value(percentage<float>::bound::zero_to_one));
}

auto gse::audio::state::set_master_volume(const percentage<float> vol) -> void {
	master_vol = vol;
	if (engine_initialized) {
		ma_engine_set_volume(&engine, vol.value(percentage<float>::bound::zero_to_one));
	}
}

auto gse::audio::state::master_volume() const -> percentage<float> {
	return master_vol;
}

auto gse::audio::state::cleanup_finished() -> void {
	for (std::uint32_t i = 0; i < voices.size(); ++i) {
		auto& slot = *voices[i];
		if (slot.active && ma_sound_at_end(&slot.sound)) {
			ma_sound_uninit(&slot.sound);
			ma_decoder_uninit(&slot.decoder);
			slot.active = false;
			free_list.push_back(i);
		}
	}
}

auto gse::audio::state::stop_all() -> void {
	for (std::uint32_t i = 0; i < voices.size(); ++i) {
		auto& slot = *voices[i];
		if (slot.active) {
			ma_sound_stop(&slot.sound);
			ma_sound_uninit(&slot.sound);
			ma_decoder_uninit(&slot.decoder);
			slot.active = false;
		}
	}
	free_list.clear();
	voices.clear();
}

auto gse::audio::state::valid_voice(const voice_handle handle) const -> bool {
	return handle.index < voices.size()
		&& voices[handle.index]->active
		&& voices[handle.index]->generation == handle.generation;
}

auto gse::audio::system::initialize(const initialize_phase&, state& s) -> void {
	const ma_engine_config cfg = ma_engine_config_init();
	const auto result = ma_engine_init(&cfg, &s.engine);
	assert(result == MA_SUCCESS, std::source_location::current(), "Failed to initialize audio engine");
	s.engine_initialized = true;
	ma_engine_set_volume(&s.engine, s.master_vol.value(percentage<float>::bound::zero_to_one));
}

auto gse::audio::system::update(update_phase&, state& s) -> void {
	s.cleanup_finished();
}

auto gse::audio::system::shutdown(shutdown_phase&, state& s) -> void {
	s.stop_all();
	if (s.engine_initialized) {
		ma_engine_uninit(&s.engine);
		s.engine_initialized = false;
	}
}
