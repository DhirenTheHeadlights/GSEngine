export module gse.runtime:audio_api;

import std;

import gse.utility;
import gse.audio;
import gse.platform;

import :core_api;

export namespace gse::audio {
	auto play(
		const std::string& clip_name,
		bool loop = false
	) -> void;

	auto play(
		const resource::handle<audio_clip>& clip,
		bool loop = false
	) -> void;

	auto stop(
		voice_handle handle
	) -> void;

	auto pause(
		voice_handle handle
	) -> void;

	auto resume(
		voice_handle handle
	) -> void;

	auto set_volume(
		voice_handle handle,
		percentage<float> vol
	) -> void;

	auto set_master_volume(
		percentage<float> vol
	) -> void;

	auto master_volume(
	) -> percentage<float>;
}

auto gse::audio::play(const std::string& clip_name, bool loop) -> void {
	defer([clip_name, loop](state& s) { 
		const auto clip = s.ctx->get<audio_clip>(clip_name);
		s.play(*clip, loop);
	});
}

auto gse::audio::play(const resource::handle<audio_clip>& clip, bool loop) -> void {
	defer([clip, loop](state& s) {
		s.play(*clip, loop);
	});
}

auto gse::audio::stop(voice_handle handle) -> void {
	defer([handle](state& s) {
		s.stop(handle);
	});
}

auto gse::audio::pause(voice_handle handle) -> void {
	defer([handle](const state& s) {
		s.pause(handle);
	});
}

auto gse::audio::resume(voice_handle handle) -> void {
	defer([handle](const state& s) {
		s.resume(handle);
	});
}

auto gse::audio::set_volume(voice_handle handle, percentage<float> vol) -> void {
	defer([handle, vol](const state& s) {
		s.set_volume(handle, vol);
	});
}

auto gse::audio::set_master_volume(percentage<float> vol) -> void {
	defer([vol](state& s) {
		s.set_master_volume(vol);
	});
}

auto gse::audio::master_volume() -> percentage<float> {
	return state_of<state>().master_volume();
}
