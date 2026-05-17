export module gse.gpu:shader_markers;

import std;

export namespace gse::gpu {
	struct dispatch_thread_id {
		std::uint32_t x = 0;
		std::uint32_t y = 0;
		std::uint32_t z = 0;
	};

	struct group_id {
		std::uint32_t x = 0;
		std::uint32_t y = 0;
		std::uint32_t z = 0;
	};

	struct group_thread_id {
		std::uint32_t x = 0;
		std::uint32_t y = 0;
		std::uint32_t z = 0;
	};

	struct group_index {
		std::uint32_t value = 0;
	};

	template <typename T>
	concept is_system_value =
		std::is_same_v<T, dispatch_thread_id> ||
		std::is_same_v<T, group_id> ||
		std::is_same_v<T, group_thread_id> ||
		std::is_same_v<T, group_index>;

	template <is_system_value T>
	consteval auto system_value_semantic() -> std::string_view;

	template <is_system_value T>
	consteval auto system_value_type_name() -> std::string_view;

	template <is_system_value T>
	consteval auto default_sv_name() -> std::string_view;
}

template <gse::gpu::is_system_value T>
consteval auto gse::gpu::system_value_semantic() -> std::string_view {
	if constexpr (std::is_same_v<T, dispatch_thread_id>) {
		return "SV_DispatchThreadID";
	}
	else if constexpr (std::is_same_v<T, group_id>) {
		return "SV_GroupID";
	}
	else if constexpr (std::is_same_v<T, group_thread_id>) {
		return "SV_GroupThreadID";
	}
	else if constexpr (std::is_same_v<T, group_index>) {
		return "SV_GroupIndex";
	}
}

template <gse::gpu::is_system_value T>
consteval auto gse::gpu::system_value_type_name() -> std::string_view {
	if constexpr (std::is_same_v<T, group_index>) {
		return "uint";
	}
	else {
		return "uint3";
	}
}

template <gse::gpu::is_system_value T>
consteval auto gse::gpu::default_sv_name() -> std::string_view {
	if constexpr (std::is_same_v<T, dispatch_thread_id>) {
		return "dispatch_id";
	}
	else if constexpr (std::is_same_v<T, group_id>) {
		return "group_id";
	}
	else if constexpr (std::is_same_v<T, group_thread_id>) {
		return "group_thread_id";
	}
	else if constexpr (std::is_same_v<T, group_index>) {
		return "group_index";
	}
}
