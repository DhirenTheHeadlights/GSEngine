export module gse.ecs:system_dispatch;

import std;
import gse.std_meta;

import gse.core;
import gse.concurrency;
import gse.diag;

import :phase_context;
import :update_context;
import :frame_context;
import :system_node;

namespace gse {
	template <typename S, bool = has_resources<S>>
	struct [[clang::nodebug]] resource_storage {};

	template <typename S>
	struct [[clang::nodebug]] resource_storage<S, true> {
		typename S::resources value;
	};

	template <typename S, bool = has_update_data<S>>
	struct [[clang::nodebug]] update_data_storage {};

	template <typename S>
	struct [[clang::nodebug]] update_data_storage<S, true> {
		typename S::update_data value;
	};

	template <typename S, bool = has_frame_data<S>>
	struct [[clang::nodebug]] frame_data_storage {};

	template <typename S>
	struct [[clang::nodebug]] frame_data_storage<S, true> {
		typename S::frame_data value;
	};

	template <typename State, bool = std::is_copy_assignable_v<State>>
	struct [[clang::nodebug]] snapshot_storage {};

	template <typename State>
	struct [[clang::nodebug]] snapshot_storage<State, true> {
		State value{};
	};

	template <typename S, typename State>
	struct [[clang::nodebug]] system_node_data {
		template <typename... Args>
		[[clang::nodebug]] explicit system_node_data(
			Args&&... args
		);

		[[no_unique_address]] resource_storage<S> resources;
		[[no_unique_address]] update_data_storage<S> update_data;
		[[no_unique_address]] frame_data_storage<S> frame_data;
		State state;
		[[no_unique_address]] snapshot_storage<State> snapshot;
	};

	template <typename U, typename S>
	concept matches_resources_v = has_resources<S> && std::is_same_v<U, typename S::resources>;

	template <typename U, typename S>
	concept matches_update_data_v = has_update_data<S> && std::is_same_v<U, typename S::update_data>;

	template <typename U, typename S>
	concept matches_frame_data_v = has_frame_data<S> && std::is_same_v<U, typename S::frame_data>;

	template <typename U, typename S, typename State>
	concept is_state_dep =
		!std::is_same_v<std::remove_cvref_t<U>, update_context> &&
		!std::is_same_v<std::remove_cvref_t<U>, frame_context> &&
		!std::is_same_v<std::remove_cvref_t<U>, State> &&
		!matches_resources_v<std::remove_cvref_t<U>, S> &&
		!matches_update_data_v<std::remove_cvref_t<U>, S> &&
		!matches_frame_data_v<std::remove_cvref_t<U>, S>;

	template <auto MemberFn>
	constexpr std::size_t arity_of = std::meta::parameters_of(MemberFn).size();

	template <auto MemberFn, std::size_t I>
	using arg_type_of = typename [: std::meta::type_of(std::meta::parameters_of(MemberFn)[I]) :];

	template <typename Arg, typename S, typename State>
	[[clang::nodebug]] auto resolve_initialize_arg(
		init_context& phase,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state
	) -> decltype(auto);

	template <typename Arg, typename S, typename State>
	[[clang::nodebug]] auto resolve_update_arg(
		update_context& ctx,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state
	) -> decltype(auto);

	template <typename Arg, typename S, typename State>
	[[clang::nodebug]] auto resolve_frame_arg(
		frame_context& ctx,
		resource_storage<S>& resources,
		frame_data_storage<S>& frame_data,
		const State& state
	) -> decltype(auto);

	template <typename S, typename State>
	[[clang::nodebug]] auto invoke_initialize_for(
		init_context& phase,
		void* data_ptr
	) -> void;

	template <typename S, typename State>
	[[clang::nodebug]] auto invoke_shutdown_for(
		shutdown_context& phase,
		void* data_ptr
	) -> void;

