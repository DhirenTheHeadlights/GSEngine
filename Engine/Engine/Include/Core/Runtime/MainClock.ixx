export module gse.runtime:main_clock;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse::main_clock {
	auto update() -> void;

	auto dt() -> time;
	auto now() -> time;
	auto fps() -> std::uint32_t;

	template <typename T, typename U, int N>
	auto per(vec_t<T, N> value) -> vec_t<T, N>;

	template <typename T, typename U>
	auto per(T value) -> T;

	constexpr time const_update_time = seconds(1.f / 100.f);
}

namespace gse::main_clock {
	clock main_clock;
	clock dt_clock;

	static constinit time delta_time;
	static constinit time frame_rate_update_time;

	static constinit std::uint32_t frame_count = 0;
	static constinit std::uint32_t frame_rate_count = 0;

	constexpr time fps_report_interval = seconds(1.0f);
}

auto gse::main_clock::update() -> void {
	delta_time = std::min(dt_clock.reset(), const_update_time);

	frame_count++;
	frame_rate_update_time += delta_time;

	if (frame_rate_update_time >= fps_report_interval) {
		frame_rate_count = static_cast<std::uint32_t>(frame_count / frame_rate_update_time.as<units::seconds>());
		frame_count = 0;
		frame_rate_update_time -= fps_report_interval;
	}
}

auto gse::main_clock::dt() -> time {
	return delta_time;
}

auto gse::main_clock::now() -> time {
	return main_clock.elapsed();
}

auto gse::main_clock::fps() -> std::uint32_t {
	return frame_rate_count;
}

template <typename T, typename U, int N>
auto gse::main_clock::per(vec_t<T, N> value) -> vec_t<T, N> {
	return value * dt().as<U>();
}

template <typename T, typename U>
auto gse::main_clock::per(T value) -> T {
	return value * dt().as<U>();
}
