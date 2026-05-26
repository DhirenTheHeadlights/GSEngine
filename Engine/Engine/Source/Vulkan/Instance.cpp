module gse.vulkan;

import std;
import vulkan;

import :instance;

import gse.assert;
import gse.log;
import gse.os;

auto gse::vulkan::instance::create(const std::span<const char* const> required_extensions, const bool enable_validation) -> instance {
	vk::detail::defaultDispatchLoaderDynamic.init();

	std::vector<const char*> validation_layers;
	if (enable_validation) {
		constexpr auto layer_name = "VK_LAYER_KHRONOS_validation";
		const auto available = vk::enumerateInstanceLayerProperties();
		const bool layer_present = std::ranges::any_of(
			available,
			[&](const vk::LayerProperties& p) {
				return std::string_view(p.layerName) == layer_name;
			}
		);
		if (layer_present) {
			validation_layers.push_back(layer_name);
		}
		else {
			log::println(
				log::category::vulkan,
				"Validation layer '{}' requested but not available on this system; continuing without it. "
				"Install vulkan-validationlayers or set VK_LAYER_PATH to the SDK's explicit_layer.d directory.",
				layer_name
			);
		}
	}

	const std::uint32_t highest_supported_version = vk::enumerateInstanceVersion();
	const vk::ApplicationInfo app_info{
		.pApplicationName = "GSEngine",
		.applicationVersion = 1,
		.pEngineName = "GSEngine",
		.engineVersion = 1,
		.apiVersion = highest_supported_version,
	};

	std::vector extensions(required_extensions.begin(), required_extensions.end());
	extensions.push_back(vk::EXTDebugUtilsExtensionName);

	const auto available_instance_extensions = vk::enumerateInstanceExtensionProperties();
	const auto has_instance_extension = [&](const std::string_view name) {
		return std::ranges::any_of(available_instance_extensions, [&](const auto& p) {
			return std::string_view(p.extensionName) == name;
		});
	};

	const auto already_enabled = [&](const std::string_view name) {
		return std::ranges::any_of(extensions, [&](const char* e) {
			return std::string_view(e) == name;
		});
	};

	for (const auto* name : {
			 vk::KHRGetSurfaceCapabilities2ExtensionName,
			 vk::EXTSurfaceMaintenance1ExtensionName,
		 }) {
		if (has_instance_extension(name) && !already_enabled(name)) {
			extensions.push_back(name);
		}
	}

	auto debug_callback = [](const vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
							 vk::DebugUtilsMessageTypeFlagsEXT message_type,
							 const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
							 void* user_data) -> vk::Bool32 {
		if (message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
			return vk::False;
		}

		if (std::string_view(callback_data->pMessage).find("VK_EXT_debug_utils") != std::string_view::npos) {
			return vk::False;
		}

		const auto lvl = message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ? log::level::error
			: message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning			 ? log::level::warning
																							 : log::level::info;

		log::println(lvl, log::category::vulkan_validation, "{}", callback_data->pMessage);

		for (std::uint32_t i = 0; i < callback_data->objectCount; ++i) {
			const auto& object = callback_data->pObjects[i];
			if (!object.pObjectName || object.pObjectName[0] == '\0') {
				continue;
			}

			log::println(
				lvl,
				log::category::vulkan_validation,
				"Object {}: {} 0x{:x} '{}'",
				i,
				vk::to_string(object.objectType),
				object.objectHandle,
				object.pObjectName
			);
		}

		log::flush();

		return vk::False;
	};

	const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{
		.flags = {},
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
		.pfnUserCallback = debug_callback,
	};

	constexpr vk::ValidationFeatureEnableEXT enables[] = {
		vk::ValidationFeatureEnableEXT::eBestPractices,
		vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
		vk::ValidationFeatureEnableEXT::eDebugPrintf,
	};

	const vk::ValidationFeaturesEXT features{
		.pNext = &debug_create_info,
		.enabledValidationFeatureCount = static_cast<std::uint32_t>(std::size(enables)),
		.pEnabledValidationFeatures = enables,
		.disabledValidationFeatureCount = 0,
		.pDisabledValidationFeatures = nullptr,
	};

	const vk::InstanceCreateInfo create_info{
		.pNext = enable_validation ? static_cast<const void*>(&features) : nullptr,
		.flags = {},
		.pApplicationInfo = &app_info,
		.enabledLayerCount = static_cast<std::uint32_t>(validation_layers.size()),
		.ppEnabledLayerNames = validation_layers.empty() ? nullptr : validation_layers.data(),
		.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
		.ppEnabledExtensionNames = extensions.data(),
	};

	vk::raii::Instance instance = nullptr;
	vk::raii::Context context;

	try {
		instance = vk::raii::Instance(context, create_info);
		log::println(
			log::category::vulkan,
			"Vulkan Instance Created{}!",
			enable_validation ? " with validation layers" : ""
		);
	}
	catch (vk::SystemError& err) {
		log::println(
			log::level::error,
			log::category::vulkan,
			"Failed to create Vulkan instance: {}",
			err.what()
		);
		throw;
	}

	vk::detail::defaultDispatchLoaderDynamic.init(*instance);

	vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
	if (enable_validation) {
		try {
			debug_messenger = instance.createDebugUtilsMessengerEXT(debug_create_info);
			log::println(log::category::vulkan, "Debug Messenger Created Successfully!");
		}
		catch (vk::SystemError& err) {
			log::println(
				log::level::error,
				log::category::vulkan,
				"Failed to create Debug Messenger: {}",
				err.what()
			);
		}
	}

	return gse::vulkan::instance(
		std::move(context),
		std::move(instance),
		std::move(debug_messenger)
	);
}
