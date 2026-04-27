export module gse.ecs:task_context;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.diag;

import :phase_context;
import :registries;

export namespace gse {
	struct task_context {
		void* gpu_ctx = nullptr;
		void* assets = nullptr;
		state_registry& states;
		resource_registry& resources_store;
		channel_registry& channels_store;
		channel_writer& channels;
		task_graph& graph;
		bool live_state = true;

		template <typename T>
		auto get(
		) const -> T&;

		template <typename T>
		auto try_get(
		) const -> T*;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;

		template <typename T>
		auto read_channel(
		) const -> channel_read_guard<T>;

		template <typename Resources>
		auto resources_of(
		) const -> const Resources&;

		template <typename Resources>
		auto try_resources_of(
		) const -> const Resources*;

		template <typename State>
		auto after(
		) -> async::task<>;

		template <typename State>
		auto notify_ready(
		) -> void;
	};
}

template <typename T>
auto gse::task_context::get() const -> T& {
	return *static_cast<T*>(gpu_ctx);
}

template <typename T>
auto gse::task_context::try_get() const -> T* {
	return static_cast<T*>(gpu_ctx);
}

template <typename State>
auto gse::task_context::state_of() const -> const State& {
	const auto* p = live_state
		? states.state_ptr(id_of<State>())
		: states.state_snapshot_ptr(id_of<State>());
	assert(p != nullptr, std::source_location::current(), "state not found");
	return *static_cast<const State*>(p);
}

template <typename State>
auto gse::task_context::try_state_of() const -> const State* {
	const auto* p = live_state
		? states.state_ptr(id_of<State>())
		: states.state_snapshot_ptr(id_of<State>());
	return static_cast<const State*>(p);
}

template <typename T>
auto gse::task_context::read_channel() const -> channel_read_guard<T> {
	channels_store.ensure(id_of<T>(), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});
	const auto* ptr = channels_store.snapshot_data(id_of<T>());
	assert(ptr != nullptr, std::source_location::current(), "channel snapshot not found");
	return channel_read_guard<T>(*static_cast<const std::vector<T>*>(ptr));
}

template <typename Resources>
auto gse::task_context::resources_of() const -> const Resources& {
	const auto* p = resources_store.resources_ptr(id_of<Resources>());
	assert(p != nullptr, std::source_location::current(), "resources not found");
	return *static_cast<const Resources*>(p);
}

template <typename Resources>
auto gse::task_context::try_resources_of() const -> const Resources* {
	return static_cast<const Resources*>(resources_store.resources_ptr(id_of<Resources>()));
}

template <typename State>
auto gse::task_context::after() -> async::task<> {
	co_await graph.wait_state_ready(id_of<State>());
}

template <typename State>
auto gse::task_context::notify_ready() -> void {
	graph.notify_state_ready(id_of<State>());
}
