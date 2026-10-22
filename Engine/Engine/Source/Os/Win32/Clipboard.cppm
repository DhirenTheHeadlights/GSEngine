export module gse.os:clipboard;

import std;

import :window;

import gse.math;

export namespace gse::clipboard {
	struct image {
		std::filesystem::path path;
		vec2u size;
		std::vector<std::byte> pixels;
	};

	[[nodiscard]] auto text() -> std::string;

	auto set_text(
		std::string value
	) -> void;

	[[nodiscard]] auto image_available() -> bool;

	auto request_image() -> void;

	[[nodiscard]] auto image_pending() -> bool;

	[[nodiscard]] auto take_image() -> std::optional<image>;

	auto sync(
		native_window_handle owner
	) -> void;
}

namespace gse::clipboard {
	struct bridge {
		std::mutex guard;
		std::string cached_text;
		std::optional<std::string> pending_text;
		std::optional<image> ready_image;
		bool primed = false;
		std::uint32_t sequence = 0;
		std::atomic<bool> image_present{ false };
		std::atomic<bool> image_wanted{ false };
	};

	inline bridge state;
}

auto gse::clipboard::text() -> std::string {
	const std::scoped_lock _(state.guard);
	if (state.pending_text) {
		return *state.pending_text;
	}
	return state.cached_text;
}

auto gse::clipboard::set_text(std::string value) -> void {
	const std::scoped_lock _(state.guard);
	state.pending_text = std::move(value);
}

auto gse::clipboard::image_available() -> bool {
	return state.image_present.load(std::memory_order_acquire);
}

auto gse::clipboard::request_image() -> void {
	state.image_wanted.store(true, std::memory_order_release);
}

auto gse::clipboard::image_pending() -> bool {
	return state.image_wanted.load(std::memory_order_acquire);
}

auto gse::clipboard::take_image() -> std::optional<image> {
	const std::scoped_lock _(state.guard);
	return std::exchange(state.ready_image, std::nullopt);
}
