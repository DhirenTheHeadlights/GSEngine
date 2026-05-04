export module gse.ecs:frame_context;

import std;

import gse.core;
import gse.concurrency;

import :component;
import :phase_context;
import :registries;
import :registry;
import :task_context;

export namespace gse {
	class frame_context : public task_context {
	public:
		frame_context(
			void* gpu_ctx,
			void* assets_ptr,
			state_registry& states,
			resource_registry& resources_store,
			channel_registry& channels_store,
			channel_writer& channels,
			task_graph& graph,
			registry& reg
		);

		template <is_component T>
		auto try_component(
			id owner
		) const -> const T*;

		template <is_component T>
		auto components(
		) const -> std::span<const T>;

		template <typename State>
		[[nodiscard]] auto state_of(
		) const -> async::task<const State&>;

	private:
		registry& m_reg;
	};
}

gse::frame_context::frame_context(
	void* gpu_ctx,
	void* assets_ptr,
	state_registry& states,
	resource_registry& resources_store,
	channel_registry& channels_store,
	channel_writer& channels,
	task_graph& graph,
	registry& reg
) : task_context{ gpu_ctx, assets_ptr, states, resources_store, channels_store, channels, graph, false },
	m_reg(reg) {}

template <gse::is_component T>
auto gse::frame_context::try_component(const id owner) const -> const T* {
	return m_reg.try_component<T>(owner);
}

template <gse::is_component T>
auto gse::frame_context::components() const -> std::span<const T> {
	return m_reg.components<T>();
}

template <typename State>
auto gse::frame_context::state_of() const -> async::task<const State&> {
	static_assert(
		std::is_trivially_copyable_v<State>,
		"frame_context state reads require S to be trivially-copyable so a "
		"per-frame snapshot exists. Make S POD-trivial (move dynamic members "
		"into the system's resources / frame_data), or stop reading it from "
		"frame() — frame() runs concurrently with the next frame's update(), "
		"so a non-snapshot read is a data race."
	);
	return task_context::state_of<State>();
}
