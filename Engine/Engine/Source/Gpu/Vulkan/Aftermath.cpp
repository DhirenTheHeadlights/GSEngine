module;

#ifdef GSE_HAVE_AFTERMATH
#include <GFSDK_Aftermath_Defines.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#endif

module gse.gpu;

import std;
import vulkan;

import gse.config;
import gse.log;
import gse.math;
import gse.time;

namespace gse::vulkan {
	constexpr std::array k_required_device_extensions = {
		vk::NVDeviceDiagnosticCheckpointsExtensionName,
		vk::NVDeviceDiagnosticsConfigExtensionName,
	};

	aftermath::state* active_state = nullptr;

	auto default_dump_directory(
	) -> std::filesystem::path;

	auto default_shader_directory(
	) -> std::filesystem::path;

	auto build_diagnostics_flags(
		const aftermath::settings& cfg
	) -> vk::DeviceDiagnosticsConfigFlagsNV;

	auto write_dump_to_disk(
		const std::filesystem::path& directory,
		const std::string& stem,
		std::string_view extension,
		const void* data,
		std::size_t size
	) -> std::filesystem::path;

#ifdef GSE_HAVE_AFTERMATH
	auto translate_result(
		GFSDK_Aftermath_Result r
	) -> std::string;
#endif
}

auto gse::vulkan::default_dump_directory() -> std::filesystem::path {
	return config::resource_path / "Misc" / "crash_dumps";
}

auto gse::vulkan::default_shader_directory() -> std::filesystem::path {
	return config::resource_path / "Misc" / "aftermath_shaders";
}

auto gse::vulkan::build_diagnostics_flags(const aftermath::settings& cfg) -> vk::DeviceDiagnosticsConfigFlagsNV {
	vk::DeviceDiagnosticsConfigFlagsNV flags{};
	if (cfg.resource_tracking) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking;
	}
	if (cfg.automatic_checkpoints) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints;
	}
	if (cfg.shader_debug_info) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo;
	}
	if (cfg.shader_error_reporting) {
		flags |= vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderErrorReporting;
	}
	return flags;
}

auto gse::vulkan::write_dump_to_disk(const std::filesystem::path& directory, const std::string& stem, std::string_view extension, const void* data, std::size_t size) -> std::filesystem::path {
	std::error_code ec;
	std::filesystem::create_directories(directory, ec);

	const auto path = directory / (stem + std::string(extension));
	std::ofstream out(path, std::ios::binary);
	if (out) {
		out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
	}
	return path;
}

#ifdef GSE_HAVE_AFTERMATH
auto gse::vulkan::translate_result(const GFSDK_Aftermath_Result r) -> std::string {
	return std::format("0x{:x}", static_cast<std::uint32_t>(r));
}

extern "C" void GFSDK_AFTERMATH_CALL gse_aftermath_gpu_crash_dump_cb(const void* dump, const std::uint32_t size, void* user) {
	auto* s = static_cast<gse::vulkan::aftermath::state*>(user);
	if (s == nullptr) {
		return;
	}
	const auto seq = s->dump_counter++;
	const auto stem = std::format("gse_{}_{}", gse::system_clock::timestamp_filename(), seq);
	s->last_dump_stem = stem;
	const auto path = gse::vulkan::write_dump_to_disk(s->cfg.dump_directory, stem, ".nv-gpudmp", dump, size);
	gse::log::println(gse::log::level::error, gse::log::category::vulkan, "Aftermath crash dump written: {}", path.string());
}

extern "C" void GFSDK_AFTERMATH_CALL gse_aftermath_shader_debug_info_cb(const void* dump, const std::uint32_t size, void* user) {
	auto* s = static_cast<gse::vulkan::aftermath::state*>(user);
	if (s == nullptr) {
		return;
	}
	const auto stem = s->last_dump_stem.empty()
		? std::format("gse_{}_shaderdebug", gse::system_clock::timestamp_filename())
		: s->last_dump_stem + "_shaderdebug";
	gse::vulkan::write_dump_to_disk(s->cfg.shader_directory, stem, ".nvdbg", dump, size);
}

extern "C" void GFSDK_AFTERMATH_CALL gse_aftermath_crash_dump_description_cb(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description, void*) {
	add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "GSEngine");
	add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "0.1.0");
}
#endif

gse::vulkan::aftermath::aftermath() : m_state(std::make_unique<state>()) {}

