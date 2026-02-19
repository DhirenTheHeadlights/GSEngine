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
	) -> voice_handle {
		auto& s = state_of<state>();
		auto clip = s.ctx->get<audio_clip>(clip_name);
		return s.play(*clip, loop);
	}

	auto play(
		const resource::handle<audio_clip>& clip,
		bool loop = false
	) -> voice_handle {
		return state_of<state>().play(*clip, loop);
	}

	auto stop(
		voice_handle handle
	) -> void {
		state_of<state>().stop(handle);
	}

	auto pause(
		voice_handle handle
	) -> void {
		state_of<state>().pause(handle);
	}

	auto resume(
		voice_handle handle
	) -> void {
		state_of<state>().resume(handle);
	}

	auto set_volume(
		voice_handle handle,
		percentage<float> vol
	) -> void {
		state_of<state>().set_volume(handle, vol);
	}

	auto set_master_volume(
		percentage<float> vol
	) -> void {
		state_of<state>().set_master_volume(vol);
	}

	auto master_volume(
	) -> percentage<float> {
		return state_of<state>().master_volume();
	}
}
