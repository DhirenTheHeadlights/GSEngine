export module gse.ecs:system_dispatch;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;
import gse.log;

import :phase_context;
import :registries;
import :access_token;
import :traits;
import :run_context;
import :settings;
import :frame_context;
import :system_node;
import :shared_view;

export namespace gse {
	template <typename S, bool = names_data<S>>
	struct state_of_helper {
		using type = S;
	};

	template <typename S>
	struct state_of_helper<S, true> {
		using type = typename S::data;
	};

	template <typename S>
	using state_of_t = typename state_of_helper<S>::type;
}

namespace gse {

	template <typename State>
	constexpr bool state_snapshotable_v = std::is_trivially_copyable_v<State> && std::is_copy_assignable_v<State>;

	template <typename State, bool = state_snapshotable_v<State>>
	struct snapshot_storage {};

	template <typename State>
	struct snapshot_storage<State, true> {
		State value{};
	};

	template <typename S>
	struct system_node_data {
		template <typename... Args>
		explicit system_node_data(
			Args&&... args
		);

		state_of_t<S> state;
		[[no_unique_address]] snapshot_storage<state_of_t<S>> snapshot;
	};

	template <typename T>
	using dep_pointee_t = std::remove_cv_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

	template <typename T>
	constexpr bool is_optional_dep_v = std::is_pointer_v<std::remove_cvref_t<T>>;

	template <typename Arg, typename S>
	constexpr bool is_state_dep_v = [] consteval {
		using U = dep_pointee_t<Arg>;
		if constexpr (std::is_same_v<U, run_context>) {
			return false;
		}
		else if constexpr (std::is_same_v<U, frame_context>) {
			return false;
		}
		else if constexpr (std::is_same_v<U, state_of_t<S>>) {
			return false;
		}
		else if constexpr (is_shared_view_v<U>) {
			return false;
		}
		else if constexpr (is_access_decl_v<U>) {
			return false;
		}
		else if constexpr (is_access_v<U>) {
			return false;
		}
		else if constexpr (is_structural_v<U>) {
			return false;
		}
		else {
			return true;
		}
	}();

	template <typename Arg, typename S>
	constexpr bool is_cyclic_frame_dep_v = [] consteval -> bool {
		if constexpr (!is_state_dep_v<Arg, S>) {
			return false;
		}
		else {
			using U = dep_pointee_t<Arg>;
			constexpr auto entity = std::meta::dealias(^^U);
			if constexpr (std::meta::is_class_member(entity)) {
				using parent_t = typename[:std::meta::parent_of(entity):];
				if constexpr (requires { typename parent_t::data; }) {
					if constexpr (std::is_same_v<typename parent_t::data, U>) {
						return names_frame<parent_t>;
					}
				}
			}
			return false;
		}
	}();

	template <auto MemberFn>
	constexpr std::size_t arity_of = std::meta::parameters_of(MemberFn).size();

	template <auto MemberFn, std::size_t I>
	using arg_type_of = typename[:std::meta::type_of(std::meta::parameters_of(MemberFn)[I]):];

	template <auto MemberFn, typename S>
	consteval auto frame_signature_has_cyclic_deps() -> bool {
		bool cyclic = false;
		[&]<std::size_t... Is>(std::index_sequence<Is...>) {
			(([&] {
				using ArgT = arg_type_of<MemberFn, Is>;
				if constexpr (is_cyclic_frame_dep_v<ArgT, S>) {
					cyclic = true;
				}
			}()), ...);
		}(std::make_index_sequence<arity_of<MemberFn>>{});
		return cyclic;
	}

	template <typename S>
	consteval auto run_phase_members() -> std::vector<std::meta::info> {
		std::vector<std::meta::info> phases;
		for (auto m : std::meta::members_of(^^typename S::run, std::meta::access_context::unchecked())) {
			if (std::meta::is_function(m) && std::meta::is_static_member(m)) {
				phases.push_back(m);
			}
		}
		return phases;
	}

	template <typename S>
	constexpr auto run_phases_v = std::define_static_array(run_phase_members<S>());

	template <typename S>
	consteval auto run_phase_count() -> std::size_t {
		return run_phases_v<S>.size();
	}

	template <typename S, std::size_t I>
	consteval auto run_phase_at() -> std::meta::info {
		return run_phases_v<S>[I];
	}

