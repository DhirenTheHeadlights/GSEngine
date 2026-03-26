export module gse.utility:system_clock;

import std;
import gse.math;

import :clock;

export namespace gse::system_clock {
	using internal_time = time_t<>;
	using default_time = time_t<float, seconds>;

	auto update() -> void;

	template <is_quantity Q = default_time>
	auto dt() -> Q;

	template <is_quantity Q = default_time>
	auto now() -> Q;

	template <is_quantity Q = default_time>
	auto constant_update_time() -> Q;

	auto fps() -> std::uint32_t;
}

namespace gse::system_clock {
	clock main_clock;
	clock dt_clock;

	internal_time delta_time{};
	internal_time frame_rate_update_time{};

	constexpr internal_time fps_report_interval = seconds(1.0);
	constexpr internal_time const_update_time = milliseconds(10.0);

	std::uint32_t frame_count = 0;
	std::uint32_t frame_rate_count = 0;
}

auto gse::system_clock::update() -> void {
	delta_time = gse::quantity_cast<internal_time>(dt_clock.reset<double>());

	frame_count++;
	frame_rate_update_time += delta_time;

	if (frame_rate_update_time >= fps_report_interval) {
		frame_rate_count = static_cast<std::uint32_t>(frame_count / frame_rate_update_time.as<seconds>());
		frame_count = 0;
		frame_rate_update_time -= fps_report_interval;
	}
}

template <gse::is_quantity Q>
auto gse::system_clock::dt() -> Q {
	return gse::quantity_cast<Q>(std::min(delta_time, const_update_time));
}

template <gse::is_quantity Q>
auto gse::system_clock::now() -> Q {
	return gse::quantity_cast<Q>(main_clock.elapsed<double>());
}

template <gse::is_quantity Q>
auto gse::system_clock::constant_update_time() -> Q {
	return gse::quantity_cast<Q>(const_update_time);
}

auto gse::system_clock::fps() -> std::uint32_t {
	return frame_rate_count;
}
