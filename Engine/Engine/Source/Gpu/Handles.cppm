export module gse.gpu:handles;

import std;
import gse.meta;

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
	using image_layout_value = std::uint32_t;
}
