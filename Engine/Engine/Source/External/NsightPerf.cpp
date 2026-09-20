module;

#ifdef GSE_HAVE_NSIGHT_PERF
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <x86intrin.h>
#include <NvPerfInit.h>
#include <NvPerfDeviceProperties.h>
#include <NvPerfMetricsEvaluator.h>
#include <NvPerfMetricsConfigBuilder.h>
#include <NvPerfCounterConfiguration.h>
#include <NvPerfCounterData.h>
#include <NvPerfPeriodicSamplerCommon.h>
#include <NvPerfPeriodicSamplerGpu.h>
#include <windows-desktop-x64/nvperf_host_impl.h>
#endif

module gse.nsight_perf;

import std;

import gse.log;
import gse.math;
import gse.meta;

namespace gse::nsight_perf {
	std::atomic<session_status> current_status{ session_status::idle };
	std::vector<std::string> active_metrics;
	time active_interval;
	std::vector<time_t<std::uint64_t>> sample_times;
	std::vector<double> sample_values;
	std::uint64_t decoded_samples = 0;
	std::uint64_t skipped_windows = 0;
	std::atomic<std::uint64_t> session_generation{ 0 };

	auto publish(
		session_status status
	) -> session_status;

#ifdef GSE_HAVE_NSIGHT_PERF
	constexpr std::uint32_t buffered_samples = 65536;

	struct sdk_state {
		nv::perf::sampler::GpuPeriodicSampler sampler;
		nv::perf::MetricsEvaluator evaluator;
		nv::perf::sampler::RingBufferCounterData counter_data;
		std::vector<NVPW_MetricEvalRequest> requests;
		std::vector<std::uint8_t> config_image;
		std::vector<double> evaluated;
		nv::perf::ClockInfo restore_clocks;
		std::size_t device_index = 0;
		bool sampling = false;
		bool clocks_locked = false;
		bool keep_latest = false;
		bool overflow_reported = false;
		bool stall_reported = false;
	};

	sdk_state sdk;

	auto forward_sdk_log(
		const char* prefix,
		const char* date,
		const char* clock,
		const char* function,
		const char* message,
		void* user
	) -> void;

	auto route_sdk_logs() -> void;

	auto map_status(
		NVPA_Status status,
		session_status fallback
	) -> session_status;

	auto report_sdk_failure(
		std::string_view call,
		NVPA_Status status
	) -> void;

	auto initialize_sdk() -> session_status;

	auto resolve_metrics() -> session_status;

	auto suggest_metric_names(
		std::string_view rejected
	) -> void;

	auto build_config(
		std::uint32_t& passes
	) -> session_status;

	auto metrics_fitting_one_pass(
		const char* chip_name
	) -> std::size_t;

	auto lock_clocks() -> void;

	auto start_sdk_session(
		const session_settings& settings
	) -> session_status;

	auto stop_sdk_session() -> void;

	auto skip_to_newest_records(
		std::size_t unread_bytes
	) -> void;

	auto decode_sdk_samples() -> void;
#endif
}

auto gse::nsight_perf::publish(const session_status status) -> session_status {
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
		std::string_view(remedy.text)
	);
	return status;
}

#ifdef GSE_HAVE_NSIGHT_PERF
auto gse::nsight_perf::forward_sdk_log(
	const char*,
	const char*,
	const char*,
	const char* function,
	const char* message,
	void*
) -> void {
	std::string_view text(message ? message : "");
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
		text.remove_suffix(1);
	}
	if (text.empty()) {
		return;
	}
	log::println(
		log::level::warning,
		log::category::gpu_perf,
		"nvperf: {}: {}",
		std::string_view(function ? function : "?"),
		text
	);
}

auto gse::nsight_perf::route_sdk_logs() -> void {
	static bool routed = false;
	if (routed) {
		return;
	}
	routed = true;
	nv::perf::UserLogEnableStderr(false);
	nv::perf::UserLogEnablePlatform(false);
	nv::perf::UserLogEnableCustom(&forward_sdk_log, nullptr);
}