	template <typename S>
	auto signature_acquire_trace_id() -> id {
		return find_or_generate_id(std::format("run_acquire:{}", type_tag<S>()));
	}

	template <auto Fn, typename S>
	auto fill_signature_locks(
		std::span<id> lock_ids,
		std::span<lock_fn> lock_fns
	) -> std::size_t {
		std::size_t k = 0;
		[&]<std::size_t... Is>(std::index_sequence<Is...>) {
			(([&] {
				using U = std::remove_cvref_t<arg_type_of<Fn, Is>>;
				if constexpr (is_access_v<U>) {
					lock_ids[k] = id_of<access_element_t<U>>();
					lock_fns[k] = is_read_access_v<U> ? &acquire_shared : &acquire_exclusive;
					++k;
				}
				else if constexpr (is_structural_v<U>) {
					lock_ids[k] = id_of<structural_element_t<U>>();
					lock_fns[k] = &acquire_exclusive;
					++k;
				}
			}()), ...);
		}(std::make_index_sequence<arity_of<Fn>>{});
		return k;
	}

	template <auto Phase, typename S>
	auto run_phase(
		run_context& ctx,
		state_of_t<S>& state
	) -> async::task<> {
		if constexpr (arity_of<Phase> > 0) {
			std::array<id, arity_of<Phase>> lock_ids{};
			std::array<lock_fn, arity_of<Phase>> lock_fns{};
			const std::size_t n_locks = fill_signature_locks<Phase, S>(lock_ids, lock_fns);
			if (n_locks > 0) {
				static const id tid = signature_acquire_trace_id<S>();
				co_await acquire_locks_in_sorted_order(
					ctx.access_mutexes(),
					std::span(lock_ids.data(), n_locks),
					std::span(lock_fns.data(), n_locks),
					tid
				);
			}
		}
		co_await [&]<std::size_t... Is>(std::index_sequence<Is...>) -> async::task<> {
			co_await [:Phase:](resolve_run_arg<arg_type_of<Phase, Is>, S>(ctx, state)...);
		}(std::make_index_sequence<arity_of<Phase>>{});
	}

	template <typename S, std::size_t I = 0>
	auto run_phases_for(
		run_context& ctx,
		state_of_t<S>& state
	) -> async::task<> {
		if constexpr (I < run_phase_count<S>()) {
			co_await run_phase<run_phase_at<S, I>(), S>(ctx, state);
			co_await run_phases_for<S, I + 1>(ctx, state);
		}
	}

	struct run_signature_metadata {
		std::vector<id> state_deps;
		std::vector<id> component_reads;
		std::vector<id> component_writes;
	};

	template <typename Arg, typename S>
	auto append_arg_state_dep(
		std::vector<id>& out
	) -> void;

	template <typename Arg, typename S>
	auto append_arg_run_metadata(
		run_signature_metadata& out
	) -> void;

	template <auto Fn, typename S>
	auto append_signature_state_deps(
		std::vector<id>& out
	) -> void;

	template <auto Fn, typename S>
	auto append_signature_run_metadata(
		run_signature_metadata& out
	) -> void;

	template <typename T>
	auto direct_state_ref(
		const task_context& ctx
	) -> const T&;

	template <typename Arg, typename S>
	auto resolve_run_arg(
		run_context& ctx,
		state_of_t<S>& state
	) -> decltype(auto);

	template <typename Arg, typename S>
	auto resolve_frame_arg(
		frame_context& ctx,
		state_of_t<S>& state
	) -> decltype(auto);

	template <typename S>
	auto invoke_shutdown_for(
		shutdown_context& phase,
		void* data_ptr
	) -> void;