gse::vulkan::aftermath::~aftermath() {
	if (!m_state || !m_state->enabled) {
		return;
	}
#ifdef GSE_HAVE_AFTERMATH
	wait_for_crash_dump();
	GFSDK_Aftermath_DisableGpuCrashDumps();
#endif
	if (active_state == m_state.get()) {
		active_state = nullptr;
	}
	m_state->enabled = false;
}

auto gse::vulkan::aftermath::create(settings cfg) -> aftermath {
	if (cfg.dump_directory.empty()) {
		cfg.dump_directory = default_dump_directory();
	}
	if (cfg.shader_directory.empty()) {
		cfg.shader_directory = default_shader_directory();
	}

	aftermath tracker;
	tracker.m_state->cfg = std::move(cfg);
	tracker.m_state->diag_config = {
		.flags = build_diagnostics_flags(tracker.m_state->cfg),
	};

	const auto* runtime_flag = std::getenv("GSE_AFTERMATH_ENABLE");
	const bool runtime_opt_in = runtime_flag != nullptr
		&& (std::string_view(runtime_flag) == "1" || std::string_view(runtime_flag) == "true");

	if (!runtime_opt_in) {
		log::println(log::category::vulkan, "Nsight Aftermath compiled in but disabled at runtime (set GSE_AFTERMATH_ENABLE=1 to enable)");
		return tracker;
	}

#ifdef GSE_HAVE_AFTERMATH
	const auto result = GFSDK_Aftermath_EnableGpuCrashDumps(
		GFSDK_Aftermath_Version_API,
		GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
		GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
		&gse_aftermath_gpu_crash_dump_cb,
		&gse_aftermath_shader_debug_info_cb,
		&gse_aftermath_crash_dump_description_cb,
		nullptr,
		tracker.m_state.get()
	);

	if (GFSDK_Aftermath_SUCCEED(result)) {
		tracker.m_state->enabled = true;
		active_state = tracker.m_state.get();
		log::println(log::category::vulkan, "Nsight Aftermath crash dumps enabled (dumps -> {}, shaders -> {})", tracker.m_state->cfg.dump_directory.string(), tracker.m_state->cfg.shader_directory.string());
	}
	else {
		log::println(log::level::warning, log::category::vulkan, "Nsight Aftermath enable failed: {}", translate_result(result));
	}
#else
	log::println(log::category::vulkan, "Nsight Aftermath SDK not compiled in; crash dumps disabled");
#endif

	return tracker;
}

auto gse::vulkan::aftermath::available() const -> bool {
	return m_state && m_state->enabled;
}

auto gse::vulkan::aftermath::required_device_extensions() const -> std::span<const char* const> {
	if (!available()) {
		return {};
	}
	return std::span<const char* const>(k_required_device_extensions.data(), k_required_device_extensions.size());
}

auto gse::vulkan::aftermath::device_create_info_pnext(void* next) -> void* {
	if (!available()) {
		return next;
	}
	m_state->diag_config.pNext = next;
	return &m_state->diag_config;
}

auto gse::vulkan::aftermath::wait_for_crash_dump(time timeout) -> void {
	if (!available()) {
		return;
	}

#ifdef GSE_HAVE_AFTERMATH
	const auto timeout_ms = std::chrono::milliseconds(static_cast<std::int64_t>(timeout.as<milliseconds>()));
	const auto deadline = std::chrono::steady_clock::now() + timeout_ms;
	while (std::chrono::steady_clock::now() < deadline) {
		GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
		const auto result = GFSDK_Aftermath_GetCrashDumpStatus(&status);
		if (!GFSDK_Aftermath_SUCCEED(result)) {
			return;
		}
		if (status == GFSDK_Aftermath_CrashDump_Status_Finished
			|| status == GFSDK_Aftermath_CrashDump_Status_NotStarted) {
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	log::println(log::level::warning, log::category::vulkan, "Aftermath crash dump did not finish within {} ms", timeout.as<milliseconds>());
#else
	(void)timeout;
#endif
}

auto gse::vulkan::aftermath::register_spirv(std::span<const std::uint32_t> spirv) -> void {
	auto* s = active_state;
	if (s == nullptr || !s->enabled || spirv.empty()) {
		return;
	}

	const auto bytes = std::as_bytes(spirv);
	const std::string_view bytes_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	const auto hash = std::hash<std::string_view>{}(bytes_view);
	const auto filename = std::format("{:016x}.spv", hash);

	std::error_code ec;
	std::filesystem::create_directories(s->cfg.shader_directory, ec);
	const auto path = s->cfg.shader_directory / filename;
	if (std::filesystem::exists(path, ec)) {
		return;
	}

	std::ofstream out(path, std::ios::binary);
	if (out) {
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
}
