export module gse.utility:clock;

import std;
import gse.physics.math;

export namespace gse {
	class clock {
	public:
		clock();

		template <typename T = float>
		auto reset() -> time_t<T>;

		template <typename T = float>
		auto elapsed() const -> time_t<T>;
	private:
		std::chrono::steady_clock::time_point m_start_time;
	};
}

gse::clock::clock(): m_start_time(std::chrono::steady_clock::now()) {}

template <typename T>
auto gse::clock::reset() -> time_t<T> {
	const auto e = elapsed<T>();
    m_start_time = std::chrono::steady_clock::now();
    return e;
}

template <typename T>
auto gse::clock::elapsed() const -> time_t<T> {
	const auto now = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_start_time).count();
	return nanoseconds(static_cast<T>(ns));
}