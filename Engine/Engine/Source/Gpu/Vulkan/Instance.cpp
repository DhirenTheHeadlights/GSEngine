module gse.gpu;

import std;
import vulkan;

import :vulkan_instance;

import gse.assert;
import gse.config;
import gse.log;
import gse.os;
import gse.save;

auto gse::vulkan::create_surface(const window& win, instance& instance) -> void {
	const auto raw_surface = win.create_vulkan_surface(*instance.raii_instance());
	instance.set_surface(vk::raii::SurfaceKHR(instance.raii_instance(), raw_surface));
}

auto gse::vulkan::instance::create(const std::span<const char* const> required_extensions, save::state& save) -> instance {
	const bool enable_validation = save::read_bool_setting_early(
		config::resource_path / "Misc/settings.toml",
		"Graphics",
		"Validation Layers",
		true
	);

	std::vector<const char*> validation_layers;
	if (enable_validation) {
		validation_layers.push_back("VK_LAYER_KHRONOS_validation");
	}

	vk::detail::defaultDispatchLoaderDynamic.init();

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

	auto debug_callback = [](
		const vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
		vk::DebugUtilsMessageTypeFlagsEXT message_type,
		const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data
	) -> vk::Bool32 {
		if (message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
			return vk::False;
		}

		if (std::string_view(callback_data->pMessage).find("VK_EXT_debug_utils") != std::string_view::npos) {
			return vk::False;
		}

		const auto lvl =
			message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ? log::level::error :
			message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ? log::level::warning :
			log::level::info;

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
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
		.pfnUserCallback = debug_callback,
	};

	constexpr vk::ValidationFeatureEnableEXT enables[] = {
		vk::ValidationFeatureEnableEXT::eBestPractices,
		vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
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
		log::println(log::category::vulkan, "Vulkan Instance Created{}!", enable_validation ? " with validation layers" : "");
	} catch (vk::SystemError& err) {
		log::println(log::level::error, log::category::vulkan, "Failed to create Vulkan instance: {}", err.what());
		throw;
	}

	vk::detail::defaultDispatchLoaderDynamic.init(*instance);

	vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
	if (enable_validation) {
		try {
			debug_messenger = instance.createDebugUtilsMessengerEXT(debug_create_info);
			log::println(log::category::vulkan, "Debug Messenger Created Successfully!");
		} catch (vk::SystemError& err) {
			log::println(log::level::error, log::category::vulkan, "Failed to create Debug Messenger: {}", err.what());
		}
	}

	return gse::vulkan::instance(std::move(context), std::move(instance), std::move(debug_messenger));
}
