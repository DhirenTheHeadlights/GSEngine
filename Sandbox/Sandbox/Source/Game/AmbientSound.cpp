module sandbox:ambient_sound_impl;

import std;
import gse;

import :ambient_sound;

auto sandbox::ambient_sound::run(data& d, gse::write<component> sounds, const gse::shared_view<gse::asset::data> assets_d, const gse::channel_write<gse::audio::play_request> audio_out) -> gse::async::task<> {
	d.waiting.append_range(sounds.drain(gse::component_event::added));

	std::erase_if(
		d.waiting,
		[&](const gse::id owner) {
			const auto* sound = sounds.find(owner);
			if (sound == nullptr) {
				return true;
			}
			const auto clip = gse::asset::get<gse::audio_clip>(assets_d, std::string(sound->clip));
			if (!clip.valid()) {
				return false;
			}
			audio_out.push<gse::audio::play_request>({
				.clip = clip,
				.loop = true,
				.gain = sound->gain,
				.emitter = owner,
			});
			return true;
		}
	);

	return {};
}
