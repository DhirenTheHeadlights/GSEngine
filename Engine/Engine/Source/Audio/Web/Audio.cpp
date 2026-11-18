module gse.audio;

import std;

import gse.log;
import gse.core;
import gse.concurrency;
import gse.assets;
import gse.ecs;
import gse.math;

namespace gse::audio {
	struct output_device {};
}

auto gse::audio_clip::load(asset::load_ctx&) -> async::task<asset_result> {
	auto loaded = load_baked<baked>(std::filesystem::path(m_path));
	if (!loaded) {
		co_return std::unexpected(std::move(loaded.error()));
	}
	co_return asset_result{};
}

auto gse::audio::device_deleter::operator()(output_device* device) const -> void {
	delete device;
}

auto gse::audio::open_device(mixer&) -> device_ptr {
	log::println(log::level::warning, log::category::general, "audio: no output device on this platform, so this run is silent");
	return nullptr;
}

auto gse::audio::stop_and_report(output_device&, const std::uint32_t) -> void {
}
