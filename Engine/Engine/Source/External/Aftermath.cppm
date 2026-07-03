module;

#ifdef GSE_HAVE_AFTERMATH
#include <GFSDK_Aftermath_Defines.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#ifndef VULKAN_H_
#define VULKAN_H_
#endif
#include <GFSDK_Aftermath_GpuCrashDumpDecoding.h>
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

	[[nodiscard]] auto shader_hash_spirv(
		std::span<const std::uint32_t> spirv
	) -> std::optional<std::uint64_t>;

	struct crash_fault_resource {
		std::uint64_t gpu_va;
		std::uint64_t size;
		std::uint64_t api_handle;
		std::uint32_t was_destroyed;
		char debug_name[260];
	};

	struct crash_fault_shader {
		std::uint64_t hash;
		std::uint32_t type;
	};

	struct crash_dump_analysis {
		std::uint32_t decoded;
		std::uint32_t has_page_fault;
		std::uint64_t faulting_va;
		std::uint32_t fault_type;
		std::uint32_t access_type;
		std::uint32_t resource_count;
		std::uint32_t shader_count;
		crash_fault_resource resources[8];
		crash_fault_shader shaders[8];
	};

	[[nodiscard]] auto analyze_crash_dump(
		const void* data,
		std::uint32_t size
	) -> crash_dump_analysis;
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

auto gse::aftermath::shader_hash_spirv([[maybe_unused]] std::span<const std::uint32_t> spirv) -> std::optional<std::uint64_t> {
#ifdef GSE_HAVE_AFTERMATH
	if (spirv.empty()) {
		return std::nullopt;
	}
	GFSDK_Aftermath_SpirvCode code{};
	code.pData = spirv.data();
	code.size = static_cast<std::uint32_t>(spirv.size_bytes());
	GFSDK_Aftermath_ShaderBinaryHash hash{};
	if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GetShaderHashSpirv(GFSDK_Aftermath_Version_API, &code, &hash))) {
		return std::nullopt;
	}
	return hash.hash;
#else
	return std::nullopt;
#endif
}

auto gse::aftermath::analyze_crash_dump([[maybe_unused]] const void* data, [[maybe_unused]] std::uint32_t size) -> crash_dump_analysis {
	crash_dump_analysis out{};
#ifdef GSE_HAVE_AFTERMATH
	GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
	if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API, data, size, &decoder))) {
		return out;
	}
	out.decoded = 1;

	GFSDK_Aftermath_GpuCrashDump_PageFaultInfo page_fault = {};
	if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &page_fault))) {
		out.has_page_fault = 1;
		out.faulting_va = page_fault.faultingGpuVA;
		out.fault_type = static_cast<std::uint32_t>(page_fault.faultType);
		out.access_type = static_cast<std::uint32_t>(page_fault.accessType);

		if (page_fault.resourceInfoCount > 0) {
			const std::uint32_t n = page_fault.resourceInfoCount < 8u ? page_fault.resourceInfoCount : 8u;
			GFSDK_Aftermath_GpuCrashDump_ResourceInfo infos[8] = {};
			if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetPageFaultResourceInfo(decoder, n, infos))) {
				out.resource_count = n;
				for (std::uint32_t i = 0; i < n; ++i) {
					out.resources[i].gpu_va = infos[i].gpuVa;
					out.resources[i].size = infos[i].size;
					out.resources[i].api_handle = infos[i].apiResource;
					out.resources[i].was_destroyed = infos[i].bWasDestroyed ? 1u : 0u;
					std::size_t j = 0;
					for (; j + 1 < sizeof(out.resources[i].debug_name) && infos[i].debugName[j] != '\0'; ++j) {
						out.resources[i].debug_name[j] = infos[i].debugName[j];
					}
					out.resources[i].debug_name[j] = '\0';
				}
			}
		}
	}

	std::uint32_t shader_count = 0;
	if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfoCount(decoder, &shader_count)) && shader_count > 0) {
		const std::uint32_t n = shader_count < 8u ? shader_count : 8u;
		GFSDK_Aftermath_GpuCrashDump_ShaderInfo infos[8] = {};
		if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfo(decoder, n, infos))) {
			out.shader_count = n;
			for (std::uint32_t i = 0; i < n; ++i) {
				out.shaders[i].hash = infos[i].shaderHash;
				out.shaders[i].type = static_cast<std::uint32_t>(infos[i].shaderType);
			}
		}
	}

	GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder);
#endif
	return out;
}
