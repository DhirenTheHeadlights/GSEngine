export module gse.utility:update_context;

import std;

import :async_task;
import :access_token;
import :registry;
import :phase_context;
import :task_graph;
import :lambda_traits;
import :mpsc_ring_buffer;
import :channel_base;
import :id;
import :concepts;

export namespace gse {
	struct scheduled_work {
		std::vector<std::type_index> reads;
		std::vector<std::type_index> writes;
		std::move_only_function<void()> execute;
	};

	struct update_context : phase_gpu_access {
		registry& reg;
		const state_snapshot_provider& snapshots;
		channel_writer& channels;
		const channel_reader_provider& channel_reader;
		const resources_provider& resources;
		task_graph& graph;
		std::vector<scheduled_work>& work;
		mpsc_ring_buffer<std::move_only_function<void()>, 64>& deferred_ops;

		template <is_component T>
		auto read(
		) -> gse::read<T>;

		template <is_component T>
		auto write(
		) -> gse::write<T>;

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

		template <typename F>
		auto schedule(
			F&& action,
			std::source_location loc = std::source_location::current()
		) -> void;

		template <is_component T, typename... Args>
		auto defer_add(
			id entity,
			Args&&... args
		) -> void;

		template <is_component T>
		auto defer_remove(
			id entity
		) -> void;

		auto defer_activate(
			id entity
		) -> void;
	};
}

namespace gse {
	template <typename AccessArg>
	auto acquire_access_token(
		registry& reg
	) -> AccessArg;

	template <typename ArgTuple, typename F, std::size_t... Is>
	auto invoke_with_access_tokens(
		registry& reg,
		F& func,
		std::index_sequence<Is...>
	) -> void;

}

template <typename AccessArg>
auto gse::acquire_access_token(registry& reg) -> AccessArg {
	using element_t = access_element_t<AccessArg>;
	if constexpr (is_read_access_v<AccessArg>) {
		return reg.acquire_read<element_t>();
	}
	else {
		return reg.acquire_write<element_t>();
	}
}

template <typename ArgTuple, typename F, std::size_t... Is>
auto gse::invoke_with_access_tokens(registry& reg, F& func, std::index_sequence<Is...>) -> void {
	func(acquire_access_token<std::tuple_element_t<Is, ArgTuple>>(reg)...);
}

template <gse::is_component T>
auto gse::update_context::read() -> gse::read<T> {
	return reg.acquire_read<T>();
}

template <gse::is_component T>
auto gse::update_context::write() -> gse::write<T> {
	return reg.acquire_write<T>();
}

template <typename State>
auto gse::update_context::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::update_context::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename T>
auto gse::update_context::read_channel() const -> channel_read_guard<T> {
	return channel_read_guard<T>(channel_reader.read<T>());
}

template <typename Resources>
auto gse::update_context::resources_of() const -> const Resources& {
	return resources.resources_of<Resources>();
}

template <typename Resources>
auto gse::update_context::try_resources_of() const -> const Resources* {
	return resources.try_resources_of<Resources>();
}

template <typename F>
auto gse::update_context::schedule(F&& action, std::source_location) -> void {
	using traits = lambda_traits<std::decay_t<F>>;
	using arg_tuple = typename traits::arg_tuple;

	if constexpr (traits::arity == 1 && std::is_same_v<std::tuple_element_t<0, arg_tuple>, registry&>) {
		work.push_back({
			.execute = [&r = reg, a = std::forward<F>(action)]() mutable { a(r); }
		});
	}
	else {
		work.push_back({
			.reads = traits::reads(),
			.writes = traits::writes(),
			.execute = [&r = reg, a = std::forward<F>(action)]() mutable {
				invoke_with_access_tokens<arg_tuple>(r, a, std::make_index_sequence<traits::arity>{});
			}
		});
	}
}

template <gse::is_component T, typename... Args>
auto gse::update_context::defer_add(const id entity, Args&&... args) -> void {
	deferred_ops.push(std::move_only_function<void()>([this, entity, ...args = std::forward<Args>(args)]() mutable {
		reg.ensure_exists(entity);
		if (!reg.active(entity)) {
			reg.ensure_active(entity);
		}
		reg.add_component<T>(entity, std::forward<Args>(args)...);
	}));
}

template <gse::is_component T>
auto gse::update_context::defer_remove(const id entity) -> void {
	deferred_ops.push(std::move_only_function<void()>([this, entity] {
		reg.remove_link<T>(entity);
	}));
}

auto gse::update_context::defer_activate(const id entity) -> void {
	deferred_ops.push(std::move_only_function<void()>([this, entity] {
		reg.ensure_active(entity);
	}));
}
