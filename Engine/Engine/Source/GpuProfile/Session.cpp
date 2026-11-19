module gse.gpu_profile;

import gse.log;
import gse.math;
import gse.meta;
import std;

import :backend;
import :session;

namespace gse::gpu_profile {
	std::atomic<session_status> current_status{ session_status::idle };
	std::atomic<std::uint64_t> session_generation{ 0 };
	session_state state;

	auto publish(
		session_status status
	) -> session_status;
}

auto gse::gpu_profile::publish(const session_status status) -> session_status {
	current_status.store(status, std::memory_order_release);

	if (status == session_status::running) {
		return status;
	}

	const status_remedy remedy = annotation_from_enum(status, status_remedy{});

	log::println(
		log::level::warning,
		log::category::gpu_perf,
		"no hardware counters ({}): {}",
		status,
		remedy.text
	);
	return status;
}

auto gse::gpu_profile::begin_session(const session_settings& settings) -> session_status {
	end_session();

	state.metrics.clear();
	state.metrics.reserve(settings.metrics.size());
	for (const std::string_view metric : settings.metrics) {
		state.metrics.emplace_back(metric);
	}
	state.interval = settings.sampling_interval;
	session_generation.fetch_add(1, std::memory_order_relaxed);

	return publish(backend::start_session(state, settings));
}

auto gse::gpu_profile::end_session() -> void {
	if (current_status.exchange(session_status::idle, std::memory_order_acq_rel) == session_status::running) {
		log::println(
			log::category::gpu_perf,
			"session ended after {} samples, {} windows dropped",
			state.decoded_samples,
			state.skipped_windows
		);
		backend::stop_session();
	}

	state.metrics.clear();
	state.interval = {};
	state.times.clear();
	state.values.clear();
	state.decoded_samples = 0;
	state.skipped_windows = 0;
}

auto gse::gpu_profile::info() -> session_info {
	const auto status = current_status.load(std::memory_order_acquire);
	const auto generation = session_generation.load(std::memory_order_relaxed);
	if (status != session_status::running) {
		return {
			.status = status,
			.generation = generation,
		};
	}
	return {
		.status = status,
		.generation = generation,
		.metrics = state.metrics,
		.sampling_interval = state.interval,
	};
}

auto gse::gpu_profile::gpu_timestamp() -> std::optional<time_t<std::uint64_t>> {
	if (current_status.load(std::memory_order_acquire) != session_status::running) {
		return std::nullopt;
	}
	return backend::raw_gpu_timestamp();
}

auto gse::gpu_profile::decode() -> sample_window {
	state.times.clear();
	state.values.clear();

	if (current_status.load(std::memory_order_acquire) == session_status::running) {
		backend::decode_samples(state);
	}

	return {
		.times = state.times,
		.values = state.values,
	};
}