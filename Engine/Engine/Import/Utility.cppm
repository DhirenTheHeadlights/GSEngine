export module gse.utility;

export import :clock;
export import :component;
export import :config;
export import :double_buffer;
export import :ecs;
export import :id;
export import :misc;
export import :non_copyable;
export import :scope_exit;
export import :system_clock;
export import :task;
export import :timed_lock;
export import :timer;
export import :variant_match;
export import :interval_timer;

export namespace gse {
	template <typename T>
	concept is_trivially_copyable = std::is_trivially_copyable_v<T>;

	auto scope(
		const std::function<void()>& in_scope
	) -> void;
}

auto gse::scope(const std::function<void()>& in_scope) -> void {
	in_scope();
}
