export module gse.utility:frame_context;

import std;

import :async_task;
import :phase_context;
import :task_graph;
import :channel_base;
import :id;

export namespace gse {
	struct frame_context : phase_gpu_access {
		const state_snapshot_provider& snapshots;
		const channel_reader_provider& channel_reader;
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

		template <typename State>
		auto after(
		) -> async::task<>;

		template <typename State>
		auto notify_ready(
		) -> void;
	};
}

template <typename State>
auto gse::frame_context::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::frame_context::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename T>
auto gse::frame_context::read_channel() const -> channel_read_guard<T> {
	return channel_read_guard<T>(channel_reader.read<T>());
}

template <typename State>
auto gse::frame_context::after() -> async::task<> {
	return graph.wait_state_ready(std::type_index(typeid(State)));
}

template <typename State>
auto gse::frame_context::notify_ready() -> void {
	graph.notify_state_ready(std::type_index(typeid(State)));
}
