export module gse.gpu_backend:core;

import std;

import gse.assert;

export namespace gse::gpu {
	template <typename T>
	struct handle {
		std::uint64_t value = 0;

		constexpr auto operator==(
			const handle&
		) const -> bool = default;

		constexpr auto operator<=>(
			const handle&
		) const = default;

		explicit constexpr operator bool() const {
			return value != 0;
		}
	};

	using device_size = std::uint64_t;
	using device_address = std::uint64_t;
	using image_format_value = std::uint32_t;

	constexpr device_size whole_size = ~static_cast<device_size>(0);

	struct command_buffer_tag {};
	struct swap_chain_tag {};
	struct device_tag {};
	struct surface_tag {};

	struct image_view {};
	struct semaphore {};
	struct fence {};
	struct queue {};
	struct shader_object {};
	struct pipeline_layout {};
	struct physical_device {};

	using command_buffer_handle = handle<command_buffer_tag>;
	using swap_chain_handle = handle<swap_chain_tag>;
	using device_handle = handle<device_tag>;
	using surface = handle<surface_tag>;

	struct bindless_slot {
		static constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t index = invalid_index;

		[[nodiscard]] auto valid() const -> bool {
			return index != invalid_index;
		}
	};

	enum class result : std::int32_t {
		success,
		not_ready,
		timeout,
		event_set,
		event_reset,
		incomplete,
		suboptimal_khr,
		error_out_of_host_memory,
		error_out_of_device_memory,
		error_device_lost,
		error_out_of_date_khr,
		error_surface_lost_khr,
		error_present_timing_queue_full,
		error_unknown,
	};

	template <typename T>
	using expected = std::expected<T, result>;

	template <typename T>
	[[nodiscard]] auto must(
		expected<T> value,
		const std::source_location loc = std::source_location::current()
	) -> T;

	struct memory_requirements {
		device_size size = 0;
		device_size alignment = 0;
		std::uint32_t memory_type_bits = 0;
	};

	struct device_memory {
		std::uint64_t value = 0;

		constexpr auto operator==(
			const device_memory&
		) const -> bool = default;

		explicit constexpr operator bool() const {
			return value != 0;
		}
	};

	struct device_fault_counts {
		std::uint32_t address_info_count = 0;
		std::uint32_t vendor_info_count = 0;
		std::size_t vendor_binary_size = 0;
	};

	struct device_fault_address_info {
		std::uint32_t address_type = 0;
		std::string address_type_name;
		device_address reported_address = 0;
		device_address address_precision = 0;
	};

	struct device_fault_vendor_info {
		std::string description;
		std::uint64_t vendor_fault_code = 0;
		std::uint64_t vendor_fault_data = 0;
	};

	struct device_fault_info {
		std::string description;
		std::span<device_fault_address_info> address_infos;
		std::span<device_fault_vendor_info> vendor_infos;
		std::span<std::byte> vendor_binary;
	};

	struct acquire_next_image_result {
		gse::gpu::result result = gse::gpu::result::error_unknown;
		std::uint32_t image_index = 0;
	};
}

template <typename T>
auto gse::gpu::must(expected<T> value, const std::source_location loc) -> T {
	if (!value) {
		assert(false, "gpu operation failed (result {}) at {}:{}", static_cast<std::int32_t>(value.error()), loc.file_name(), loc.line());
		std::abort();
	}
	return std::move(*value);
}
