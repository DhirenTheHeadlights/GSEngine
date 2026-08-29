export module gse.meta:fixed_string;

import std;

export namespace gse {
	template <std::size_t N>
	struct fixed_string {
		char data[N]{};

		fixed_string() = default;

		consteval fixed_string(
			const char (&s)[N]
		);

		[[nodiscard]] consteval auto size() const -> std::size_t;

		consteval operator std::string_view() const;
	};

	template <std::size_t N>
	fixed_string(
		const char (&)[N]
	) -> fixed_string<N>;
}

template <std::size_t N>
consteval gse::fixed_string<N>::fixed_string(const char (&s)[N]) {
	std::ranges::copy(s, data);
}

template <std::size_t N>
consteval auto gse::fixed_string<N>::size() const -> std::size_t {
	std::size_t length = 0;
	while (length < N && data[length] != '\0') {
		++length;
	}
	return length;
}

template <std::size_t N>
consteval gse::fixed_string<N>::operator std::string_view() const {
	return { data, size() };
}
