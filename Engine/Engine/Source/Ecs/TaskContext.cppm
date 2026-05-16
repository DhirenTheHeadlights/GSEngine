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
		state_registry& states;
		resource_registry& resources_store;
		channel_registry& channels_store;
		channel_writer& channels;
		task_graph& graph;
		bool live_state = true;

		template <typename T>
		auto read_channel(
		) const -> channel_read_guard<T>;

		auto after_id(
			id state_id
		) -> async::task<>;

		auto notify_ready_by_id(
			id state_id
		) -> void;
	};
}

template <typename T>
auto gse::task_context::read_channel() const -> channel_read_guard<T> {
	return channel_read_guard<T>(channels_store.ensure_typed<T>().data.read_raw());
}

auto gse::task_context::after_id(const id state_id) -> async::task<> {
	co_await graph.wait_state_ready(state_id);
}

auto gse::task_context::notify_ready_by_id(const id state_id) -> void {
	graph.notify_state_ready(state_id);
}