	template <typename S>
	auto invoke_run_for(
		run_context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <typename S>
	auto invoke_init_for(
		run_context& ctx,
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

	auto noop_shutdown(
		shutdown_context& phase,
		void* data_ptr
	) -> void;

	struct noop_dispatchers {
		template <typename S>
		static auto noop_frame_for(
			frame_context& ctx,
			void* data_ptr
		) -> async::task<>;
	};

	auto noop_snapshot(
		void* data_ptr
	) -> void;

	template <typename S>
	auto collect_run_metadata() -> run_signature_metadata;

	template <typename S>
	auto extract_init_state_deps() -> std::vector<id>;

	template <typename S>
	auto extract_frame_state_deps() -> std::vector<id>;

	template <typename T>
	constexpr id state_dep_id_v = id_of<dep_pointee_t<T>>();

	template <typename S>
	concept shutdown_takes_state = requires(shutdown_context& p, state_of_t<S>& s) { S::shutdown(p, s); };

	template <typename S>
	concept shutdown_takes_phase_only = requires(shutdown_context& p) { S::shutdown(p); };
}

template <typename S>
template <typename... Args>
gse::system_node_data<S>::system_node_data(Args&&... args) : state(std::forward<Args>(args)...) {
}

template <typename T>
auto gse::direct_state_ref(const task_context& ctx) -> const T& {
	constexpr id state_lookup_id = id_of<T>();
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
	assert(p != nullptr, "cross-system state or external resource not found");
	return *static_cast<const T*>(p);
}

template <typename Arg, typename S>
auto gse::resolve_run_arg(run_context& ctx, state_of_t<S>& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, run_context>) {
		return (ctx);
	}
	else if constexpr (std::is_same_v<U, state_of_t<S>>) {
		return (state);
	}
	else if constexpr (is_access_v<U>) {
		return ctx.template make_access<U>();
	}
	else if constexpr (is_structural_v<U>) {
		return ctx.template make_structural<structural_element_t<U>>();
	}
	else if constexpr (is_access_decl_v<U>) {
		return U{};
	}
	else if constexpr (is_shared_view_v<U>) {
		using Target = shared_view_target_t<U>;
		constexpr id lookup_id = id_of<Target>();
		const void* p = ctx.states.state_ptr(lookup_id);
		assert(p != nullptr, "shared_view target system not registered");
		return make_shared_view<Target>(*static_cast<const typename Target::data*>(p));
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		static_assert(
			std::is_const_v<std::remove_pointer_t<U>>,
			"cross-system state must be const; use channels for mutation"
		);
		constexpr id state_lookup_id = id_of<Pointee>();
		const void* p = ctx.live_state
			? ctx.states.state_ptr(state_lookup_id)
			: (ctx.states.state_snapshot_ptr(state_lookup_id) ? ctx.states.state_snapshot_ptr(state_lookup_id)
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
auto gse::resolve_frame_arg(frame_context& ctx, state_of_t<S>& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, frame_context>) {
		return (ctx);
	}
	else if constexpr (std::is_same_v<U, state_of_t<S>>) {
		return (state);
	}
	else if constexpr (is_shared_view_v<U>) {
		using Target = shared_view_target_t<U>;
		constexpr id lookup_id = id_of<Target>();
		const void* p = ctx.live_state
			? ctx.states.state_ptr(lookup_id)
			: (ctx.states.state_snapshot_ptr(lookup_id) ? ctx.states.state_snapshot_ptr(lookup_id)
														: ctx.states.state_ptr(lookup_id));
		assert(p != nullptr, "shared_view target system not registered");
		return make_shared_view<Target>(*static_cast<const typename Target::data*>(p));
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		static_assert(
			std::is_const_v<std::remove_pointer_t<U>>,
			"cross-system state must be const; use channels for mutation"
		);
		constexpr id state_lookup_id = id_of<Pointee>();
		const void* p = ctx.live_state
			? ctx.states.state_ptr(state_lookup_id)
			: (ctx.states.state_snapshot_ptr(state_lookup_id) ? ctx.states.state_snapshot_ptr(state_lookup_id)
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

auto gse::noop_shutdown(shutdown_context&, void*) -> void {
}

template <typename S>
auto gse::noop_dispatchers::noop_frame_for(frame_context&, void*) -> async::task<> {
	co_return;
}

auto gse::noop_snapshot(void*) -> void {
}

template <typename Arg, typename S>
auto gse::append_arg_state_dep(std::vector<id>& out) -> void {
	if constexpr (is_state_dep_v<Arg, S>) {
		(void)trace_id<dep_pointee_t<Arg>>();
		if constexpr (!is_optional_dep_v<Arg>) {
			out.push_back(state_dep_id_v<Arg>);
		}
	}
}

template <typename Arg, typename S>
auto gse::append_arg_run_metadata(run_signature_metadata& out) -> void {
	append_arg_state_dep<Arg, S>(out.state_deps);

	using ArgT = std::remove_cvref_t<Arg>;
	if constexpr (is_reads_v<ArgT>) {
		const auto ids = ArgT::component_ids();
		out.component_reads.insert(out.component_reads.end(), ids.begin(), ids.end());
	}
	else if constexpr (is_access_v<ArgT> && is_read_access_v<ArgT>) {
		out.component_reads.push_back(id_of<access_element_t<ArgT>>());
	}

	if constexpr (is_writes_v<ArgT>) {
		const auto ids = ArgT::component_ids();
		out.component_writes.insert(out.component_writes.end(), ids.begin(), ids.end());
	}
	else if constexpr (is_access_v<ArgT> && !is_read_access_v<ArgT>) {
		out.component_writes.push_back(id_of<access_element_t<ArgT>>());
	}
	else if constexpr (is_structural_v<ArgT>) {
		out.component_writes.push_back(id_of<structural_element_t<ArgT>>());
	}
}

template <auto Fn, typename S>
auto gse::append_signature_state_deps(std::vector<id>& out) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((append_arg_state_dep<arg_type_of<Fn, Is>, S>(out)), ...);
	}(std::make_index_sequence<arity_of<Fn>>{});
	log::println(
		"[sig-state-deps] S={} fn={} fn_arity={} collected={}",
		type_tag<S>(),
		std::meta::display_string_of(Fn),
		arity_of<Fn>,
		out.size()
	);
	log::flush();
}

template <auto Fn, typename S>
auto gse::append_signature_run_metadata(run_signature_metadata& out) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((append_arg_run_metadata<arg_type_of<Fn, Is>, S>(out)), ...);
	}(std::make_index_sequence<arity_of<Fn>>{});
}

template <typename S>
auto gse::collect_run_metadata() -> run_signature_metadata {
	run_signature_metadata out;
	if constexpr (names_run_fn<S>) {
		append_signature_run_metadata<^^S::run, S>(out);
	}
	else if constexpr (names_run_phased<S>) {
		template for (constexpr auto phase : run_phases_v<S>) {
			append_signature_run_metadata<phase, S>(out);
		}
	}
	return out;
}

template <typename S>
auto gse::extract_init_state_deps() -> std::vector<id> {
	std::vector<id> out;
	if constexpr (names_init<S>) {
		[&]<std::size_t... Is>(std::index_sequence<Is...>) {
			((append_arg_state_dep<arg_type_of<^^S::init, Is>, S>(out)), ...);
		}(std::make_index_sequence<arity_of<^^S::init>>{});
	}
	return out;
}

template <typename S>
auto gse::extract_frame_state_deps() -> std::vector<id> {
	std::vector<id> out;
	if constexpr (names_frame<S>) {
		[&]<std::size_t... Is>(std::index_sequence<Is...>) {
			((append_arg_state_dep<arg_type_of<^^S::frame, Is>, S>(out)), ...);
		}(std::make_index_sequence<arity_of<^^S::frame>>{});
	}
	return out;
}

template <typename S>
auto gse::invoke_shutdown_for(shutdown_context& phase, void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	if constexpr (shutdown_takes_state<S>) {
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
auto gse::invoke_run_for(run_context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);

	if constexpr (names_run_phased<S>) {
		co_await run_phases_for<S>(ctx, d.state);
	}
	else {
		if constexpr (arity_of<^^S::run> > 0) {
			std::array<id, arity_of<^^S::run>> lock_ids{};
			std::array<lock_fn, arity_of<^^S::run>> lock_fns{};
			const std::size_t n_locks = fill_signature_locks<^^S::run, S>(lock_ids, lock_fns);
			if (n_locks > 0) {
				static const id tid = signature_acquire_trace_id<S>();
				co_await acquire_locks_in_sorted_order(
					ctx.access_mutexes(),
					std::span(lock_ids.data(), n_locks),
					std::span(lock_fns.data(), n_locks),
					tid
				);
			}
		}
		co_await [&]<std::size_t... Is>(std::index_sequence<Is...>) -> async::task<> {
			co_await S::run(resolve_run_arg<arg_type_of<^^S::run, Is>, S>(ctx, d.state)...);
		}(std::make_index_sequence<arity_of<^^S::run>>{});
	}
}

template <typename S>
auto gse::invoke_init_for(run_context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return S::init(resolve_run_arg<arg_type_of<^^S::init, Is>, S>(ctx, d.state)...);
	}(std::make_index_sequence<arity_of<^^S::init>>{});
}

template <typename S>
auto gse::invoke_frame_for(frame_context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return S::frame(resolve_frame_arg<arg_type_of<^^S::frame, Is>, S>(ctx, d.state)...);
	}(std::make_index_sequence<arity_of<^^S::frame>>{});
}

template <typename S>
auto gse::invoke_snapshot_for(void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	if constexpr (state_snapshotable_v<state_of_t<S>>) {
		d.snapshot.value = d.state;
	}
}

template <typename S>
auto gse::invoke_apply_settings_for(void* data_ptr, channel_registry& channels_store, channel_writer& channels) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	using data_t = typename S::data;
	const auto& reqs = channels_store.ensure_typed<settings::change_request<S>>().data.read_raw();
	for (const auto& req : reqs) {
		if (!req.apply) {
			continue;
		}
		if constexpr (std::is_trivially_copyable_v<data_t>) {
			data_t old_value = d.state;
			req.apply(d.state);
			channels.push<settings::changed<S>>({
				.old_value = std::move(old_value),
				.new_value = d.state
			});
		}
		else {
			req.apply(d.state);
		}
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
	node.data = std::unique_ptr<void, void (*)(void*)>(d, &data_delete_for<S>);

	if constexpr (names_shutdown<S>) {
		node.invoke_shutdown_fn = &invoke_shutdown_for<S>;
	}
	else {
		node.invoke_shutdown_fn = &noop_shutdown;
	}
	if constexpr (names_run<S>) {
		node.invoke_run_fn = &invoke_run_for<S>;
	}
	if constexpr (names_init<S>) {
		node.invoke_init_fn = &invoke_init_for<S>;
		node.resume_event = std::make_unique<async::manual_event>();
		node.paused_event = std::make_unique<async::manual_event>();
	}
	else {
		node.init_done = true;
	}
	if constexpr (names_frame<S>) {
		static_assert(
			!frame_signature_has_cyclic_deps<^^S::frame, S>(),
			"frame() has a const-ref dependency on another system's data, where the target system also defines "
			"frame(). "
			"This deadlocks the scheduler: state_ready for the target only fires after its pass dispatches in "
			"execute_frame, "
			"which is after the channel drain ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â so this frame's pass push misses the drain and the coroutine never "
			"resumes. "
			"Use shared_view<X> for snapshot reads (annotate the fields you need with [[= gse::shared]]), or channels "
			"for "
			"cross-frame communication."
		);
		node.invoke_frame_fn = &invoke_frame_for<S>;
	}
	else {
		node.invoke_frame_fn = &noop_dispatchers::noop_frame_for<S>;
	}
	constexpr bool has_state_snapshot = state_snapshotable_v<state_of_t<S>>;
	if constexpr (has_state_snapshot) {
		node.invoke_snapshot_fn = &invoke_snapshot_for<S>;
	}
	else {
		node.invoke_snapshot_fn = &noop_snapshot;
	}

	auto run_metadata = collect_run_metadata<S>();
	node.run_state_deps = std::move(run_metadata.state_deps);
	node.init_state_deps = extract_init_state_deps<S>();
	node.frame_state_deps = extract_frame_state_deps<S>();
	node.component_reads = std::move(run_metadata.component_reads);
	node.component_writes = std::move(run_metadata.component_writes);

	node.state_ptr = &d->state;
	if constexpr (has_state_snapshot) {
		node.state_snapshot_ptr = &d->snapshot.value;
	}
	if constexpr (has_settings<S>) {
		node.invoke_apply_settings_fn = &invoke_apply_settings_for<S>;
		node.settings_record = settings::build_settings_record<S>(d->state);
	}
	node.has_frame = names_frame<S>;
	node.state_id = id_of<S>();
	if constexpr (names_data<S>) {
		node.state_type_id = id_of<typename S::data>();
	}
	node.update_wall_id = find_or_generate_id(std::format("update_wall:{}", type_tag<S>()));
	node.frame_wall_id = find_or_generate_id(std::format("frame_wall:{}", type_tag<S>()));
	node.frame_start_id = find_or_generate_id(std::format("frame_start:{}", type_tag<S>()));
	node.trace_id = trace_id<S>();
	node.system_name = std::string(type_tag<S>());

	return node;
}
