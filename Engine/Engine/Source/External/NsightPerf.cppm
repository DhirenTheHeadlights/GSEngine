export module gse.nsight_perf;

import std;

import gse.math;

export namespace gse::nsight_perf {
	constexpr bool compiled_in =
#ifdef GSE_HAVE_NSIGHT_PERF
		true;
#else
		false;
#endif

	struct status_remedy {
		char text[192];
	};

	enum class session_status : std::uint8_t {
		idle,

		not_compiled_in [[= status_remedy{
			.text = "build the engine with NSIGHT_PERF_SDK pointing at the unzipped Nsight Perf SDK root",
		}]],

		load_failed [[= status_remedy{
			.text = "nvperf_grfx_host.dll did not load; it must sit next to the executable",
		}]],

		no_device [[= status_remedy{
			.text = "no NVIDIA device at the configured index; set Graphics.gpu_perf_metrics_device",
		}]],

		unsupported_gpu [[= status_remedy{
			.text = "this GPU has no periodic sampler; the SDK supports Turing and newer",
		}]],

		no_permission [[= status_remedy{
			.text = "allow GPU performance counters for all users under NVIDIA Control Panel > Developer, "
					"or run elevated: https://developer.nvidia.com/ERR_NVGPUCTRPERM",
		}]],

		no_metrics [[= status_remedy{
			.text = "Graphics.gpu_perf_metrics named no metric this device knows",
		}]],

		config_needs_replay [[= status_remedy{
			.text = "the metric set needs more than one pass and the periodic sampler cannot replay; "
					"remove metrics until it fits one pass",
		}]],

		start_failed [[= status_remedy{
			.text = "the SDK refused the session; the preceding gpu_perf line names the call that failed",
		}]],

		running
	};

	struct session_settings {
		std::uint32_t device_index = 0;
		std::span<const std::string_view> metrics;
		time sampling_interval;
		bool lock_clocks_to_rated_tdp = false;
	};

	struct session_info {
		session_status status = session_status::idle;
		std::uint64_t generation = 0;
		std::span<const std::string> metrics;
		time sampling_interval;
	};

	struct sample_window {
		std::span<const time_t<std::uint64_t>> times;
		std::span<const double> values;
	};

	auto begin_session(
		const session_settings& settings
	) -> session_status;

	auto end_session() -> void;

	[[nodiscard]] auto info() -> session_info;

	[[nodiscard]] auto gpu_timestamp() -> std::optional<time_t<std::uint64_t>>;

	[[nodiscard]] auto decode() -> sample_window;
}