auto gse::nsight_perf::map_status(const NVPA_Status status, const session_status fallback) -> session_status {
	switch (status) {
		case NVPA_STATUS_INSUFFICIENT_PRIVILEGE:
		case NVPA_STATUS_PROFILING_NOT_ALLOWED:
			return session_status::no_permission;
		case NVPA_STATUS_UNSUPPORTED_GPU:
		case NVPA_STATUS_VIRTUALIZED_DEVICE_NOT_SUPPORTED:
			return session_status::unsupported_gpu;
		case NVPA_STATUS_NOT_LOADED:
		case NVPA_STATUS_FUNCTION_NOT_FOUND:
		case NVPA_STATUS_DRIVER_NOT_LOADED:
		case NVPA_STATUS_INSUFFICIENT_DRIVER_VERSION:
			return session_status::load_failed;
		default:
			return fallback;
	}
}

auto gse::nsight_perf::report_sdk_failure(const std::string_view call, const NVPA_Status status) -> void {
	const char* status_text = "";
	const char* comment = "";
	NVPW_NVPAStatusToString(status, &status_text, &comment);
	log::println(
		log::level::warning,
		log::category::gpu_perf,
		"nvperf: {} failed: {} ({})",
		call,
		std::string_view(status_text),
		std::string_view(comment)
	);
}

auto gse::nsight_perf::initialize_sdk() -> session_status {
	route_sdk_logs();

	NVPW_InitializeHost_Params host_params{ NVPW_InitializeHost_Params_STRUCT_SIZE };
	if (const auto status = NVPW_InitializeHost(&host_params); status != NVPA_STATUS_SUCCESS) {
		report_sdk_failure("NVPW_InitializeHost", status);
		return map_status(status, session_status::load_failed);
	}

	NVPW_InitializeTarget_Params target_params{ NVPW_InitializeTarget_Params_STRUCT_SIZE };
	if (const auto status = NVPW_InitializeTarget(&target_params); status != NVPA_STATUS_SUCCESS) {
		report_sdk_failure("NVPW_InitializeTarget", status);
		return map_status(status, session_status::load_failed);
	}

	NVPW_D3D12_LoadDriver_Params driver_params{ NVPW_D3D12_LoadDriver_Params_STRUCT_SIZE };
	if (const auto status = NVPW_D3D12_LoadDriver(&driver_params); status != NVPA_STATUS_SUCCESS) {
		report_sdk_failure("NVPW_D3D12_LoadDriver", status);
		return map_status(status, session_status::load_failed);
	}

	NVPW_GPU_PeriodicSampler_IsGpuSupported_Params supported_params{
		NVPW_GPU_PeriodicSampler_IsGpuSupported_Params_STRUCT_SIZE
	};
	supported_params.deviceIndex = sdk.device_index;
	if (const auto status = NVPW_GPU_PeriodicSampler_IsGpuSupported(&supported_params);
		status != NVPA_STATUS_SUCCESS) {
		report_sdk_failure("NVPW_GPU_PeriodicSampler_IsGpuSupported", status);
		return map_status(status, session_status::no_device);
	}
	if (!supported_params.isSupported) {
		return session_status::unsupported_gpu;
	}

	return session_status::running;
}

auto gse::nsight_perf::resolve_metrics() -> session_status {
	const auto identifiers = nv::perf::GetDeviceIdentifiers(sdk.device_index);
	if (!identifiers.pChipName) {
		return session_status::no_device;
	}

	std::vector<std::uint8_t> scratch;
	NVPW_MetricsEvaluator* const evaluator =
		nv::perf::sampler::DeviceCreateMetricsEvaluator(scratch, identifiers.pChipName);
	if (!evaluator) {
		return session_status::start_failed;
	}
	sdk.evaluator = nv::perf::MetricsEvaluator(evaluator, std::move(scratch));

	std::vector<std::string> resolved;
	resolved.reserve(active_metrics.size());
	sdk.requests.clear();
	sdk.requests.reserve(active_metrics.size());

	for (const auto& metric : active_metrics) {
		NVPW_MetricEvalRequest request{};
		if (!nv::perf::ToMetricEvalRequest(sdk.evaluator, metric.c_str(), request)) {
			log::println(
				log::level::warning,
				log::category::gpu_perf,
				"dropping metric this device does not know: {}",
				metric
			);
			suggest_metric_names(metric);
			continue;
		}
		resolved.push_back(metric);
		sdk.requests.push_back(request);
	}

	if (sdk.requests.empty()) {
		return session_status::no_metrics;
	}

	active_metrics = std::move(resolved);
	sdk.evaluated.assign(sdk.requests.size(), 0.0);
	return session_status::running;
}

