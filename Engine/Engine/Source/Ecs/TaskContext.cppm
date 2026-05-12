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

namespace gse {
	auto ensure_and_snapshot_channel(
		channel_registry& store,
		id type,
		channel_factory_fn factory
	) -> const void*;
}

template <typename T>
auto gse::task_context::read_channel() const -> channel_read_guard<T> {
	const auto* ptr = ensure_and_snapshot_channel(
		channels_store,
		id_of<T>(),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
	return channel_read_guard<T>(*static_cast<const std::vector<T>*>(ptr));
}

auto gse::ensure_and_snapshot_channel(channel_registry& store, const id type, const channel_factory_fn factory) -> const void* {
	store.ensure(type, factory);
	const auto* ptr = store.snapshot_data(type);
	assert(ptr != nullptr, "channel snapshot not found");
	return ptr;
}

auto gse::task_context::after_id(const id state_id) -> async::task<> {
	co_await graph.wait_state_ready(state_id);
}

auto gse::task_context::notify_ready_by_id(const id state_id) -> void {
	graph.notify_state_ready(state_id);
}
