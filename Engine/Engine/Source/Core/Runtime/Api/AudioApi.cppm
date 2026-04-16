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
	) -> channel_future<voice_handle>;

	auto play(
		const resource::handle<audio_clip>& clip,
		bool loop = false
	) -> channel_future<voice_handle>;

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

auto gse::audio::play(const std::string& clip_name, const bool loop) -> channel_future<voice_handle> {
	const auto clip = resources_of<renderer::system::resources>().get<audio_clip>(clip_name);
	return channel_add<play_request>({
		.clip = clip.resolve(),
		.loop = loop
	});
}

auto gse::audio::play(const resource::handle<audio_clip>& clip, const bool loop) -> channel_future<voice_handle> {
	return channel_add<play_request>({
		.clip = clip.resolve(),
		.loop = loop
	});
}

auto gse::audio::stop(const voice_handle handle) -> void {
	channel_add<stop_request>({
		.handle = handle
	});
}

auto gse::audio::pause(const voice_handle handle) -> void {
	channel_add<pause_request>({
		.handle = handle
	});
}

auto gse::audio::resume(const voice_handle handle) -> void {
	channel_add<resume_request>({
		.handle = handle
	});
}

auto gse::audio::set_volume(const voice_handle handle, const percentage<float> vol) -> void {
	channel_add<set_volume_request>({
		.handle = handle,
		.vol = vol
	});
}

auto gse::audio::set_master_volume(const percentage<float> vol) -> void {
	channel_add<set_master_volume_request>({
		.vol = vol
	});
}

auto gse::audio::master_volume() -> percentage<float> {
	return state_of<state>().master_vol;
}