auto gse::nsight_perf::suggest_metric_names(const std::string_view rejected) -> void {
	constexpr std::size_t max_suggestions = 6;
	const std::string_view base = rejected.substr(0, rejected.find('.'));

	for (const auto type : {
			NVPW_METRIC_TYPE_COUNTER,
			NVPW_METRIC_TYPE_RATIO,
			NVPW_METRIC_TYPE_THROUGHPUT,
		}) {
		bool present = false;
		for (const char* const name : nv::perf::EnumerateMetrics(sdk.evaluator, type)) {
			if (base == name) {
				present = true;
				break;
			}
		}
		if (!present) {
			continue;
		}

		std::vector<NVPW_Submetric> submetrics;
		if (!nv::perf::GetSupportedSubmetrics(sdk.evaluator, type, submetrics)) {
			return;
		}

		const std::string_view rollup = type == NVPW_METRIC_TYPE_COUNTER ? ".avg" : "";
		std::string spellings;
		for (std::size_t i = 0; i < submetrics.size() && i < max_suggestions; ++i) {
			if (!spellings.empty()) {
				spellings += ", ";
			}
			spellings += std::format(
				"{}{}{}",
				base,
				rollup,
				std::string_view(nv::perf::ToCString(submetrics[i]))
			);
		}

		log::println(
			log::level::warning,
			log::category::gpu_perf,
			"{} is a {} on this device; it needs a rollup and submetric, such as {}",
			base,
			std::string_view(nv::perf::ToCString(type)),
			spellings
		);
		return;
	}
}

auto gse::nsight_perf::metrics_fitting_one_pass(const char* const chip_name) -> std::size_t {
	for (std::size_t count = 1; count <= sdk.requests.size(); ++count) {
		NVPW_RawCounterConfig* const raw_config =
			nv::perf::sampler::DeviceCreateRawCounterConfig(chip_name);
		if (!raw_config) {
			return count - 1;
		}

		nv::perf::MetricsConfigBuilder builder;
		if (!builder.Initialize(sdk.evaluator, raw_config, chip_name)) {
			return count - 1;
		}
		if (!builder.AddMetrics(sdk.requests.data(), count, false)) {
			return count - 1;
		}

		nv::perf::CounterConfiguration configuration;
		if (!nv::perf::CreateConfiguration(builder, configuration) || configuration.numPasses != 1) {
			return count - 1;
		}
	}
	return sdk.requests.size();
}

