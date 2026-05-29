module;

#ifdef GSE_HAVE_AFTERMATH
#include <GFSDK_Aftermath_Defines.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#endif

export module gse.aftermath;

#ifdef GSE_HAVE_AFTERMATH
export {
	using ::GFSDK_Aftermath_Result;
	using ::GFSDK_Aftermath_CrashDump_Status;
	using ::PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription;

	using ::GFSDK_Aftermath_EnableGpuCrashDumps;
	using ::GFSDK_Aftermath_DisableGpuCrashDumps;
	using ::GFSDK_Aftermath_GetCrashDumpStatus;

	using ::GFSDK_Aftermath_Version_API;
	using ::GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan;
	using ::GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks;
	using ::GFSDK_Aftermath_CrashDump_Status_Unknown;
	using ::GFSDK_Aftermath_CrashDump_Status_Finished;
	using ::GFSDK_Aftermath_CrashDump_Status_NotStarted;
	using ::GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName;
	using ::GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion;

	inline auto aftermath_succeeded(const GFSDK_Aftermath_Result result) -> bool {
		return GFSDK_Aftermath_SUCCEED(result);
	}
}
#endif
