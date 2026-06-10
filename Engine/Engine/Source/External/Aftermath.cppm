module;

#ifdef GSE_HAVE_AFTERMATH
#include <GFSDK_Aftermath_Defines.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#endif

export module gse.aftermath;

import std;

export namespace gse::aftermath {
	using crash_dump_fn = void (*)(const void* data, std::uint32_t size);
	using shader_debug_fn = void (*)(const void* data, std::uint32_t size);

	enum class dump_status : std::uint8_t {
		unknown,
		not_started,
		collecting,
		finished,
		failed
	};

	constexpr bool compiled_in =
#ifdef GSE_HAVE_AFTERMATH
		true;
#else
		false;
#endif

	[[nodiscard]] auto enable(
		crash_dump_fn on_dump,
		shader_debug_fn on_shader,
		std::string_view app_name,
		std::string_view app_version
	) -> bool;

	auto disable() -> void;

	[[nodiscard]] auto status() -> dump_status;
}

namespace gse::aftermath {
	struct callback_registry {
		crash_dump_fn on_dump = nullptr;
		shader_debug_fn on_shader = nullptr;
		std::string app_name;
		std::string app_version;
	};

	inline callback_registry registry;

#ifdef GSE_HAVE_AFTERMATH
	auto dump_trampoline(
		const void* data,
		std::uint32_t size,
		void*
	) -> void;

	auto shader_trampoline(
		const void* data,
		std::uint32_t size,
		void*
	) -> void;

	auto description_trampoline(
		PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description,
		void*
	) -> void;
#endif
}

#ifdef GSE_HAVE_AFTERMATH
auto gse::aftermath::dump_trampoline(const void* data, std::uint32_t size, void*) -> void {
	if (registry.on_dump) {
		registry.on_dump(data, size);
	}
}

auto gse::aftermath::shader_trampoline(const void* data, std::uint32_t size, void*) -> void {
	if (registry.on_shader) {
		registry.on_shader(data, size);
	}
}

auto gse::aftermath::description_trampoline(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description, void*) -> void {
	add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, registry.app_name.c_str());
	add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, registry.app_version.c_str());
}
#endif

auto gse::aftermath::enable(crash_dump_fn on_dump, shader_debug_fn on_shader, std::string_view app_name, std::string_view app_version) -> bool {
	registry.on_dump = on_dump;
	registry.on_shader = on_shader;
	registry.app_name = app_name;
	registry.app_version = app_version;

#ifdef GSE_HAVE_AFTERMATH
	const auto result = GFSDK_Aftermath_EnableGpuCrashDumps(
		GFSDK_Aftermath_Version_API,
		GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
		GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
		&dump_trampoline,
		&shader_trampoline,
		&description_trampoline,
		nullptr,
		nullptr
	);
	return GFSDK_Aftermath_SUCCEED(result);
#else
	return false;
#endif
}

auto gse::aftermath::disable() -> void {
#ifdef GSE_HAVE_AFTERMATH
	GFSDK_Aftermath_DisableGpuCrashDumps();
#endif
	registry.on_dump = nullptr;
	registry.on_shader = nullptr;
}

auto gse::aftermath::status() -> dump_status {
#ifdef GSE_HAVE_AFTERMATH
	GFSDK_Aftermath_CrashDump_Status raw = GFSDK_Aftermath_CrashDump_Status_Unknown;
	if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GetCrashDumpStatus(&raw))) {
		return dump_status::failed;
	}
	switch (raw) {
		case GFSDK_Aftermath_CrashDump_Status_NotStarted:
			return dump_status::not_started;
		case GFSDK_Aftermath_CrashDump_Status_CollectingData:
		case GFSDK_Aftermath_CrashDump_Status_InvokingCallback:
			return dump_status::collecting;
		case GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed:
			return dump_status::failed;
		case GFSDK_Aftermath_CrashDump_Status_Finished:
			return dump_status::finished;
		default:
			return dump_status::unknown;
	}
#else
	return dump_status::unknown;
#endif
}
