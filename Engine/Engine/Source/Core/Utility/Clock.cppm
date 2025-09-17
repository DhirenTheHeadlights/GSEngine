export module gse.utility:clock;

import std;
import gse.physics.math;

export namespace gse {
	class clock {
	public:
		clock() : m_start_time(std::chrono::steady_clock::now()) {}

		auto reset() -> time;
		auto elapsed() const -> time;
	private:
		std::chrono::steady_clock::time_point m_start_time;
	};
}

auto gse::clock::reset() -> time {
	const time elapsed_time = elapsed();
	m_start_time = std::chrono::steady_clock::now();
	return elapsed_time;
}

auto gse::clock::elapsed() const -> time {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsed_time = now - m_start_time;
	return seconds(elapsed_time.count());
}