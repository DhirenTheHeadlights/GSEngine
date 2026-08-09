export module gse.ecs:task_context;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.diag;

import :registries;
import :task_graph;
import :traits;

export namespace gse {
	class context;
	class scheduler;

	class task_context {
	public:
		task_context(
			state_registry& states,
			resource_registry& resources_store,
			channel_registry& channels_store,
			channel_writer& channels,
			task_graph& graph,
			bool live_state = true
		);

	protected:
		channel_writer& channels;

		state_registry& states;
		resource_registry& resources_store;
		channel_registry& channels_store;
		task_graph& graph;
		bool live_state = true;

		auto after_id(
			id state_id
		) -> async::task<>;

		auto notify_ready_by_id(
			id state_id
		) -> void;

		template <typename Arg, typename S>
		friend auto resolve_run_arg(
			context& ctx,
			state_of_t<S>& state
		) -> decltype(auto);

		template <typename T>
		friend auto external_resource_ref(
			const task_context& ctx
		) -> const T&;

		friend class scheduler;
	};
}

gse::task_context::task_context(state_registry& states, resource_registry& resources_store, channel_registry& channels_store, channel_writer& channels, task_graph& graph, const bool live_state)
	: channels(channels), states(states), resources_store(resources_store), channels_store(channels_store), graph(graph), live_state(live_state) {
}

auto gse::task_context::after_id(const id state_id) -> async::task<> {
	co_await graph.wait_state_ready(state_id);
}

auto gse::task_context::notify_ready_by_id(const id state_id) -> void {
	graph.notify_state_ready(state_id);
}
