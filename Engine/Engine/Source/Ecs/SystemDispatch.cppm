export module gse.ecs:system_dispatch;

import std;

import gse.std_meta;
import gse.core;
import gse.concurrency;
import gse.diag;
import gse.settings;

import :phase_context;
import :registries;
import :update_context;
import :frame_context;
import :system_node;

namespace gse {
	template <typename S, bool = names_state<S>>
	struct state_of_helper {
		using type = S;
	};

	template <typename S>
	struct state_of_helper<S, true> {
		using type = typename S::state;
	};

	template <typename S>
	using state_of_t = typename state_of_helper<S>::type;

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

	template <typename S, bool = has_settings<S>>
	struct settings_storage {};

	template <typename S>
	struct settings_storage<S, true> {
		typename S::settings value;
	};

	template <typename S, bool = has_settings<S>>
	struct settings_is_trivial : std::false_type {};

	template <typename S>
	struct settings_is_trivial<S, true> : std::bool_constant<std::is_trivially_copyable_v<typename S::settings>> {};

	template <typename S>
	constexpr bool settings_is_trivial_v = settings_is_trivial<S>::value;

	template <typename State, bool = std::is_trivially_copyable_v<State>>
	struct snapshot_storage {};

	template <typename State>
	struct snapshot_storage<State, true> {
		State value{};
	};

	template <typename S, bool = has_settings<S>>
	struct settings_snapshot_storage {};

	template <typename S>
	struct settings_snapshot_storage<S, true> {
		[[no_unique_address]] settings_storage<S> value;
	};

	template <typename S>
	struct system_node_data {
		template <typename... Args>
		explicit system_node_data(
			Args&&... args
		);

		[[no_unique_address]] resource_storage<S> resources;
		[[no_unique_address]] update_data_storage<S> update_data;
		[[no_unique_address]] frame_data_storage<S> frame_data;
		[[no_unique_address]] settings_storage<S> settings;
		state_of_t<S> state;
		[[no_unique_address]] snapshot_storage<state_of_t<S>> snapshot;
		[[no_unique_address]] settings_snapshot_storage<S> settings_snapshot;
	};

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

	template <typename U, typename S, bool = has_settings<S>>
	struct matches_settings_t : std::false_type {};

	template <typename U, typename S>
	struct matches_settings_t<U, S, true> : std::bool_constant<std::is_same_v<U, typename S::settings>> {};

	template <typename U, typename S>
	constexpr bool matches_settings_v = matches_settings_t<U, S>::value;

	template <typename T>
	using dep_pointee_t = std::remove_cv_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

	template <typename Arg, typename S>
	constexpr bool is_state_dep_v = [] consteval {
		using U = dep_pointee_t<Arg>;
		if constexpr (std::is_same_v<U, init_context>) {
			return false;
		}
		else if constexpr (std::is_same_v<U, update_context>) {
			return false;
		}
		else if constexpr (std::is_same_v<U, frame_context>) {
			return false;
		}
		else if constexpr (std::is_same_v<U, state_of_t<S>>) {
			return false;
		}
		else if constexpr (matches_resources_v<U, S>) {
			return false;
		}
		else if constexpr (matches_update_data_v<U, S>) {
			return false;
		}
		else if constexpr (matches_frame_data_v<U, S>) {
			return false;
		}
		else if constexpr (matches_settings_v<U, S>) {
			return false;
		}
		else {
			return true;
		}
	}();

	template <auto MemberFn>
	constexpr std::size_t arity_of = std::meta::parameters_of(MemberFn).size();

	template <auto MemberFn, std::size_t I>
	using arg_type_of = typename [: std::meta::type_of(std::meta::parameters_of(MemberFn)[I]) :];

	template <typename T>
	auto direct_state_ref(
		const task_context& ctx
	) -> const T&;

	template <typename T>
	auto direct_resources_ref(
		const task_context& ctx
	) -> const T&;

	template <typename Arg, typename S>
	auto resolve_initialize_arg(
		init_context& phase,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		settings_storage<S>& settings,
		state_of_t<S>& state
	) -> decltype(auto);

	template <typename Arg, typename S>
	auto resolve_update_arg(
		update_context& ctx,
		resource_storage<S>& resources,
		update_data_storage<S>& update_data,
		frame_data_storage<S>& frame_data,
		settings_storage<S>& settings,
		state_of_t<S>& state
	) -> decltype(auto);

