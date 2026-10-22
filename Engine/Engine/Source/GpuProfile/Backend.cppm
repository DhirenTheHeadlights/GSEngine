export module gse.gpu_profile:backend;

import std;

import gse.math;

import :session;

namespace gse::gpu_profile {
	struct session_state {
		std::vector<std::string> metrics;
		time interval;
		std::vector<time_t<std::uint64_t>> times;
		std::vector<double> values;
		std::uint64_t decoded_samples = 0;
		std::uint64_t skipped_windows = 0;
	};
}

namespace gse::gpu_profile::backend {
	auto start_session(
		session_state& state,
		const session_settings& settings
	) -> session_status;

	auto stop_session() -> void;

	[[nodiscard]] auto raw_gpu_timestamp() -> std::optional<time_t<std::uint64_t>>;

	auto decode_samples(
		session_state& state
	) -> void;
}
