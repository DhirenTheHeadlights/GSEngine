module gse.audio;

import std;

import gse.log;
import gse.core;
import gse.concurrency;
import gse.assets;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.miniaudio;

namespace gse::audio {
	constexpr std::size_t recorded_block_capacity = 1u << 17;

	using audio_clock = std::chrono::steady_clock;

	struct output_device {
		ma_device device{};
		mixer* output = nullptr;
		std::vector<profile::sample_time> mix_times;
		std::vector<profile::sample_time> block_periods;
		audio_clock::time_point first_start;
		profile::sample_time buffer_span;
		profile::sample_time lateness_floor;
		std::uint64_t frames_supplied = 0;
		std::uint64_t blocks = 0;
		std::uint64_t underruns = 0;
		std::uint64_t allocations = 0;
	};

	auto elapsed(
		audio_clock::time_point from,
		audio_clock::time_point to
	) -> profile::sample_time;

	auto on_device_block(
		ma_device* device,
		void* out,
		const void* in,
		ma_uint32 frame_count
	) -> void;
}

auto gse::audio_clip::load(asset::load_ctx&) -> async::task<asset_result> {
	auto loaded = load_baked<baked>(std::filesystem::path(m_path));
	if (!loaded) {
		co_return std::unexpected(std::move(loaded.error()));
	}
	const auto& bytes = loaded->bytes.storage;

	ma_decoder_config cfg = ma_decoder_config_init(ma_format_value_f32, audio::output_channels, audio::output_frame_rate);
	ma_uint64 frames = 0;
	void* decoded = nullptr;
	if (ma_decode_memory(bytes.data(), bytes.size(), &cfg, &frames, &decoded) != ma_result_success) {
		co_return std::unexpected(asset_error{
			.code = asset_error_code::load_failure,
			.path = std::filesystem::path(m_path),
			.detail = "Unable to decode baked audio"
		});
	}

	const std::span<const float> pcm(static_cast<const float*>(decoded), static_cast<std::size_t>(frames) * audio::output_channels);
	m_samples.assign(pcm.begin(), pcm.end());
	ma_free(decoded, nullptr);

	co_return asset_result{};
}

auto gse::audio::device_deleter::operator()(output_device* device) const -> void {
	ma_device_uninit(&device->device);
	delete device;
}

auto gse::audio::elapsed(const audio_clock::time_point from, const audio_clock::time_point to) -> profile::sample_time {
	return nanoseconds(std::chrono::duration<double, std::nano>(to - from).count());
}

auto gse::audio::on_device_block(ma_device* device, void* out, const void*, const ma_uint32 frame_count) -> void {
	constexpr double clock_drift_allowance = 0.001;

	auto& state = *static_cast<output_device*>(device->pUserData);
	const frequency_t<double> rate = sample_rate();

	const auto started = audio_clock::now();
	if (state.blocks == 0) {
		state.first_start = started;
	}
	const std::uint64_t allocations_before = alloc::thread_allocation_count();
	state.output->render(std::span(static_cast<float*>(out), static_cast<std::size_t>(frame_count) * output_channels));
	state.allocations += alloc::thread_allocation_count() - allocations_before;

	const profile::sample_time mix_time = elapsed(started, audio_clock::now());
	const profile::sample_time block_period = static_cast<double>(frame_count) / rate;

	const profile::sample_time lateness = elapsed(state.first_start, started) - static_cast<double>(state.frames_supplied) / rate;
	state.lateness_floor = std::min(lateness, state.lateness_floor + block_period * clock_drift_allowance);
	if (lateness - state.lateness_floor > state.buffer_span || mix_time > block_period) {
		++state.underruns;
	}

	if (state.mix_times.size() < state.mix_times.capacity()) {
		state.mix_times.push_back(mix_time);
		state.block_periods.push_back(block_period);
	}
	state.frames_supplied += frame_count;
	++state.blocks;
}

auto gse::audio::open_device(mixer& output) -> device_ptr {
	auto state = std::make_unique<output_device>();
	state->output = &output;
	state->mix_times.reserve(recorded_block_capacity);
	state->block_periods.reserve(recorded_block_capacity);

	ma_device_config cfg = ma_device_config_init(ma_device_type_value_playback);
	cfg.playback.format = ma_format_value_f32;
	cfg.playback.channels = output_channels;
	cfg.sampleRate = output_frame_rate;
	cfg.periodSizeInFrames = static_cast<ma_uint32>(std::lround(sample_rate() * milliseconds(10.f)));
	cfg.noFixedSizedCallback = 1;
	cfg.dataCallback = &on_device_block;
	cfg.pUserData = state.get();

	if (ma_device_init(nullptr, &cfg, &state->device) != ma_result_success) {
		log::println(log::level::warning, log::category::general, "audio: no output device could be opened, so this run is silent");
		return nullptr;
	}

	device_ptr opened(state.release());
	const auto& playback = opened->device.playback;
	opened->buffer_span = static_cast<double>(playback.internalPeriodSizeInFrames * playback.internalPeriods) / hertz(static_cast<double>(playback.internalSampleRate));

	if (ma_device_start(&opened->device) != ma_result_success) {
		log::println(log::level::warning, log::category::general, "audio: the output device opened but would not start, so this run is silent");
		return nullptr;
	}

	log::println(log::category::general, "audio: output device started, {} channels, device buffer {:.1f:ms}", output_channels, opened->buffer_span);
	return opened;
}

auto gse::audio::stop_and_report(output_device& device, const std::uint32_t peak_voices) -> void {
	ma_device_stop(&device.device);

	std::ranges::sort(device.mix_times);
	std::ranges::sort(device.block_periods);
	const profile::sample_time mix_p50 = profile::percentile_of(device.mix_times, 0.50);
	const profile::sample_time mix_p95 = profile::percentile_of(device.mix_times, 0.95);
	const profile::sample_time block_period = profile::percentile_of(device.block_periods, 0.50);

	log::println(
		log::category::general,
		"audio: device ran {} blocks, {} underruns, {} allocations on the audio thread, mix p50 {:.3f:us} p95 {:.3f:us}, block period {:.3f:ms}, p95 over block {:.4f}, peak {} voices",
		device.blocks,
		device.underruns,
		device.allocations,
		mix_p50,
		mix_p95,
		block_period,
		device.blocks > 0 ? mix_p95 / block_period : 0.0,
		peak_voices
	);
}