	template <typename Arg, typename S>
	auto resolve_frame_arg(
		frame_context& ctx,
		resource_storage<S>& resources,
		frame_data_storage<S>& frame_data,
		const settings_storage<S>& settings,
		const state_of_t<S>& state
	) -> decltype(auto);

	template <typename S>
	auto invoke_initialize_for(
		init_context& phase,
		void* data_ptr
	) -> void;

	template <typename S>
	auto invoke_shutdown_for(
		shutdown_context& phase,
		void* data_ptr
	) -> void;

	template <typename S>
	auto invoke_update_for(
		update_context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <typename S>
	auto invoke_frame_for(
		frame_context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <typename S>
	auto invoke_snapshot_for(
		void* data_ptr
	) -> void;

	template <typename S>
	auto invoke_apply_settings_for(
		void* data_ptr,
		channel_registry& channels_store,
		channel_writer& channels
	) -> void;

	template <typename S>
	auto data_delete_for(
		void* data_ptr
	) -> void;

	auto noop_initialize(
		init_context& phase,
		void* data_ptr
	) -> void;

	auto noop_shutdown(
		shutdown_context& phase,
		void* data_ptr
	) -> void;

	struct noop_dispatchers {
		template <typename S>
		static auto noop_update_for(
			update_context& ctx,
			void* data_ptr
		) -> async::task<>;

		template <typename S>
		static auto noop_frame_for(
			frame_context& ctx,
			void* data_ptr
		) -> async::task<>;
	};

	auto noop_snapshot(
		void* data_ptr
	) -> void;

	template <auto MemberFn, typename S>
	auto register_state_dep_tags(
	) -> void;

	template <typename S>
	auto extract_init_state_deps(
	) -> std::vector<id>;

	template <typename S>
	auto extract_update_state_deps(
	) -> std::vector<id>;

	template <typename S>
	auto extract_frame_state_deps(
	) -> std::vector<id>;

	template <typename T>
	consteval auto compute_state_dep_id(
	) -> id;

	template <typename T>
	constexpr id state_dep_id_v = compute_state_dep_id<dep_pointee_t<T>>();

	template <auto MemberFn, typename S>
	consteval auto compute_state_dep_count(
	) -> std::size_t;

	template <auto MemberFn, typename S>
	consteval auto compute_state_dep_ids(
	) -> std::array<id, compute_state_dep_count<MemberFn, S>()>;

	template <typename S>
	concept shutdown_takes_resources_state = has_resources<S> && requires(shutdown_context& p, typename S::resources& r, state_of_t<S>& s) {
		S::shutdown(p, r, s);
	};

	template <typename S>
	concept shutdown_takes_state = requires(shutdown_context& p, state_of_t<S>& s) {
		S::shutdown(p, s);
	};

	template <typename S>
	concept shutdown_takes_phase_only = requires(shutdown_context& p) {
		S::shutdown(p);
	};
}

template <typename S>
template <typename... Args>
gse::system_node_data<S>::system_node_data(Args&&... args) : state(std::forward<Args>(args)...) {}

template <typename Arg, typename S>
auto gse::resolve_initialize_arg(init_context& phase, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, settings_storage<S>& settings, state_of_t<S>& state) -> decltype(auto) {
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
	else if constexpr (matches_settings_v<U, S>) {
		return (settings.value);
	}
	else if constexpr (std::is_same_v<U, state_of_t<S>>) {
		return (state);
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		static_assert(
			std::is_const_v<std::remove_pointer_t<U>>,
			"cross-system state must be const; use channels for mutation"
		);
		constexpr id state_lookup_id = compute_state_dep_id<Pointee>();
		if (const auto* p = phase.states.state_ptr(state_lookup_id)) {
			return static_cast<const Pointee*>(p);
		}
		return static_cast<const Pointee*>(phase.resources_store.resources_ptr(id_of<Pointee>()));
	}
	else {
		static_assert(
			std::is_const_v<std::remove_reference_t<Arg>>,
			"cross-system state must be const; use channels for mutation"
		);
		constexpr id state_lookup_id = compute_state_dep_id<U>();
		if (const auto* p = phase.states.state_ptr(state_lookup_id)) {
			return static_cast<const U&>(*static_cast<const U*>(p));
		}
		const auto* p = phase.resources_store.resources_ptr(id_of<U>());
		assert(p != nullptr, "cross-system state or resources not found");
		return static_cast<const U&>(*static_cast<const U*>(p));
	}
}

template <typename T>
auto gse::direct_state_ref(const task_context& ctx) -> const T& {
	constexpr id state_lookup_id = compute_state_dep_id<T>();
	const void* p = nullptr;
	if (ctx.live_state) {
		p = ctx.states.state_ptr(state_lookup_id);
	}
	else {
		p = ctx.states.state_snapshot_ptr(state_lookup_id);
		if (!p) {
			p = ctx.states.state_ptr(state_lookup_id);
		}
	}
	if (!p) {
		p = ctx.resources_store.resources_ptr(id_of<T>());
	}
	assert(p != nullptr, "cross-system state or resources not found");
	return *static_cast<const T*>(p);
}

template <typename T>
auto gse::direct_resources_ref(const task_context& ctx) -> const T& {
	const auto* p = ctx.resources_store.resources_ptr(id_of<T>());
	assert(p != nullptr, "resources not found");
	return *static_cast<const T*>(p);
}

template <typename Arg, typename S>
auto gse::resolve_update_arg(update_context& ctx, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, settings_storage<S>& settings, state_of_t<S>& state) -> decltype(auto) {
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
	else if constexpr (matches_settings_v<U, S>) {
		return (settings.value);
	}
	else if constexpr (std::is_same_v<U, state_of_t<S>>) {
		return (state);
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		static_assert(
			std::is_const_v<std::remove_pointer_t<U>>,
			"cross-system state must be const; use channels for mutation"
		);
		constexpr id state_lookup_id = compute_state_dep_id<Pointee>();
		const void* p = ctx.live_state ? ctx.states.state_ptr(state_lookup_id)
		                                : (ctx.states.state_snapshot_ptr(state_lookup_id)
		                                    ? ctx.states.state_snapshot_ptr(state_lookup_id)
		                                    : ctx.states.state_ptr(state_lookup_id));
		if (!p) {
			p = ctx.resources_store.resources_ptr(id_of<Pointee>());
		}
		return static_cast<const Pointee*>(p);
	}
	else {
		static_assert(
			std::is_const_v<std::remove_reference_t<Arg>>,
			"cross-system state must be const; use channels for mutation"
		);
		return direct_state_ref<U>(ctx);
	}
}

template <typename Arg, typename S>
auto gse::resolve_frame_arg(frame_context& ctx, resource_storage<S>& resources, frame_data_storage<S>& frame_data, const settings_storage<S>& settings, const state_of_t<S>& state) -> decltype(auto) {
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
	else if constexpr (matches_settings_v<U, S>) {
		return (settings.value);
	}
	else if constexpr (std::is_same_v<U, state_of_t<S>>) {
		return (state);
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		static_assert(
			std::is_const_v<std::remove_pointer_t<U>>,
			"cross-system state must be const; use channels for mutation"
		);
		constexpr id state_lookup_id = compute_state_dep_id<Pointee>();
		const void* p = ctx.live_state ? ctx.states.state_ptr(state_lookup_id)
		                                : (ctx.states.state_snapshot_ptr(state_lookup_id)
		                                    ? ctx.states.state_snapshot_ptr(state_lookup_id)
		                                    : ctx.states.state_ptr(state_lookup_id));
		if (!p) {
			p = ctx.resources_store.resources_ptr(id_of<Pointee>());
		}
		return static_cast<const Pointee*>(p);
	}
	else {
		static_assert(
			std::is_const_v<std::remove_reference_t<Arg>>,
			"cross-system state must be const; use channels for mutation"
		);
		return direct_state_ref<U>(ctx);
	}
}

auto gse::noop_initialize(init_context&, void*) -> void {}

auto gse::noop_shutdown(shutdown_context&, void*) -> void {}

template <typename S>
auto gse::noop_dispatchers::noop_update_for(update_context&, void*) -> async::task<> {
	co_return;
}

template <typename S>
auto gse::noop_dispatchers::noop_frame_for(frame_context&, void*) -> async::task<> {
	co_return;
}

auto gse::noop_snapshot(void*) -> void {}

template <typename T>
consteval auto gse::compute_state_dep_id() -> id {
	if constexpr (std::meta::is_class_member(^^T)) {
		using parent_t = typename [: std::meta::parent_of(^^T) :];
		if constexpr (requires { typename parent_t::state; }) {
			if constexpr (std::is_same_v<typename parent_t::state, T>) {
				return id_of<parent_t>();
			}
		}
	}
	return id_of<T>();
}

template <auto MemberFn, typename S>
consteval auto gse::compute_state_dep_count() -> std::size_t {
	std::size_t count = 0;
	for (auto p : std::meta::parameters_of(MemberFn)) {
		auto t = std::meta::dealias(std::meta::type_of(p));
		const bool is_dep = std::meta::extract<bool>(
			std::meta::substitute(^^is_state_dep_v, { t, ^^S })
		);
		if (is_dep) {
			++count;
		}
	}
	return count;
}

template <auto MemberFn, typename S>
consteval auto gse::compute_state_dep_ids() -> std::array<id, compute_state_dep_count<MemberFn, S>()> {
	std::array<id, compute_state_dep_count<MemberFn, S>()> result{};
	std::size_t i = 0;
	for (auto p : std::meta::parameters_of(MemberFn)) {
		auto t = std::meta::dealias(std::meta::type_of(p));
		const bool is_dep = std::meta::extract<bool>(
			std::meta::substitute(^^is_state_dep_v, { t, ^^S })
		);
		if (!is_dep) {
			continue;
		}
		const id dep_id = std::meta::extract<id>(
			std::meta::substitute(^^state_dep_id_v, { t })
		);
		result[i++] = dep_id;
	}
	return result;
}

template <auto MemberFn, typename S>
auto gse::register_state_dep_tags() -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		(([] {
			using ArgT = arg_type_of<MemberFn, Is>;
			if constexpr (is_state_dep_v<ArgT, S>) {
				(void)trace_id<dep_pointee_t<ArgT>>();
			}
		}()), ...);
	}(std::make_index_sequence<arity_of<MemberFn>>{});
}