	template <typename S, typename State>
	[[clang::nodebug]] auto invoke_update_for(
		update_context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <typename S, typename State>
	[[clang::nodebug]] auto invoke_frame_for(
		frame_context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <typename S, typename State>
	[[clang::nodebug]] auto invoke_snapshot_for(
		void* data_ptr
	) -> void;

	template <typename S, typename State>
	[[clang::nodebug]] auto data_delete_for(
		void* data_ptr
	) -> void;
}

export namespace gse {
	[[clang::nodebug]] auto empty_task(
	) -> async::task<>;

	template <typename S, typename State>
	[[clang::nodebug]] auto extract_update_state_deps(
	) -> std::vector<id>;

	template <typename S, typename State>
	[[clang::nodebug]] auto extract_frame_state_deps(
	) -> std::vector<id>;

	template <typename S, typename State, typename... Args>
	[[clang::nodebug]] auto make_system_node(
		Args&&... args
	) -> system_node;
}

template <typename S, typename State>
template <typename... Args>
gse::system_node_data<S, State>::system_node_data(Args&&... args) : state(std::forward<Args>(args)...) {}

template <typename Arg, typename S, typename State>
auto gse::resolve_initialize_arg(init_context& phase, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, State& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, init_context>) {
		return (phase);
	}
	else if constexpr (matches_resources_v<U, S>) {
		return (resources.value);
	}
	else if constexpr (matches_update_data_v<U, S>) {
		return (update_data.value);
	}
	else if constexpr (matches_frame_data_v<U, S>) {
		return (frame_data.value);
	}
	else if constexpr (std::is_same_v<U, State>) {
		return (state);
	}
}

template <typename Arg, typename S, typename State>
auto gse::resolve_update_arg(update_context& ctx, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, State& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, update_context>) {
		return (ctx);
	}
	else if constexpr (matches_resources_v<U, S>) {
		return (resources.value);
	}
	else if constexpr (matches_update_data_v<U, S>) {
		return (update_data.value);
	}
	else if constexpr (matches_frame_data_v<U, S>) {
		return (frame_data.value);
	}
	else if constexpr (std::is_same_v<U, State>) {
		return (state);
	}
	else {
		return ctx.template state_of<U>();
	}
}

template <typename Arg, typename S, typename State>
auto gse::resolve_frame_arg(frame_context& ctx, resource_storage<S>& resources, frame_data_storage<S>& frame_data, const State& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, frame_context>) {
		return (ctx);
	}
	else if constexpr (matches_resources_v<U, S>) {
		return (resources.value);
	}
	else if constexpr (matches_frame_data_v<U, S>) {
		return (frame_data.value);
	}
	else if constexpr (std::is_same_v<U, State>) {
		return (state);
	}
	else {
		return ctx.template state_of<U>();
	}
}

auto gse::empty_task() -> async::task<> {
	co_return;
}

template <typename S, typename State>
auto gse::extract_update_state_deps() -> std::vector<id> {
	std::vector<id> result;
	if constexpr (names_update<S>) {
		template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(^^S::update))) {
			using P = typename [: std::meta::type_of(p) :];
			if constexpr (is_state_dep<P, S, State>) {
				result.push_back(id_of<std::remove_cvref_t<P>>());
			}
		}
	}
	return result;
}

template <typename S, typename State>
auto gse::extract_frame_state_deps() -> std::vector<id> {
	std::vector<id> result;
	if constexpr (names_frame<S>) {
		template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(^^S::frame))) {
			using P = typename [: std::meta::type_of(p) :];
			if constexpr (is_state_dep<P, S, State>) {
				result.push_back(id_of<std::remove_cvref_t<P>>());
			}
		}
	}
	return result;
}

template <typename S, typename State>
auto gse::invoke_initialize_for(init_context& phase, void* data_ptr) -> void {
	if constexpr (names_initialize<S>) {
		auto& d = *static_cast<system_node_data<S, State>*>(data_ptr);
		[&]<std::size_t... Is>(std::index_sequence<Is...>) {
			S::initialize(
				resolve_initialize_arg<arg_type_of<^^S::initialize, Is>, S, State>(
					phase, d.resources, d.update_data, d.frame_data, d.state
				)...
			);
		}(std::make_index_sequence<arity_of<^^S::initialize>>{});
	}
}

