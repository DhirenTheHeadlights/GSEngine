export module gse.ecs:task_context;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;

import :phase_context;

export namespace gse {
	struct task_context : phase_gpu_access {
		scheduler& sched;
		channel_writer& channels;
		task_graph& graph;
		bool live_state = true;

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