auto gse::nsight_perf::build_config(std::uint32_t& passes) -> session_status {
	const auto identifiers = nv::perf::GetDeviceIdentifiers(sdk.device_index);
	NVPW_RawCounterConfig* const raw_config =
		nv::perf::sampler::DeviceCreateRawCounterConfig(identifiers.pChipName);
	if (!raw_config) {
		return session_status::start_failed;
	}

	nv::perf::MetricsConfigBuilder builder;
	if (!builder.Initialize(sdk.evaluator, raw_config, identifiers.pChipName)) {
		return session_status::start_failed;
	}

	for (std::size_t i = 0; i < sdk.requests.size(); ++i) {
		if (!builder.AddMetrics(&sdk.requests[i], 1, false)) {
			log::println(
				log::level::warning,
				log::category::gpu_perf,
				"the device refused metric {}",
				active_metrics[i]
			);
			return session_status::config_needs_replay;
		}
	}

	nv::perf::CounterConfiguration configuration;
	if (!nv::perf::CreateConfiguration(builder, configuration)) {
		return session_status::start_failed;
	}

	passes = static_cast<std::uint32_t>(configuration.numPasses);
	if (configuration.numPasses != 1) {
		if (const auto fits = metrics_fitting_one_pass(identifiers.pChipName);
			fits < active_metrics.size()) {
			log::println(
				log::level::warning,
				log::category::gpu_perf,
				"one pass holds the first {} of {} metrics; {} is the first that does not fit",
				fits,
				active_metrics.size(),
				active_metrics[fits]
			);
		}
		return session_status::config_needs_replay;
	}

	sdk.config_image = std::move(configuration.configImage);

	if (!sdk.counter_data.Initialize(
			buffered_samples,
			false,
			[&](const std::uint32_t max_samples,
				const NVPW_PeriodicSampler_CounterData_AppendMode append_mode,
				std::vector<std::uint8_t>& image) {
				return nv::perf::sampler::GpuPeriodicSamplerCreateCounterData(
					sdk.device_index,
					configuration.counterDataPrefix.data(),
					configuration.counterDataPrefix.size(),
					max_samples,
					append_mode,
					image
				);
			}
		)) {
		return session_status::start_failed;
	}

	if (!nv::perf::MetricsEvaluatorSetDeviceAttributes(
			sdk.evaluator,
			sdk.counter_data.GetCounterData().data(),
			sdk.counter_data.GetCounterData().size()
		)) {
		return session_status::start_failed;
	}

	return session_status::running;
}

auto gse::nsight_perf::lock_clocks() -> void {
	sdk.restore_clocks = nv::perf::GetDeviceClockState(sdk.device_index);
	sdk.clocks_locked =
		nv::perf::SetDeviceClockState(sdk.device_index, NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP);
}

auto gse::nsight_perf::start_sdk_session(const session_settings& settings) -> session_status {
	sdk.device_index = settings.device_index;

	if (const auto status = initialize_sdk(); status != session_status::running) {
		return status;
	}
	if (const auto status = resolve_metrics(); status != session_status::running) {
		return status;
	}

	std::uint32_t passes = 0;
	if (const auto status = build_config(passes); status != session_status::running) {
		if (status == session_status::config_needs_replay && passes > 1) {
			log::println(
				log::level::warning,
				log::category::gpu_perf,
				"{} metrics need {} passes; the periodic sampler collects one",
				active_metrics.size(),
				passes
			);
		}
		return status;
	}

	if (!sdk.sampler.Initialize(sdk.device_index)) {
		return session_status::unsupported_gpu;
	}

	const auto interval_ns =
		static_cast<std::uint32_t>(std::max(1.f, static_cast<float>(settings.sampling_interval)));
	if (!sdk.sampler.IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_TIME_INTERVAL) &&
		!sdk.sampler.IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL)) {
		return session_status::unsupported_gpu;
	}
	const auto pulse = sdk.sampler.GetGpuPulseSamplingInterval(interval_ns);

	std::size_t record_buffer_size = 0;
	if (!nv::perf::sampler::GpuPeriodicSamplerCalculateRecordBufferSize(
			sdk.device_index,
			sdk.config_image,
			buffered_samples,
			record_buffer_size
		)) {
		return session_status::start_failed;
	}

	if (settings.lock_clocks_to_rated_tdp) {
		lock_clocks();
	}

	sdk.keep_latest = nv::perf::sampler::GpuPeriodicSamplerIsKeepLatestModeSupported(sdk.device_index);
	const auto append_mode = sdk.keep_latest
		? NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_KEEP_LATEST
		: NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_KEEP_OLDEST;

	if (!sdk.sampler.BeginSession(
			record_buffer_size,
			1,
			{ pulse.triggerSource },
			pulse.samplingInterval,
			append_mode
		)) {
		return session_status::start_failed;
	}
	if (!sdk.sampler.SetConfig(sdk.config_image, 0)) {
		return session_status::start_failed;
	}
	if (!sdk.sampler.StartSampling()) {
		return session_status::start_failed;
	}
	sdk.sampling = true;

	const auto& identifiers = sdk.sampler.GetDeviceIdentifiers();
	log::println(
		log::category::gpu_perf,
		"sampling {} metrics on {} ({}) every {:.1f:us} in 1 pass, buffer {} samples ({}), clocks {}",
		active_metrics.size(),
		std::string_view(identifiers.pDeviceName ? identifiers.pDeviceName : "?"),
		std::string_view(identifiers.pChipName ? identifiers.pChipName : "?"),
		active_interval,
		buffered_samples,
		sdk.keep_latest ? "keep latest" : "keep oldest",
		sdk.clocks_locked ? "locked to rated tdp" : "untouched"
	);
	return session_status::running;
}

