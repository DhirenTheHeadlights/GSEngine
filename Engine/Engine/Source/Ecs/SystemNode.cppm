module;

#include <meta>

export module gse.ecs:system_node;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;

import :phase_context;
import :task_context;
import :update_context;
import :frame_context;

export namespace gse {
	class system_node_base {
	public:
		virtual ~system_node_base(
		) = default;

		virtual auto initialize(
			init_context&
		) -> void = 0;

		virtual auto shutdown(
			shutdown_context&
		) -> void = 0;

		virtual auto graph_update(
			update_context&
		) -> async::task<> = 0;

		virtual auto graph_frame(
			frame_context&
		) -> async::task<> = 0;

		virtual auto has_frame(
		) const -> bool = 0;

		virtual auto snapshot_state(
		) -> void = 0;

		virtual auto state_ptr(
		) -> void* = 0;

		virtual auto state_ptr(
		) const -> const void* = 0;

		virtual auto state_snapshot_ptr(
		) const -> const void* = 0;

		virtual auto resources_ptr(
		) -> void* = 0;

		virtual auto resources_ptr(
		) const -> const void* = 0;

		virtual auto state_type(
		) const -> std::type_index = 0;

		virtual auto resources_type(
		) const -> std::type_index = 0;

		virtual auto trace_id(
		) const -> id = 0;
	};

	template <typename S>
	concept has_resources = requires { typename S::resources; };

	template <typename S>
	concept has_update_data = requires { typename S::update_data; };

	template <typename S>
	concept has_frame_data = requires { typename S::frame_data; };

	template <typename S>
	concept names_initialize = requires { &S::initialize; };

	template <typename S>
	concept names_update = requires { &S::update; };

	template <typename S>
	concept names_shutdown = requires { &S::shutdown; };

	template <typename S>
	concept names_frame = requires { &S::frame; };

	template <typename S, typename State>
	auto extract_state_deps(
	) -> std::vector<std::type_index>;

	template <typename S, bool = has_resources<S>>
	struct resource_storage {};

	template <typename S>
	struct resource_storage<S, true> {
		typename S::resources value;
	};

	template <typename S, bool = has_update_data<S>>
	struct update_data_storage {};

	template <typename S>
	struct update_data_storage<S, true> {
		typename S::update_data value;
	};

	template <typename S, bool = has_frame_data<S>>
	struct frame_data_storage {};

	template <typename S>
	struct frame_data_storage<S, true> {
		typename S::frame_data value;
	};

	template <typename State, bool = std::is_copy_assignable_v<State>>
	struct snapshot_storage {};

	template <typename State>
	struct snapshot_storage<State, true> {
		State value{};
	};

	template <typename S, typename State>
	class system_node final : public system_node_base, non_copyable {
	public:
		~system_node(
		) override = default;

		template <typename... Args>
		explicit system_node(
			Args&&... args
		);

		auto initialize(
			init_context& phase
		) -> void override;

		auto shutdown(
			shutdown_context& phase
		) -> void override;

		auto graph_update(
			update_context& ctx
		) -> async::task<> override;

		auto graph_frame(
			frame_context& ctx
		) -> async::task<> override;

		auto has_frame(
		) const -> bool override;

		auto snapshot_state(
		) -> void override;

		auto state_ptr(
		) -> void* override;

		auto state_ptr(
		) const -> const void* override;

		auto state_snapshot_ptr(
		) const -> const void* override;

		auto resources_ptr(
		) -> void* override;

		auto resources_ptr(
		) const -> const void* override;

		auto state_type(
		) const -> std::type_index override;

		auto resources_type(
		) const -> std::type_index override;

		auto state(
			this auto& self
		) -> auto&;

		auto trace_id(
		) const -> id override;

	private:
		[[no_unique_address]] resource_storage<S> m_resources;
		[[no_unique_address]] update_data_storage<S> m_update_data;
		[[no_unique_address]] frame_data_storage<S> m_frame_data;
		State m_state;
		[[no_unique_address]] snapshot_storage<State> m_snapshot;
	};
}

