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
	) -> void {
		defer<state>([clip_name, loop](state& s) {
			auto clip = s.ctx->get<audio_clip>(clip_name);
			s.play(*clip, loop);
		});
	}

	auto play(
		const resource::handle<audio_clip>& clip,
		bool loop = false
	) -> void {
		defer<state>([clip, loop](state& s) {
			s.play(*clip, loop);
		});
	}

	auto stop(
		voice_handle handle
	) -> void {
		defer<state>([handle](state& s) { s.stop(handle); });
	}

	auto pause(
		voice_handle handle
	) -> void {
		defer<state>([handle](state& s) { s.pause(handle); });
	}

	auto resume(
		voice_handle handle
	) -> void {
		defer<state>([handle](state& s) { s.resume(handle); });
	}

	auto set_volume(
		voice_handle handle,
		percentage<float> vol
	) -> void {
		defer<state>([handle, vol](state& s) { s.set_volume(handle, vol); });
	}

	auto set_master_volume(
		percentage<float> vol
	) -> void {
		defer<state>([vol](state& s) { s.set_master_volume(vol); });
	}

	auto master_volume(
	) -> percentage<float> {
		return state_of<state>().master_volume();
	}
}
