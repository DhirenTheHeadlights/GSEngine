export module gse.utility:percentage;

import std;

export namespace gse {
	template <typename T> requires std::is_floating_point_v<T>
	class percentage {
	public:
		constexpr static auto scalar(
			const T& value = static_cast<T>(0)
		) -> percentage;

		constexpr static auto fraction(
			const T& value = static_cast<T>(0)
		) -> percentage;

		constexpr static auto ratio(
			const T& numerator = static_cast<T>(0),
			const T& denominator = static_cast<T>(1)
		) -> percentage;

		constexpr static auto one(
		) -> percentage;

		constexpr static auto zero(
		) -> percentage;

		enum struct bound {
			one_to_hundred,
			zero_to_one,
			unbounded_one_to_hundred,
			unbounded_zero_to_one,
		};

		constexpr auto value(
			bound b = bound::unbounded_zero_to_one
		) const -> T;
	private:
		explicit constexpr percentage(
			const T& value = static_cast<T>(0)
		);

		T m_value = static_cast<T>(0);
	};
}

template <typename T> requires std::is_floating_point_v<T>
constexpr auto gse::percentage<T>::scalar(const T& value) -> percentage {
	return percentage(value / static_cast<T>(100));
}

template <typename T> requires std::is_floating_point_v<T>
constexpr auto gse::percentage<T>::fraction(const T& value) -> percentage {
	return percentage(value);
}

template <typename T> requires std::is_floating_point_v<T>
constexpr auto gse::percentage<T>::ratio(const T& numerator, const T& denominator) -> percentage {
	return percentage(numerator / denominator);
}

template <typename T> requires std::is_floating_point_v<T>
constexpr auto gse::percentage<T>::one() -> percentage {
	return percentage(static_cast<T>(1));
}

template <typename T> requires std::is_floating_point_v<T>
constexpr auto gse::percentage<T>::zero() -> percentage {
	return percentage(static_cast<T>(0));
}

template <typename T> requires std::is_floating_point_v<T>
constexpr auto gse::percentage<T>::value(const bound b) const -> T {
	switch (b) {
	case bound::one_to_hundred:
		return std::clamp(m_value * static_cast<T>(100), static_cast<T>(0), static_cast<T>(100));
	case bound::zero_to_one:
		return std::clamp(m_value, static_cast<T>(0), static_cast<T>(1));
	case bound::unbounded_one_to_hundred:
		return m_value * static_cast<T>(100);
	case bound::unbounded_zero_to_one:
		return m_value;
	}
	return m_value;
}

template <typename T> requires std::is_floating_point_v<T>
constexpr gse::percentage<T>::percentage(const T& value) : m_value(value) {}