template <typename S, typename State>
auto gse::invoke_shutdown_for(shutdown_context& phase, void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S, State>*>(data_ptr);
	if constexpr (has_resources<S>) {
		if constexpr (requires { S::shutdown(phase, d.resources.value, d.state); }) {
			S::shutdown(phase, d.resources.value, d.state);
			return;
		}
	}
	if constexpr (requires { S::shutdown(phase, d.state); }) {
		S::shutdown(phase, d.state);
		return;
	}
	if constexpr (requires { S::shutdown(phase); }) {
		S::shutdown(phase);
		return;
	}
	if constexpr (names_shutdown<S>) {
		assert(
			false,
			std::source_location::current(),
			"System '{}' declares shutdown but no overload matched the dispatcher",
			type_tag<S>()
		);
	}
}

template <typename S, typename State>
auto gse::invoke_update_for(update_context& ctx, void* data_ptr) -> async::task<> {
	if constexpr (names_update<S>) {
		auto& d = *static_cast<system_node_data<S, State>*>(data_ptr);
		return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
			return S::update(
				resolve_update_arg<arg_type_of<^^S::update, Is>, S, State>(
					ctx, d.resources, d.update_data, d.frame_data, d.state
				)...
			);
		}(std::make_index_sequence<arity_of<^^S::update>>{});
	}
	else {
		return empty_task();
	}
}

template <typename S, typename State>
auto gse::invoke_frame_for(frame_context& ctx, void* data_ptr) -> async::task<> {
	if constexpr (names_frame<S>) {
		auto& d = *static_cast<system_node_data<S, State>*>(data_ptr);
		if constexpr (std::is_trivially_copyable_v<State>) {
			const State& state_ref = d.snapshot.value;
			return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
				return S::frame(
					resolve_frame_arg<arg_type_of<^^S::frame, Is>, S, State>(
						ctx, d.resources, d.frame_data, state_ref
					)...
				);
			}(std::make_index_sequence<arity_of<^^S::frame>>{});
		}
		else {
			const State& state_ref = d.state;
			return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
				return S::frame(
					resolve_frame_arg<arg_type_of<^^S::frame, Is>, S, State>(
						ctx, d.resources, d.frame_data, state_ref
					)...
				);
			}(std::make_index_sequence<arity_of<^^S::frame>>{});
		}
	}
	else {
		return empty_task();
	}
}

template <typename S, typename State>
auto gse::invoke_snapshot_for(void* data_ptr) -> void {
	if constexpr (std::is_copy_assignable_v<State>) {
		auto& d = *static_cast<system_node_data<S, State>*>(data_ptr);
		d.snapshot.value = d.state;
	}
}

template <typename S, typename State>
auto gse::data_delete_for(void* data_ptr) -> void {
	delete static_cast<system_node_data<S, State>*>(data_ptr);
}

template <typename S, typename State, typename... Args>
auto gse::make_system_node(Args&&... args) -> system_node {
	auto* d = new system_node_data<S, State>(std::forward<Args>(args)...);

	system_node node;
	node.data = std::unique_ptr<void, void(*)(void*)>(d, &data_delete_for<S, State>);

	node.invoke_initialize_fn = &invoke_initialize_for<S, State>;
	node.invoke_shutdown_fn = &invoke_shutdown_for<S, State>;
	node.invoke_update_fn = &invoke_update_for<S, State>;
	node.invoke_frame_fn = &invoke_frame_for<S, State>;
	node.invoke_snapshot_fn = &invoke_snapshot_for<S, State>;

	node.update_state_deps = extract_update_state_deps<S, State>();
	node.frame_state_deps = extract_frame_state_deps<S, State>();

	node.state_ptr = &d->state;
	if constexpr (std::is_copy_assignable_v<State>) {
		node.state_snapshot_ptr = &d->snapshot.value;
	}
	if constexpr (has_resources<S>) {
		node.resources_ptr = &d->resources.value;
	}
	node.has_frame = names_frame<S>;
	node.state_id = id_of<State>();
	node.frame_wall_id = find_or_generate_id(std::format("frame_wall:{}", type_tag<S>()));
	node.trace_id = gse::trace_id<S>();

	return node;
}
