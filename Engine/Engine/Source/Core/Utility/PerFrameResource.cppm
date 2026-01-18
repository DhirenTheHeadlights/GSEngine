export module gse.utility:per_frame_resource;

import std;

import :misc;

import gse.assert;

export namespace gse {
	template<typename T, std::size_t N = 2>
	class per_frame_resource {
	public:
		static_assert(N >= 2, "per_frame_resource requires at least 2 frames");

		using value_type = T;
		static constexpr std::size_t frames_in_flight = N;

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
		std::array<std::optional<value_type>, N> m_resources{};
	};
}

template <typename T, std::size_t N>
gse::per_frame_resource<T, N>::per_frame_resource(const value_type& initial_value) {
	for (auto& resource : m_resources) {
		resource.emplace(initial_value);
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
	return self.m_resources;
}