template <typename S>
auto gse::extract_init_state_deps() -> std::vector<id> {
	if constexpr (!names_initialize<S>) {
		return {};
	}
	else {
		register_state_dep_tags<^^S::initialize, S>();
		constexpr auto deps = compute_state_dep_ids<^^S::initialize, S>();
		return std::vector<id>(deps.begin(), deps.end());
	}
}

template <typename S>
auto gse::extract_update_state_deps() -> std::vector<id> {
	if constexpr (!names_update<S>) {
		return {};
	}
	else {
		register_state_dep_tags<^^S::update, S>();
		constexpr auto deps = compute_state_dep_ids<^^S::update, S>();
		return std::vector<id>(deps.begin(), deps.end());
	}
}

template <typename S>
auto gse::extract_frame_state_deps() -> std::vector<id> {
	if constexpr (!names_frame<S>) {
		return {};
	}
	else {
		register_state_dep_tags<^^S::frame, S>();
		constexpr auto deps = compute_state_dep_ids<^^S::frame, S>();
		return std::vector<id>(deps.begin(), deps.end());
	}
}

template <typename S>
auto gse::invoke_initialize_for(init_context& phase, void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		S::initialize(
			resolve_initialize_arg<arg_type_of<^^S::initialize, Is>, S>(
				phase, d.resources, d.update_data, d.frame_data, d.settings, d.state
			)...
		);
	}(std::make_index_sequence<arity_of<^^S::initialize>>{});
}