auto gse::nsight_perf::stop_sdk_session() -> void {
	if (sdk.sampling) {
		sdk.sampler.StopSampling();
		sdk.sampling = false;
	}
	sdk.sampler.Reset();

	if (sdk.clocks_locked) {
		nv::perf::SetDeviceClockState(sdk.device_index, sdk.restore_clocks);
		sdk.clocks_locked = false;
	}

	sdk.counter_data.Reset();
	sdk.evaluator.Reset();
	sdk.requests.clear();
	sdk.config_image.clear();
	sdk.evaluated.clear();
	sdk.keep_latest = false;
	sdk.overflow_reported = false;
	sdk.stall_reported = false;
}

auto gse::nsight_perf::skip_to_newest_records(const std::size_t unread_bytes) -> void {
	nv::perf::sampler::GpuPeriodicSampler::GetRecordBufferStatusParams status_params{};
	status_params.queryWriteOffset = true;
	if (!sdk.sampler.GetRecordBufferStatus(status_params)) {
		return;
	}

	NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params offset_params{
		NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params_STRUCT_SIZE
	};
	offset_params.deviceIndex = sdk.device_index;
	offset_params.readOffset = status_params.writeOffset;
	if (const auto status = NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset(&offset_params);
		status != NVPA_STATUS_SUCCESS) {
		report_sdk_failure("NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset", status);
		return;
	}
	if (!sdk.sampler.AcknowledgeRecordBuffer(unread_bytes)) {
		return;
	}
	++skipped_windows;
}

auto gse::nsight_perf::decode_sdk_samples() -> void {
	nv::perf::sampler::GpuPeriodicSampler::GetRecordBufferStatusParams status_params{};
	status_params.queryNumUnreadBytes = true;
	status_params.queryOverflow = true;
	if (!sdk.sampler.GetRecordBufferStatus(status_params)) {
		return;
	}

	if (status_params.overflow && !sdk.overflow_reported) {
		sdk.overflow_reported = true;
		log::println(
			log::level::warning,
			log::category::gpu_perf,
			"sample record buffer overflowed after {} samples; the driver stops recording until the session "
			"restarts. raise Graphics.gpu_perf_metrics_interval",
			decoded_samples
		);
	}
	if (status_params.numUnreadBytes == 0) {
		return;
	}
	if (status_params.numUnreadBytes * 2 > status_params.totalSize) {
		if (!sdk.stall_reported) {
			sdk.stall_reported = true;
			log::println(
				log::level::warning,
				log::category::gpu_perf,
				"{} of {} record bytes unread at decode; dropping to the newest records. "
				"raise Graphics.gpu_perf_metrics_interval to attribute every frame",
				status_params.numUnreadBytes,
				status_params.totalSize
			);
		}
		skip_to_newest_records(status_params.numUnreadBytes);
		return;
	}

	auto stop_reason = NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_OTHER;
	std::size_t merged = 0;
	std::size_t consumed_bytes = 0;
	if (!sdk.sampler.DecodeCounters(
			sdk.counter_data.GetCounterData(),
			status_params.numUnreadBytes,
			stop_reason,
			merged,
			consumed_bytes
		)) {
		return;
	}
	if (!sdk.sampler.AcknowledgeRecordBuffer(consumed_bytes)) {
		return;
	}
	if (stop_reason == NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_UNEXPECTED_RECORD ||
		stop_reason == NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_OUT_OF_ORDER_RECORD ||
		stop_reason == NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_EXCESSIVE_BACKPRESSURE) {
		if (!sdk.stall_reported) {
			sdk.stall_reported = true;
			log::println(
				log::level::warning,
				log::category::gpu_perf,
				"record stream decode stopped on reason {}; dropping to the newest records",
				static_cast<std::uint32_t>(stop_reason)
			);
		}
		skip_to_newest_records(status_params.numUnreadBytes - consumed_bytes);
		return;
	}
	if (!sdk.counter_data.UpdatePut()) {
		return;
	}

	const auto ranges = sdk.counter_data.GetNumUnreadRanges();
	if (ranges == 0) {
		return;
	}

	const auto metric_count = sdk.requests.size();
	sample_times.reserve(ranges);
	sample_values.reserve(static_cast<std::size_t>(ranges) * metric_count);

	std::uint32_t decoded = 0;
	sdk.counter_data.ConsumeData(
		[&](const std::uint8_t* image, const std::size_t image_size, const std::uint32_t range_index, bool&) {
			nv::perf::sampler::SampleTimestamp stamp{};
			if (!nv::perf::sampler::CounterDataGetSampleTime(image, range_index, stamp)) {
				return false;
			}
			if (!nv::perf::EvaluateToGpuValues(
					sdk.evaluator,
					image,
					image_size,
					range_index,
					metric_count,
					sdk.requests.data(),
					sdk.evaluated.data()
				)) {
				return false;
			}
			sample_times.push_back(nanoseconds(stamp.start));
			sample_values.insert(sample_values.end(), sdk.evaluated.begin(), sdk.evaluated.end());
			++decoded;
			return true;
		}
	);

	if (decoded == 0) {
		return;
	}
	sdk.counter_data.UpdateGet(decoded);
	decoded_samples += decoded;
}
#endif

