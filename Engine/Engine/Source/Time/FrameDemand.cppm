export module gse.time:frame_demand;

import std;

import gse.math;

import :system_clock;

export namespace gse::frame_demand {
	using waker = void(*)();

	auto install_waker(
		waker fn
	) -> void;

	auto clear_waker() -> void;

	auto request_redraw() -> void;

	auto request_frame_at(
		time_t<double, seconds> deadline
	) -> void;

	[[nodiscard]] auto active() -> bool;

	[[nodiscard]] auto wait_budget(
		time floor
	) -> time;

	auto consume_redraw() -> void;
}

namespace gse::frame_demand {
	using deadline = time_t<double, seconds>;

	auto no_deadline() -> deadline;

	inline std::atomic<bool> redraw_pending{ true };
	inline std::atomic<deadline> next_deadline{ no_deadline() };
	inline std::atomic<waker> installed_waker{ nullptr };
}

auto gse::frame_demand::no_deadline() -> deadline {
	return seconds(std::numeric_limits<double>::infinity());
}

auto gse::frame_demand::install_waker(const waker fn) -> void {
	installed_waker.store(fn, std::memory_order_release);
}

auto gse::frame_demand::clear_waker() -> void {
	installed_waker.store(nullptr, std::memory_order_release);
}

auto gse::frame_demand::request_redraw() -> void {
	redraw_pending.store(true, std::memory_order_release);
	if (const waker fn = installed_waker.load(std::memory_order_acquire)) {
		fn();
	}
}

auto gse::frame_demand::request_frame_at(const deadline target) -> void {
	auto current = next_deadline.load(std::memory_order_acquire);
	while (target < current && !next_deadline.compare_exchange_weak(current, target, std::memory_order_acq_rel, std::memory_order_acquire)) {
	}
}

auto gse::frame_demand::active() -> bool {
	return redraw_pending.load(std::memory_order_acquire);
}

auto gse::frame_demand::wait_budget(const time floor) -> time {
	const auto target = next_deadline.exchange(no_deadline(), std::memory_order_acq_rel);
	if (target == no_deadline()) {
		return floor;
	}
	const auto now = system_clock::now<deadline>();
	if (target <= now) {
		return {};
	}
	return std::min(time(target - now), floor);
}

auto gse::frame_demand::consume_redraw() -> void {
	redraw_pending.store(false, std::memory_order_release);
}
