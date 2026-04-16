module;

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef assert

export module gse.audio;

import std;

import gse.assert;
import gse.log;
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
			gpu::resource_manager& context
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
			log::println(log::level::warning, log::category::assets, "Failed to compile audio '{}': {}", source.string(), ec.message());
			return false;
		}
		log::println(log::category::assets, "Audio compiled: {}", destination.filename().string());
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

	struct play_request {
		using result_type = voice_handle;
		const audio_clip* clip = nullptr;
		bool loop = false;
		channel_promise<voice_handle> promise;
	};

	struct stop_request {
		voice_handle handle;
	};

	struct pause_request {
		voice_handle handle;
	};

	struct resume_request {
		voice_handle handle;
	};

	struct set_volume_request {
		voice_handle handle;
		percentage<float> vol;
	};

	struct set_master_volume_request {
		percentage<float> vol;
	};

	struct state {
		ma_engine engine{};
		bool engine_initialized = false;
		percentage<float> master_vol = percentage<float>::one();
	};

	struct system {
		struct resources {
			std::vector<std::unique_ptr<voice_slot>> voices;
			std::vector<std::uint32_t> free_list;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			resources& r,
			state& s
		) -> void;

		static auto shutdown(
			shutdown_context& phase,
			resources& r,
			state& s
		) -> void;
	};

	auto allocate_voice(
		system::resources& r,
		state& s,
		const audio_clip& clip,
		bool loop
	) -> voice_handle;

	auto release_voice(
		system::resources& r,
		voice_handle handle
	) -> void;

	auto valid_voice(
		const system::resources& r,
		voice_handle handle
	) -> bool;
}

gse::audio_clip::audio_clip(const std::filesystem::path& filepath) : identifiable(filepath, config::baked_resource_path), m_path(filepath) {}

auto gse::audio_clip::load(gpu::resource_manager&) -> void {
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
		m_duration = (length > 0 && m_sample_rate > 0) ? seconds(static_cast<float>(length) / static_cast<float>(m_sample_rate)) : seconds(0.f);
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

auto gse::audio::allocate_voice(system::resources& r, state& s, const audio_clip& clip, const bool loop) -> voice_handle {
	std::uint32_t index;
	if (!r.free_list.empty()) {
		index = r.free_list.back();
		r.free_list.pop_back();
	} else {
		index = static_cast<std::uint32_t>(r.voices.size());
		r.voices.push_back(std::make_unique<voice_slot>());
	}

	auto& [decoder, sound, generation, active] = *r.voices[index];
	generation++;
	active = true;

	const ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
	auto result = ma_decoder_init_memory(clip.data().data(), clip.data().size(), &cfg, &decoder);
	assert(result == MA_SUCCESS, std::source_location::current(), "Failed to init audio decoder");

	result = ma_sound_init_from_data_source(&s.engine, &decoder, 0, nullptr, &sound);
	assert(result == MA_SUCCESS, std::source_location::current(), "Failed to init audio sound");

	ma_sound_set_looping(&sound, loop ? MA_TRUE : MA_FALSE);
	ma_sound_start(&sound);

	return {
		.index = index,
		.generation = generation
	};
}

auto gse::audio::release_voice(system::resources& r, const voice_handle handle) -> void {
	if (!valid_voice(r, handle)) {
		return;
	}
	auto& slot = *r.voices[handle.index];
	ma_sound_stop(&slot.sound);
	ma_sound_uninit(&slot.sound);
	ma_decoder_uninit(&slot.decoder);
	slot.active = false;
	r.free_list.push_back(handle.index);
}

auto gse::audio::valid_voice(const system::resources& r, const voice_handle handle) -> bool {
	return handle.index < r.voices.size()
		&& r.voices[handle.index]->active
		&& r.voices[handle.index]->generation == handle.generation;
}

auto gse::audio::system::initialize(const init_context&, resources&, state& s) -> void {
	const ma_engine_config cfg = ma_engine_config_init();
	const auto result = ma_engine_init(&cfg, &s.engine);
	assert(result == MA_SUCCESS, std::source_location::current(), "Failed to initialize audio engine");
	s.engine_initialized = true;
	ma_engine_set_volume(&s.engine, s.master_vol.value(percentage<float>::bound::zero_to_one));
}

auto gse::audio::system::update(update_context& ctx, resources& r, state& s) -> void {
	for (const auto& req : ctx.read_channel<play_request>()) {
		if (req.clip) {
			const auto handle = allocate_voice(r, s, *req.clip, req.loop);
			req.promise.fulfill(handle);
		}
	}

	for (const auto& req : ctx.read_channel<stop_request>()) {
		release_voice(r, req.handle);
	}

	for (const auto& req : ctx.read_channel<pause_request>()) {
		if (valid_voice(r, req.handle)) {
			ma_sound_stop(&r.voices[req.handle.index]->sound);
		}
	}

	for (const auto& req : ctx.read_channel<resume_request>()) {
		if (valid_voice(r, req.handle)) {
			ma_sound_start(&r.voices[req.handle.index]->sound);
		}
	}

	for (const auto& req : ctx.read_channel<set_volume_request>()) {
		if (valid_voice(r, req.handle)) {
			ma_sound_set_volume(&r.voices[req.handle.index]->sound, req.vol.value(percentage<float>::bound::zero_to_one));
		}
	}

	for (const auto& req : ctx.read_channel<set_master_volume_request>()) {
		s.master_vol = req.vol;
		if (s.engine_initialized) {
			ma_engine_set_volume(&s.engine, req.vol.value(percentage<float>::bound::zero_to_one));
		}
	}

	for (std::uint32_t i = 0; i < r.voices.size(); ++i) {
		auto& slot = *r.voices[i];
		if (slot.active && ma_sound_at_end(&slot.sound)) {
			ma_sound_uninit(&slot.sound);
			ma_decoder_uninit(&slot.decoder);
			slot.active = false;
			r.free_list.push_back(i);
		}
	}
}

auto gse::audio::system::shutdown(shutdown_context&, resources& r, state& s) -> void {
	for (std::uint32_t i = 0; i < r.voices.size(); ++i) {
		auto& slot = *r.voices[i];
		if (slot.active) {
			ma_sound_stop(&slot.sound);
			ma_sound_uninit(&slot.sound);
			ma_decoder_uninit(&slot.decoder);
			slot.active = false;
		}
	}
	r.free_list.clear();
	r.voices.clear();

	if (s.engine_initialized) {
		ma_engine_uninit(&s.engine);
		s.engine_initialized = false;
	}
}
