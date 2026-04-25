export module gse.concurrency:per_frame_resource;

import std;

import gse.assert;
import gse.core;

export namespace gse {
	template<typename T, std::size_t N = 2>
	class per_frame_resource {
	public:
		static_assert(N >= 2, "per_frame_resource requires at least 2 frames");

		using value_type = T;
		using storage_type = std::array<T, N>;
		static constexpr std::size_t frames_in_flight = N;

		per_frame_resource(
		) requires std::is_default_constructible_v<T> = default;

		template <std::convertible_to<T>... Args>
			requires (sizeof...(Args) == N)
		explicit per_frame_resource(
			Args&&... slots
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
template <std::convertible_to<T>... Args>
	requires (sizeof...(Args) == N)
gse::per_frame_resource<T, N>::per_frame_resource(Args&&... slots) : m_resources{ std::forward<Args>(slots)... } {}

template <typename T, std::size_t N>
auto gse::per_frame_resource<T, N>::operator[](this auto&& self, const std::size_t frame_index) -> decltype(auto) {
	assert(
		frame_index < frames_in_flight,
		std::source_location::current(),
		"Frame index {} out of bounds (max: {})",
		frame_index,
		frames_in_flight - 1
	);

	return self.m_resources[frame_index];
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
