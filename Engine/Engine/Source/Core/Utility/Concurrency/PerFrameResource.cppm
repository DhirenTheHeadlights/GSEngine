export module gse.utility:per_frame_resource;

import std;

import :misc;

import gse.assert;

namespace gse {
	thread_local const std::uint32_t* expected_frame_index = nullptr;
}

export namespace gse {
	template<typename T, std::size_t N = 2>
	class per_frame_resource {
	public:
		static_assert(N >= 2, "per_frame_resource requires at least 2 frames");

		using value_type = T;
		static constexpr std::size_t frames_in_flight = N;
		static constexpr bool is_direct_storage = std::is_default_constructible_v<T>;

		using storage_type = std::conditional_t<
			is_direct_storage,
			std::array<T, N>,
			std::array<std::optional<T>, N>
		>;

		per_frame_resource() = default;

		explicit per_frame_resource(
			const value_type& initial_value
		);

		auto operator[](
			this auto&& self,
			std::size_t frame_index
		) -> decltype(auto);

		auto begin(
			this auto&& self
		) -> decltype(auto);

		auto end(
			this auto&& self
		) -> decltype(auto);

		auto resources(
			this auto&& self
		) -> decltype(auto);

		explicit operator value_type&() = delete;
		explicit operator const value_type&() const = delete;
	private:
		storage_type m_resources{};
	};
}

template <typename T, std::size_t N>
gse::per_frame_resource<T, N>::per_frame_resource(const value_type& initial_value) {
	if constexpr (is_direct_storage) {
		for (auto& resource : m_resources) {
			resource = initial_value;
		}
	} else {
		for (auto& resource : m_resources) {
			resource.emplace(initial_value);
		}
	}
}

template <typename T, std::size_t N>
auto gse::per_frame_resource<T, N>::operator[](this auto&& self, const std::size_t frame_index) -> decltype(auto) {
	assert(
		frame_index < frames_in_flight,
		std::source_location::current(),
		"Frame index {} out of bounds (max: {})",
		frame_index,
		frames_in_flight - 1
	);

#ifndef NDEBUG
	if (expected_frame_index != nullptr) {
		assert(
			frame_index == *expected_frame_index,
			std::source_location::current(),
			"Frame index mismatch: got {}, expected {}",
			frame_index,
			*expected_frame_index
		);
	}
#endif

	if constexpr (is_direct_storage) {
		return self.m_resources[frame_index];
	} else {
		return self.m_resources[frame_index];
	}
}

template <typename T, std::size_t N>
auto gse::per_frame_resource<T, N>::begin(this auto&& self) -> decltype(auto) {
	return self.m_resources.begin();
}

template <typename T, std::size_t N>
auto gse::per_frame_resource<T, N>::end(this auto&& self) -> decltype(auto) {
	return self.m_resources.end();
}

template <typename T, std::size_t N>
auto gse::per_frame_resource<T, N>::resources(this auto&& self) -> decltype(auto) {
	return (self.m_resources);
}