auto gse::nsight_perf::begin_session(const session_settings& settings) -> session_status {
	end_session();

	active_metrics.clear();
	active_metrics.reserve(settings.metrics.size());
	for (const std::string_view metric : settings.metrics) {
		active_metrics.emplace_back(metric);
	}
	active_interval = settings.sampling_interval;
	session_generation.fetch_add(1, std::memory_order_relaxed);

#ifndef GSE_HAVE_NSIGHT_PERF
	return publish(session_status::not_compiled_in);
#else
	if (active_metrics.empty()) {
		return publish(session_status::no_metrics);
	}

	const auto status = start_sdk_session(settings);
	if (status != session_status::running) {
		stop_sdk_session();
		active_metrics.clear();
	}
	return publish(status);
#endif
}

auto gse::nsight_perf::end_session() -> void {
	if (current_status.exchange(session_status::idle, std::memory_order_acq_rel) == session_status::running) {
		log::println(
			log::category::gpu_perf,
			"session ended after {} samples, {} windows dropped",
			decoded_samples,
			skipped_windows
		);
#ifdef GSE_HAVE_NSIGHT_PERF
		stop_sdk_session();
#endif
	}

	active_metrics.clear();
	active_interval = {};
	sample_times.clear();
	sample_values.clear();
	decoded_samples = 0;
	skipped_windows = 0;
}

auto gse::nsight_perf::info() -> session_info {
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
		.metrics = active_metrics,
		.sampling_interval = active_interval,
	};
}

auto gse::nsight_perf::gpu_timestamp() -> std::optional<time_t<std::uint64_t>> {
#ifdef GSE_HAVE_NSIGHT_PERF
	if (current_status.load(std::memory_order_acquire) != session_status::running) {
		return std::nullopt;
	}
	NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params params{
		NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params_STRUCT_SIZE
	};
	params.deviceIndex = sdk.device_index;
	if (NVPW_GPU_PeriodicSampler_GetGpuTimestamp(&params) != NVPA_STATUS_SUCCESS) {
		return std::nullopt;
	}
	return nanoseconds(params.timestamp);
#else
	return std::nullopt;
#endif
}

auto gse::nsight_perf::decode() -> sample_window {
	sample_times.clear();
	sample_values.clear();

#ifdef GSE_HAVE_NSIGHT_PERF
	if (current_status.load(std::memory_order_acquire) == session_status::running) {
		decode_sdk_samples();
	}
#endif

	return {
		.times = sample_times,
		.values = sample_values,
	};
}