namespace gse {
	template <typename S, typename State>
	auto dispatch_initialize(
		init_context& phase,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state
	) -> void;

	template <typename S, typename State>
	auto dispatch_update(
		update_context& ctx,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state
	) -> async::task<>;

	template <typename S, typename State>
	auto dispatch_frame(
		frame_context& ctx,
		resource_storage<S>& resources,
		frame_data_storage<S>& frame_data,
		const State& state
	) -> async::task<>;

	template <typename S, typename State>
	auto dispatch_has_frame(
	) -> bool;
}

namespace gse {
	template <typename U, typename S, bool = has_resources<S>>
	struct matches_resources_t : std::false_type {};

	template <typename U, typename S>
	struct matches_resources_t<U, S, true> : std::bool_constant<std::is_same_v<U, typename S::resources>> {};

	template <typename U, typename S>
	constexpr bool matches_resources_v = matches_resources_t<U, S>::value;

	template <typename U, typename S, bool = has_update_data<S>>
	struct matches_update_data_t : std::false_type {};

	template <typename U, typename S>
	struct matches_update_data_t<U, S, true> : std::bool_constant<std::is_same_v<U, typename S::update_data>> {};

	template <typename U, typename S>
	constexpr bool matches_update_data_v = matches_update_data_t<U, S>::value;

	template <typename U, typename S, bool = has_frame_data<S>>
	struct matches_frame_data_t : std::false_type {};

	template <typename U, typename S>
	struct matches_frame_data_t<U, S, true> : std::bool_constant<std::is_same_v<U, typename S::frame_data>> {};

	template <typename U, typename S>
	constexpr bool matches_frame_data_v = matches_frame_data_t<U, S>::value;

	template <typename Arg, typename S, typename State>
	constexpr bool is_state_dep_v = [] consteval {
		using U = std::remove_cvref_t<Arg>;
		if constexpr (std::is_same_v<U, update_context>) return false;
		else if constexpr (std::is_same_v<U, frame_context>) return false;
		else if constexpr (std::is_same_v<U, State>) return false;
		else if constexpr (matches_resources_v<U, S>) return false;
		else if constexpr (matches_update_data_v<U, S>) return false;
		else if constexpr (matches_frame_data_v<U, S>) return false;
		else return true;
	}();

	template <auto MemberFn>
	constexpr std::size_t arity_of = std::meta::parameters_of(MemberFn).size();

	template <auto MemberFn, std::size_t I>
	using arg_type_of = typename [: std::meta::type_of(std::meta::parameters_of(MemberFn)[I]) :];
}

template <typename S, typename State>
auto gse::extract_state_deps() -> std::vector<std::type_index> {
	std::vector<std::type_index> result;
	if constexpr (names_update<S>) {
		template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(^^S::update))) {
			using P = typename [: std::meta::type_of(p) :];
			if constexpr (is_state_dep_v<P, S, State>) {
				result.push_back(std::type_index(typeid(std::remove_cvref_t<P>)));
			}
		}
	}
	if constexpr (names_frame<S>) {
		template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(^^S::frame))) {
			using P = typename [: std::meta::type_of(p) :];
			if constexpr (is_state_dep_v<P, S, State>) {
				result.push_back(std::type_index(typeid(std::remove_cvref_t<P>)));
			}
		}
	}
	return result;
}

