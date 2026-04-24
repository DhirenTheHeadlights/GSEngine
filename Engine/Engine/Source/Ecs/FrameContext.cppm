export module gse.ecs:frame_context;

import std;

import gse.core;
import gse.concurrency;

import :component;
import :phase_context;
import :registry;
import :task_context;

export namespace gse {
	class frame_context : public task_context {
	public:
		frame_context(
			void* gpu_ctx,
			const state_snapshot_provider& snapshots,
			channel_writer& channels,
			const channel_reader_provider& channel_reader,
			const resources_provider& resources,
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

	private:
		registry& m_reg;
	};
}

gse::frame_context::frame_context(
	void* gpu_ctx,
	const state_snapshot_provider& snapshots,
	channel_writer& channels,
	const channel_reader_provider& channel_reader,
	const resources_provider& resources,
	task_graph& graph,
	registry& reg
) : task_context{ gpu_ctx, snapshots, channels, channel_reader, resources, graph },
	m_reg(reg) {}

template <gse::is_component T>
auto gse::frame_context::try_component(const id owner) const -> const T* {
	return m_reg.try_component<T>(owner);
}

template <gse::is_component T>
auto gse::frame_context::components() const -> std::span<const T> {
	return m_reg.components<T>();
}
