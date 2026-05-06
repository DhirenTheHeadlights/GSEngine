export module gse.ecs:phase_context;

import std;

import gse.assert;
import gse.core;
import gse.containers;
import gse.concurrency;

import :component;
import :registries;
import :registry;

export namespace gse {
	class scheduler;

	struct init_context {
		registry& reg;
		scheduler& sched;
		state_registry& states;
		resource_registry& resources_store;
		channel_registry& channels_store;
		channel_writer& channels;

		template <typename T>
		auto read_channel(
		) const -> channel_read_guard<T>;
	};

	struct shutdown_context {
		registry& reg;
	};
}

template <typename T>
auto gse::init_context::read_channel() const -> channel_read_guard<T> {
	channels_store.ensure(id_of<T>(), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});
	const auto* ptr = channels_store.snapshot_data(id_of<T>());
	assert(ptr != nullptr, "channel snapshot not found");
	return channel_read_guard<T>(*static_cast<const std::vector<T>*>(ptr));
}
