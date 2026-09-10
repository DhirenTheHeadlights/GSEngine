export module gse.os:frame_demand;

import std;

import gse.math;
import gse.time;

export namespace gse::frame_demand {
	auto request_redraw() -> void;

	auto request_frames(
		time duration
	) -> void;

	auto request_interaction() -> void;

	[[nodiscard]] auto active() -> bool;

	auto consume_redraw() -> void;
}

namespace gse::frame_demand {
	using deadline = time_t<double, seconds>;

	constexpr time interaction_hold = milliseconds(150.f);

	inline std::atomic<bool> redraw_pending{ true };
	inline std::atomic<deadline> hold_until{ deadline{} };
}

auto gse::frame_demand::request_redraw() -> void {
	redraw_pending.store(true, std::memory_order_release);
}

auto gse::frame_demand::request_frames(const time duration) -> void {
	const auto target = system_clock::now<deadline>() + duration;

	auto current = hold_until.load(std::memory_order_acquire);
	while (current < target && !hold_until.compare_exchange_weak(current, target, std::memory_order_acq_rel, std::memory_order_acquire)) {
	}

	redraw_pending.store(true, std::memory_order_release);
}

auto gse::frame_demand::request_interaction() -> void {
	request_frames(interaction_hold);
}

auto gse::frame_demand::active() -> bool {
	if (redraw_pending.load(std::memory_order_acquire)) {
		return true;
	}
	return system_clock::now<deadline>() < hold_until.load(std::memory_order_acquire);
}

auto gse::frame_demand::consume_redraw() -> void {
	redraw_pending.store(false, std::memory_order_release);
}
