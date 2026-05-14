module gse.audio;

import std;

import gse.assert;
import gse.log;
import gse.core;
import gse.config;
import gse.concurrency;
import gse.assets;
import gse.ecs;
import gse.math;
import gse.gpu;
import gse.miniaudio;

namespace gse::audio {
	struct voice_slot {
		ma_decoder decoder{};
		ma_sound sound{};
		std::uint32_t generation = 0;
		bool active = false;
	};

	struct audio_engine {
		ma_engine inner{};
	};
}

auto gse::bake(const std::filesystem::path& src, audio_clip::baked& out) -> bool {
	std::ifstream file(src, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		log::println(log::level::warning, log::category::assets, "Failed to open audio source: {}", src.string());
		return false;
	}
	const auto size = file.tellg();
	file.seekg(0);
	out.bytes.storage.resize(size);
	file.read(reinterpret_cast<char*>(out.bytes.storage.data()), size);
	return true;
}

gse::audio_clip::audio_clip(const std::filesystem::path& filepath) : identifiable(filepath, config::baked_resource_path), m_path(filepath) {}

auto gse::audio_clip::load(asset::load_ctx&) -> async::task<> {
	baked b{};
	if (!load_baked(m_path, b)) {
		co_return;
	}
	m_bytes = std::move(b.bytes.storage);

	const ma_decoder_config cfg = ma_decoder_config_init(ma_format_value_unknown, 0, 0);
	ma_decoder decoder;
	if (const auto result = ma_decoder_init_memory(m_bytes.data(), m_bytes.size(), &cfg, &decoder); result == ma_result_success) {
		ma_uint64 length = 0;
		ma_decoder_get_length_in_pcm_frames(&decoder, &length);
		m_sample_rate = decoder.outputSampleRate;
		m_channels = decoder.outputChannels;
		m_frame_count = length;
		m_duration = (length > 0 && m_sample_rate > 0) ? seconds(static_cast<float>(length) / static_cast<float>(m_sample_rate)) : seconds(0.f);
		ma_decoder_uninit(&decoder);
	}
	co_return;
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

auto gse::audio::system::allocate_voice(resources& r, state& s, const audio_clip& clip, const bool loop) -> voice_handle {
	std::uint32_t index;
	if (!r.free_list.empty()) {
		index = r.free_list.back();
		r.free_list.pop_back();
	}
	else {
		index = static_cast<std::uint32_t>(r.voices.size());
		r.voices.push_back(new voice_slot());
	}

	auto& [decoder, sound, generation, active] = *r.voices[index];
	generation++;
	active = true;

	const ma_decoder_config cfg = ma_decoder_config_init(ma_format_value_f32, 0, 0);
	auto result = ma_decoder_init_memory(clip.data().data(), clip.data().size(), &cfg, &decoder);
	assert(result == ma_result_success, "Failed to init audio decoder");

	result = ma_sound_init_from_data_source(&s.engine->inner, &decoder, 0, nullptr, &sound);
	assert(result == ma_result_success, "Failed to init audio sound");

	ma_sound_set_looping(&sound, loop ? ma_bool_true : ma_bool_false);
	ma_sound_start(&sound);

	return {
		.index = index,
		.generation = generation,
	};
}

auto gse::audio::system::release_voice(resources& r, const voice_handle handle) -> void {
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

auto gse::audio::system::valid_voice(const resources& r, const voice_handle handle) -> bool {
	return handle.index < r.voices.size()
		&& r.voices[handle.index]->active
		&& r.voices[handle.index]->generation == handle.generation;
}

auto gse::audio::system::run(run_context& ctx, resources& r, state& s) -> async::task<> {
	s.engine = new audio_engine();
	const ma_engine_config cfg = ma_engine_config_init();
	const auto result = ma_engine_init(&cfg, &s.engine->inner);
	assert(result == ma_result_success, "Failed to initialize audio engine");
	s.engine_initialized = true;
	ma_engine_set_volume(&s.engine->inner, s.master_vol.value(percentage<float>::bound::zero_to_one));

	while (true) {
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
				ma_engine_set_volume(&s.engine->inner, req.vol.value(percentage<float>::bound::zero_to_one));
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

		co_await ctx.next_tick();
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
		delete r.voices[i];
	}
	r.free_list.clear();
	r.voices.clear();

	if (s.engine_initialized) {
		ma_engine_uninit(&s.engine->inner);
		delete s.engine;
		s.engine = nullptr;
		s.engine_initialized = false;
	}
}
