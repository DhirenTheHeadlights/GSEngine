export module gse.utility;

export import gse.config;

import std;

export import :access_token;
export import :archive;
export import :async_task;

export import :flat_map;
export import :frame_arena;
export import :linear_vector;
export import :slot_map;
export import :static_vector;

export import :channel_base;
export import :channel_promise;
export import :clock;
export import :component;
export import :concepts;
export import :default_scene_hook;
export import :n_buffer;
export import :per_frame_resource;
export import :flags;
export import :frame_sync;
export import :hook;
export import :hookable;
export import :interval_timer;
export import :id;
export import :lambda_traits;
export import :misc;
export import :move_only_function;
export import :mpsc_ring_buffer;
export import :non_copyable;
export import :percentage;
export import :profile_aggregator;
export import :registry;
export import :scene_hook;
export import :scene_hook_impl;
export import :scene;
export import :scheduler;
export import :scope_exit;
export import :spsc_ring_buffer;
export import :save_system;
export import :phase_context;
export import :system_node;
export import :system_clock;
export import :task;
export import :task_graph;
export import :timed_lock;
export import :timer;
export import :trace;
export import :variant_match;
export import :file_watcher;
export import :frame_context;
export import :update_context;

export namespace gse {
	template <typename T>
	concept is_trivially_copyable = std::is_trivially_copyable_v<T>;

	template <typename T>
	concept contiguous_byte_source = requires(const T& c) {
		std::ranges::data(c);
		std::ranges::size(c);
	};

	auto scope(
		const std::function<void()>& in_scope
	) -> void;

	template <is_trivially_copyable... Src>
	auto memcpy(
		std::byte* dest,
		const Src&... src
	) -> void requires (!std::is_pointer_v<Src> && ...);

	template <contiguous_byte_source Container>
	auto memcpy(
		std::byte* dest,
		const Container& src
	) -> void;

	template <is_trivially_copyable T>
	auto memcpy(
		T& dest,
		const std::byte* src
	) -> void;

	template <is_trivially_copyable T, std::size_t N>
	auto memcpy(
		std::byte* dest,
		const T (&src)[N]
	) -> void;

	auto memcpy(
		std::byte* dest,
		const void* src,
		std::size_t size
	) -> void;
}

auto gse::scope(const std::function<void()>& in_scope) -> void {
	in_scope();
}

template <gse::is_trivially_copyable ... Src>
auto gse::memcpy(std::byte* dest, const Src&... src) -> void requires (!std::is_pointer_v<Src> && ...) {
	std::byte* out = dest;
	((std::memcpy(out, std::addressof(src), sizeof(Src)), out += sizeof(Src)), ...);
}

template <gse::contiguous_byte_source Container>
auto gse::memcpy(std::byte* dest, const Container& src) -> void {
	using value_type = std::remove_cvref_t<decltype(*std::ranges::data(src))>;
	const std::size_t byte_size = std::ranges::size(src) * sizeof(value_type);
	std::memcpy(dest, std::ranges::data(src), byte_size);
}

template <gse::is_trivially_copyable T>
auto gse::memcpy(T& dest, const std::byte* src) -> void {
	std::memcpy(std::addressof(dest), src, sizeof(T));
}

template <gse::is_trivially_copyable T, std::size_t N>
auto gse::memcpy(std::byte* dest, const T (&src)[N]) -> void {
	std::memcpy(dest, src, sizeof(T) * N);
}

auto gse::memcpy(std::byte* dest, const void* src, const std::size_t size) -> void {
	std::memcpy(dest, src, size);
}