namespace gse {
	template <typename Arg, typename S, typename State>
	auto resolve_initialize_arg(
		init_context& phase,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state
	) -> decltype(auto) {
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

	template <typename S, typename State, std::size_t... Is>
	auto invoke_initialize_impl(
		init_context& phase,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state,
		std::index_sequence<Is...>
	) -> void {
		S::initialize(
			resolve_initialize_arg<arg_type_of<^^S::initialize, Is>, S, State>(
				phase, resources, update_data, frame_data, state
			)...
		);
	}
}

template <typename S, typename State>
auto gse::dispatch_initialize(init_context& phase, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, State& state) -> void {
	if constexpr (names_initialize<S>) {
		invoke_initialize_impl<S, State>(
			phase, resources, update_data, frame_data, state,
			std::make_index_sequence<arity_of<^^S::initialize>>{}
		);
	}
}

namespace gse {
	template <typename Arg, typename S, typename State>
	auto resolve_update_arg(
		update_context& ctx,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state
	) -> decltype(auto) {
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
	auto resolve_frame_arg(
		frame_context& ctx,
		resource_storage<S>& resources,
		frame_data_storage<S>& frame_data,
		const State& state
	) -> decltype(auto) {
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

	template <typename S, typename State, std::size_t I = 0>
	auto await_update_deps(update_context& ctx) -> async::task<> {
		if constexpr (I < arity_of<^^S::update>) {
			using Arg = arg_type_of<^^S::update, I>;
			if constexpr (is_state_dep_v<Arg, S, State>) {
				co_await ctx.template after<std::remove_cvref_t<Arg>>();
			}
			co_await await_update_deps<S, State, I + 1>(ctx);
		}
	}

	template <typename S, typename State, std::size_t I = 0>
	auto await_frame_deps(frame_context& ctx) -> async::task<> {
		if constexpr (I < arity_of<^^S::frame>) {
			using Arg = arg_type_of<^^S::frame, I>;
			if constexpr (is_state_dep_v<Arg, S, State>) {
				co_await ctx.template after<std::remove_cvref_t<Arg>>();
			}
			co_await await_frame_deps<S, State, I + 1>(ctx);
		}
	}

	template <typename S, typename State, std::size_t... Is>
	auto invoke_update_impl(
		update_context& ctx,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		State& state,
		std::index_sequence<Is...>
	) -> async::task<> {
		co_await S::update(
			resolve_update_arg<arg_type_of<^^S::update, Is>, S, State>(
				ctx, resources, update_data, frame_data, state
			)...
		);
	}

	template <typename S, typename State, std::size_t... Is>
	auto invoke_frame_impl(
		frame_context& ctx,
		resource_storage<S>& resources,
		frame_data_storage<S>& frame_data,
		const State& state,
		std::index_sequence<Is...>
	) -> async::task<> {
		co_await S::frame(
			resolve_frame_arg<arg_type_of<^^S::frame, Is>, S, State>(
				ctx, resources, frame_data, state
			)...
		);
	}
}

template <typename S, typename State>
auto gse::dispatch_update(update_context& ctx, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, State& state) -> async::task<> {
	if constexpr (names_update<S>) {
		co_await await_update_deps<S, State>(ctx);
		co_await invoke_update_impl<S, State>(
			ctx, resources, update_data, frame_data, state,
			std::make_index_sequence<arity_of<^^S::update>>{}
		);
	}
	co_return;
}

template <typename S, typename State>
auto gse::dispatch_frame(frame_context& ctx, resource_storage<S>& resources, frame_data_storage<S>& frame_data, const State& state) -> async::task<> {
	if constexpr (names_frame<S>) {
		co_await await_frame_deps<S, State>(ctx);
		co_await invoke_frame_impl<S, State>(
			ctx, resources, frame_data, state,
			std::make_index_sequence<arity_of<^^S::frame>>{}
		);
	}
	co_return;
}

template <typename S, typename State>
auto gse::dispatch_has_frame() -> bool {
	return names_frame<S>;
}

template <typename S, typename State>
template <typename... Args>
gse::system_node<S, State>::system_node(Args&&... args) : m_state(std::forward<Args>(args)...) {}

template <typename S, typename State>
auto gse::system_node<S, State>::initialize(init_context& phase) -> void {
	dispatch_initialize<S>(phase, m_resources, m_update_data, m_frame_data, m_state);
}

template <typename S, typename State>
auto gse::system_node<S, State>::trace_id() const -> id {
	static const auto cached = [] {
		std::string_view raw = typeid(S).name();
		for (std::string_view prefix : {"struct ", "class "}) {
			if (raw.starts_with(prefix)) {
				raw.remove_prefix(prefix.size());
				break;
			}
		}
		const auto last = raw.rfind("::");
		if (last != std::string_view::npos && last > 0) {
			const auto prev = raw.rfind("::", last - 1);
			raw = (prev != std::string_view::npos) ? raw.substr(prev + 2) : raw.substr(last + 2);
		}
		return find_or_generate_id(raw);
	}();
	return cached;
}

template <typename S, typename State>
auto gse::system_node<S, State>::shutdown(shutdown_context& phase) -> void {
	if constexpr (has_resources<S>) {
		if constexpr (requires { S::shutdown(phase, m_resources.value, m_state); }) {
			S::shutdown(phase, m_resources.value, m_state);
			return;
		}
	}
	if constexpr (requires { S::shutdown(phase, m_state); }) {
		S::shutdown(phase, m_state);
		return;
	}
	if constexpr (requires { S::shutdown(phase); }) {
		S::shutdown(phase);
		return;
	}
	if constexpr (names_shutdown<S>) {
		assert(false, std::source_location::current(), "System '{}' declares shutdown but no overload matched the dispatcher", typeid(S).name());
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::graph_update(update_context& ctx) -> async::task<> {
	co_await dispatch_update<S>(ctx, m_resources, m_update_data, m_frame_data, m_state);
	ctx.notify_ready<State>();
	co_return;
}

template <typename S, typename State>
auto gse::system_node<S, State>::graph_frame(frame_context& ctx) -> async::task<> {
	static const auto wall_id = [] {
		std::string_view raw = typeid(S).name();
		for (std::string_view prefix : {"struct ", "class "}) {
			if (raw.starts_with(prefix)) {
				raw.remove_prefix(prefix.size());
				break;
			}
		}
		const auto last = raw.rfind("::");
		if (last != std::string_view::npos && last > 0) {
			const auto prev = raw.rfind("::", last - 1);
			raw = (prev != std::string_view::npos) ? raw.substr(prev + 2) : raw.substr(last + 2);
		}
		return find_or_generate_id(std::format("frame_wall:{}", raw));
	}();

	const auto eid = trace::begin_block(wall_id, 0);
	auto guard = make_scope_exit([eid] { trace::end_block(wall_id, eid, 0); });

	if constexpr (std::is_trivially_copyable_v<State>) {
		co_await dispatch_frame<S>(ctx, m_resources, m_frame_data, m_snapshot.value);
	} else {
		co_await dispatch_frame<S>(ctx, m_resources, m_frame_data, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::has_frame() const -> bool {
	return dispatch_has_frame<S, State>();
}

template <typename S, typename State>
auto gse::system_node<S, State>::snapshot_state() -> void {
	if constexpr (std::is_copy_assignable_v<State>) {
		m_snapshot.value = m_state;
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_snapshot_ptr() const -> const void* {
	if constexpr (std::is_copy_assignable_v<State>) {
		return &m_snapshot.value;
	} else {
		assert(false, std::source_location::current(),
			"Attempted to read snapshot of non-copyable state '{}'. "
			"Either make the state copyable or avoid try_state_of<T>() for this type.",
			typeid(State).name());
		return nullptr;
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_ptr() -> void* {
	return &m_state;
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_ptr() const -> const void* {
	return &m_state;
}

template <typename S, typename State>
auto gse::system_node<S, State>::resources_ptr() -> void* {
	if constexpr (has_resources<S>) {
		return &m_resources.value;
	}
	return nullptr;
}

template <typename S, typename State>
auto gse::system_node<S, State>::resources_ptr() const -> const void* {
	if constexpr (has_resources<S>) {
		return &m_resources.value;
	}
	return nullptr;
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_type() const -> std::type_index {
	return std::type_index(typeid(State));
}

template <typename S, typename State>
auto gse::system_node<S, State>::resources_type() const -> std::type_index {
	if constexpr (has_resources<S>) {
		return std::type_index(typeid(typename S::resources));
	}
	return std::type_index(typeid(void));
}

template <typename S, typename State>
auto gse::system_node<S, State>::state(this auto& self) -> auto& {
	return self.m_state;
}