template <typename S>
auto gse::invoke_shutdown_for(shutdown_context& phase, void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	if constexpr (shutdown_takes_resources_state<S>) {
		S::shutdown(phase, d.resources.value, d.state);
		return;
	}
	else if constexpr (shutdown_takes_state<S>) {
		S::shutdown(phase, d.state);
		return;
	}
	else if constexpr (shutdown_takes_phase_only<S>) {
		S::shutdown(phase);
		return;
	}
	else {
		static_assert(!names_shutdown<S>, "System declares shutdown but no overload matched the dispatcher");
	}
}

template <typename S>
auto gse::invoke_update_for(update_context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return S::update(
			resolve_update_arg<arg_type_of<^^S::update, Is>, S>(
				ctx, d.resources, d.update_data, d.frame_data, d.settings, d.state
			)...
		);
	}(std::make_index_sequence<arity_of<^^S::update>>{});
}

template <typename S>
auto gse::invoke_frame_for(frame_context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	const state_of_t<S>& state_ref = [&]() -> const state_of_t<S>& {
		if constexpr (std::is_trivially_copyable_v<state_of_t<S>>) {
			return d.snapshot.value;
		}
		else {
			return d.state;
		}
	}();
	const settings_storage<S>& settings_ref = [&]() -> const settings_storage<S>& {
		if constexpr (settings_is_trivial_v<S>) {
			return d.settings_snapshot.value;
		}
		else {
			return d.settings;
		}
	}();
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return S::frame(
			resolve_frame_arg<arg_type_of<^^S::frame, Is>, S>(
				ctx, d.resources, d.frame_data, settings_ref, state_ref
			)...
		);
	}(std::make_index_sequence<arity_of<^^S::frame>>{});
}

