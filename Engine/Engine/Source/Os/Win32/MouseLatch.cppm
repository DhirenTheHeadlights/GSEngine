export module gse.os:mouse_latch;

import std;

export namespace gse::input {
	struct mouse_totals {
		std::uint32_t x = 0;
		std::uint32_t y = 0;

		[[nodiscard]] auto operator==(const mouse_totals&) const -> bool = default;
	};

	auto advance_mouse_latch(
		std::int32_t dx,
		std::int32_t dy
	) -> mouse_totals;

	[[nodiscard]] auto mouse_latch_totals() -> mouse_totals;

	auto set_mouse_latch_sink(
		std::uint64_t* sink
	) -> void;
}

namespace gse::input {
	struct mouse_latch_state {
		std::mutex mutex;
		mouse_totals totals;
		std::uint64_t* sink = nullptr;
	};

	auto latch_state() -> mouse_latch_state&;

	auto pack(
		mouse_totals totals
	) -> std::uint64_t;
}

auto gse::input::latch_state() -> mouse_latch_state& {
	static mouse_latch_state state;
	return state;
}

auto gse::input::pack(const mouse_totals totals) -> std::uint64_t {
	return static_cast<std::uint64_t>(totals.x) | static_cast<std::uint64_t>(totals.y) << 32;
}

auto gse::input::advance_mouse_latch(const std::int32_t dx, const std::int32_t dy) -> mouse_totals {
	auto& state = latch_state();
	std::scoped_lock lock(state.mutex);
	state.totals.x += static_cast<std::uint32_t>(dx);
	state.totals.y += static_cast<std::uint32_t>(dy);
	if (state.sink) {
		std::atomic_ref(*state.sink).store(pack(state.totals), std::memory_order_release);
	}
	return state.totals;
}

auto gse::input::mouse_latch_totals() -> mouse_totals {
	auto& state = latch_state();
	std::scoped_lock lock(state.mutex);
	return state.totals;
}

auto gse::input::set_mouse_latch_sink(std::uint64_t* sink) -> void {
	auto& state = latch_state();
	std::scoped_lock lock(state.mutex);
	state.sink = sink;
	if (sink) {
		std::atomic_ref(*sink).store(pack(state.totals), std::memory_order_release);
	}
}
