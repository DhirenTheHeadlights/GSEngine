export module gse.ecs:task_context;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;

import :phase_context;

export namespace gse {
	struct task_context : phase_gpu_access {
		const state_snapshot_provider& snapshots;
		channel_writer& channels;
		const channel_reader_provider& channel_reader;
		const resources_provider& resources;
		task_graph& graph;

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

template <typename State>
auto gse::task_context::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::task_context::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename T>
auto gse::task_context::read_channel() const -> channel_read_guard<T> {
	return channel_read_guard<T>(channel_reader.read<T>());
}

template <typename Resources>
auto gse::task_context::resources_of() const -> const Resources& {
	return resources.resources_of<Resources>();
}

template <typename Resources>
auto gse::task_context::try_resources_of() const -> const Resources* {
	return resources.try_resources_of<Resources>();
}

template <typename State>
auto gse::task_context::after() -> async::task<> {
	static const id tid = find_or_generate_id(std::format("after<{}>", typeid(State).name()));
	const auto key = trace::allocate_async_key();
	trace::begin_async(tid, key);
	co_await graph.wait_state_ready(std::type_index(typeid(State)));
	trace::end_async(tid, key);
}

template <typename State>
auto gse::task_context::notify_ready() -> void {
	graph.notify_state_ready(std::type_index(typeid(State)));
}
