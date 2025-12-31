export module gse.utility;

export import :clock;
export import :component;
export import :concepts;
export import :config;
export import :default_scene_hook;
export import :double_buffer;
export import :entity;
export import :entity_hook;
export import :frame_sync;
export import :hook;
export import :hookable;
export import :interval_timer;
export import :id;
export import :lambda_traits;
export import :misc;
export import :mpsc_ring_buffer;
export import :non_copyable;
export import :registry;
export import :scene_hook;
export import :scene;
export import :scheduler;
export import :scope_exit;
export import :spsc_ring_buffer;
export import :system;
export import :system_clock;
export import :task;
export import :timed_lock;
export import :timer;
export import :trace;
export import :variant_match;

export namespace gse {
	template <typename T>
	concept is_trivially_copyable = std::is_trivially_copyable_v<T>;

	auto scope(
		const std::function<void()>& in_scope
	) -> void;

	template <is_trivially_copyable... Src>
	auto memcpy(
		std::byte* dest, 
		const Src*... src
	) -> void;
}

auto gse::scope(const std::function<void()>& in_scope) -> void {
	in_scope();
}

template <gse::is_trivially_copyable ... Src>
auto gse::memcpy(std::byte* dest, const Src*... src) -> void {
	std::byte* out = dest;
	((std::memcpy(out, src, sizeof(Src)), out += sizeof(Src)), ...);
}

