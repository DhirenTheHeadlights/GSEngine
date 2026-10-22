module gse.audio;

import std;

import gse.log;
import gse.core;
import gse.concurrency;
import gse.assets;
import gse.ecs;
import gse.math;

auto gse::audio_clip::load(asset::load_ctx&) -> async::task<asset_result> {
	auto loaded = load_baked<baked>(std::filesystem::path(m_path));
	if (!loaded) {
		co_return std::unexpected(std::move(loaded.error()));
	}
	m_bytes = std::move(loaded->bytes.storage);
	co_return asset_result{};
}

auto gse::audio::allocate_voice(data&, const audio_clip&, const bool) -> voice_handle {
	return {};
}

auto gse::audio::release_voice(data&, const voice_handle) -> void {
}

auto gse::audio::valid_voice(const data&, const voice_handle) -> bool {
	return false;
}

auto gse::audio::init(data&) -> async::task<> {
	log::println(log::level::warning, log::category::assets, "no audio backend on this platform; every clip is silent");
	return {};
}

auto gse::audio::run(context&, data& d, const channel_read<play_request, stop_request, pause_request, resume_request, set_volume_request, set_master_volume_request> requests_in) -> async::task<> {
	for (const auto& req : requests_in.of<play_request>()) {
		req.promise.fulfill(voice_handle{});
	}

	for (const auto& req : requests_in.of<set_master_volume_request>()) {
		d.master_vol = req.vol;
	}
	return {};
}

auto gse::audio::shutdown(data&) -> void {
}