template <typename S>
auto gse::invoke_snapshot_for(void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	if constexpr (std::is_trivially_copyable_v<state_of_t<S>>) {
		d.snapshot.value = d.state;
	}
	if constexpr (settings_is_trivial_v<S>) {
		d.settings_snapshot.value.value = d.settings.value;
	}
}

template <typename S>
auto gse::invoke_apply_settings_for(void* data_ptr, channel_registry& channels_store, channel_writer& channels) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	using T = typename S::settings;
	channels_store.ensure(id_of<settings::change_request<T>>(), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<settings::change_request<T>>>();
	});
	const auto* snap = channels_store.snapshot_data(id_of<settings::change_request<T>>());
	if (!snap) {
		return;
	}
	const auto& reqs = *static_cast<const std::vector<settings::change_request<T>>*>(snap);
	for (const auto& req : reqs) {
		if (!req.apply) {
			continue;
		}
		T old_value = d.settings.value;
		req.apply(d.settings.value);
		channels.push(settings::changed<T>{ .old_value = std::move(old_value), .new_value = d.settings.value });
	}
}

template <typename S>
auto gse::data_delete_for(void* data_ptr) -> void {
	delete static_cast<system_node_data<S>*>(data_ptr);
}

template <typename S, typename... Args>
auto gse::make_system_node(Args&&... args) -> system_node {
	auto* d = new system_node_data<S>(std::forward<Args>(args)...);

	system_node node;
	node.data = std::unique_ptr<void, void(*)(void*)>(d, &data_delete_for<S>);

	if constexpr (names_initialize<S>) {
		node.invoke_initialize_fn = &invoke_initialize_for<S>;
	}
	else {
		node.invoke_initialize_fn = &noop_initialize;
	}
	if constexpr (names_shutdown<S>) {
		node.invoke_shutdown_fn = &invoke_shutdown_for<S>;
	}
	else {
		node.invoke_shutdown_fn = &noop_shutdown;
	}
	if constexpr (names_update<S>) {
		node.invoke_update_fn = &invoke_update_for<S>;
	}
	else {
		node.invoke_update_fn = &noop_dispatchers::noop_update_for<S>;
	}
	if constexpr (names_frame<S>) {
		node.invoke_frame_fn = &invoke_frame_for<S>;
	}
	else {
		node.invoke_frame_fn = &noop_dispatchers::noop_frame_for<S>;
	}
	constexpr bool has_state_snapshot = std::is_trivially_copyable_v<state_of_t<S>>;
	constexpr bool has_settings_snapshot = settings_is_trivial_v<S>;
	if constexpr (has_state_snapshot || has_settings_snapshot) {
		node.invoke_snapshot_fn = &invoke_snapshot_for<S>;
	}
	else {
		node.invoke_snapshot_fn = &noop_snapshot;
	}

	node.init_state_deps = extract_init_state_deps<S>();
	node.update_state_deps = extract_update_state_deps<S>();
	node.frame_state_deps = extract_frame_state_deps<S>();

	node.state_ptr = &d->state;
	if constexpr (has_state_snapshot) {
		node.state_snapshot_ptr = &d->snapshot.value;
	}
	if constexpr (has_resources<S>) {
		node.resources_ptr = &d->resources.value;
		node.resources_id = id_of<typename S::resources>();
	}
	if constexpr (has_settings<S>) {
		node.settings_ptr = &d->settings.value;
		node.settings_id = id_of<typename S::settings>();
		node.invoke_apply_settings_fn = &invoke_apply_settings_for<S>;
		if constexpr (has_settings_snapshot) {
			node.settings_snapshot_ptr = &d->settings_snapshot.value.value;
		}
	}
	node.has_frame = names_frame<S>;
	node.state_id = id_of<S>();
	node.update_wall_id = find_or_generate_id(std::format("update_wall:{}", type_tag<S>()));
	node.frame_wall_id = find_or_generate_id(std::format("frame_wall:{}", type_tag<S>()));
	node.trace_id = trace_id<S>();

	return node;
}
