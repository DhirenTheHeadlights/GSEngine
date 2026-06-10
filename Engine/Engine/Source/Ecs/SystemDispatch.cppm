export module gse.ecs:system_dispatch;

import std;

import gse.core;
import gse.concurrency;
import gse.diag;

import :registries;
import :access_token;
import :traits;
import :context;
import :settings;
import :context;
import :system_node;
import :shared_view;

namespace gse {

	template <typename S>
	struct system_node_data {
		template <typename... Args>
		explicit system_node_data(
			Args&&... args
		);

		state_of_t<S> state;
		[[no_unique_address]] shared_snapshot<state_of_t<S>> snapshot{};
	};

	template <typename T>
	using dep_pointee_t = std::remove_cv_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

	template <typename T>
	constexpr bool is_optional_dep_v = std::is_pointer_v<std::remove_cvref_t<T>>;

	template <typename Arg, typename S>
	constexpr bool is_state_dep_v = [] consteval {
		using U = dep_pointee_t<Arg>;
		if constexpr (std::is_same_v<U, context>) {
			return false;
		}
		else if constexpr (std::is_same_v<U, state_of_t<S>>) {
			return false;
		}
		else if constexpr (is_shared_view_v<U>) {
			return false;
		}
		else if constexpr (is_optional_shared_view_v<U>) {
			return false;
		}
		else if constexpr (is_access_v<U>) {
			return false;
		}
		else if constexpr (is_structural_v<U>) {
			return false;
		}
		else if constexpr (is_registry_access_v<U>) {
			return false;
		}
		else if constexpr (is_entities_v<U>) {
			return false;
		}
		else {
			return true;
		}
	}();

	template <typename T>
	constexpr bool is_system_data_v = [] consteval {
		constexpr auto entity = std::meta::dealias(^^T);
		if constexpr (std::meta::is_class_member(entity)) {
			if constexpr (std::meta::has_identifier(entity)) {
				return std::meta::identifier_of(entity) == "data";
			}
		}
		return false;
	}();

	template <auto MemberFn>
	constexpr std::size_t arity_of = std::meta::parameters_of(MemberFn).size();

	template <auto MemberFn, std::size_t I>
	using arg_type_of = typename[:std::meta::type_of(std::meta::parameters_of(MemberFn)[I]):];

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

	template <auto Member, typename S>
	auto call_member(
		context& ctx,
		state_of_t<S>& state
	) -> async::task<> {
		return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
			return [:Member:](resolve_run_arg<arg_type_of<Member, Is>, S>(ctx, state)...);
		}(std::make_index_sequence<arity_of<Member>>{});
	}

	template <typename S, std::size_t I = 0>
	auto run_phases_for(
		context& ctx,
		state_of_t<S>& state
	) -> async::task<> {
		if constexpr (I < run_phase_count<S>()) {
			co_await call_member<run_phase_at<S, I>(), S>(ctx, state);
			co_await run_phases_for<S, I + 1>(ctx, state);
		}
	}

	struct run_signature_metadata {
		std::vector<id> state_deps;
		std::vector<id> optional_state_deps;
		std::vector<id> component_reads;
		std::vector<id> component_writes;
	};

	struct phase_state_deps {
		std::vector<id> required;
		std::vector<id> optional;
	};

	template <typename Arg, typename S>
	auto append_arg_state_dep(
		std::vector<id>& out
	) -> void;

	template <typename Arg>
	auto append_arg_view_deps(
		std::vector<id>& required_out,
		std::vector<id>& optional_out
	) -> void;

	template <typename Arg, typename S>
	auto append_arg_run_metadata(
		run_signature_metadata& out
	) -> void;

	template <auto Fn, typename S>
	auto append_signature_run_metadata(
		run_signature_metadata& out
	) -> void;

	template <typename T>
	auto external_resource_ref(
		const task_context& ctx
	) -> const T&;

	template <typename Arg, typename S>
	auto resolve_run_arg(
		context& ctx,
		state_of_t<S>& state
	) -> decltype(auto);

	template <typename S>
	auto invoke_shutdown_for(
		void* data_ptr
	) -> void;

	template <typename S>
	auto invoke_run_for(
		context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <auto Member, typename S>
	auto invoke_member(
		context& ctx,
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
		void* data_ptr
	) -> void;

	struct noop_dispatchers {
		template <typename S>
		static auto noop_frame_for(
			context& ctx,
			void* data_ptr
		) -> async::task<>;
	};

	template <typename S>
	auto collect_run_metadata() -> run_signature_metadata;

	template <typename S>
	auto extract_init_state_deps() -> phase_state_deps;

	template <typename S>
	auto extract_frame_state_deps() -> std::vector<id>;

	template <typename T>
	constexpr id state_dep_id_v = id_of<dep_pointee_t<T>>();

	template <typename S>
	concept shutdown_takes_state = requires(state_of_t<S>& s) { S::shutdown(s); };

	template <typename S>
	concept shutdown_takes_nothing = requires { S::shutdown(); };
}

template <typename S>
template <typename... Args>
gse::system_node_data<S>::system_node_data(Args&&... args) : state(std::forward<Args>(args)...) {
}

template <typename T>
auto gse::external_resource_ref(const task_context& ctx) -> const T& {
	const void* p = ctx.resources_store.resources_ptr(id_of<T>());
	assert(p != nullptr, "external resource not found; register it with register_external_resource");
	return *static_cast<const T*>(p);
}

template <typename Arg, typename S>
auto gse::resolve_run_arg(context& ctx, state_of_t<S>& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, context>) {
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
	else if constexpr (is_registry_access_v<U>) {
		return ctx.make_registry_access();
	}
	else if constexpr (is_entities_v<U>) {
		return ctx.make_entities();
	}
	else if constexpr (is_shared_view_v<U>) {
		using Target = shared_view_target_t<U>;
		constexpr id lookup_id = id_of<Target>();
		if (ctx.live_state) {
			const void* p = ctx.states.state_ptr(lookup_id);
			assert(p != nullptr, "shared_view target system not registered");
			return make_shared_view_live<Target>(*static_cast<const state_of_t<Target>*>(p));
		}
		const void* sp = ctx.states.state_snapshot_ptr(lookup_id);
		assert(sp != nullptr, "shared_view target system not registered");
		return make_shared_view_snapshot<Target>(*static_cast<const shared_snapshot<state_of_t<Target>>*>(sp));
	}
	else if constexpr (is_optional_shared_view_v<U>) {
		using Target = optional_shared_view_target_t<U>;
		constexpr id lookup_id = id_of<Target>();
		if (ctx.live_state) {
			const void* p = ctx.states.state_ptr(lookup_id);
			if (!p) {
				return std::optional<shared_view<Target>>{};
			}
			return std::optional<shared_view<Target>>{ make_shared_view_live<Target>(*static_cast<const state_of_t<Target>*>(p)) };
		}
		const void* sp = ctx.states.state_snapshot_ptr(lookup_id);
		if (!sp) {
			return std::optional<shared_view<Target>>{};
		}
		return std::optional<shared_view<Target>>{ make_shared_view_snapshot<Target>(*static_cast<const shared_snapshot<state_of_t<Target>>*>(sp)) };
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		static_assert(
			!is_system_data_v<Pointee>,
			"cross-system state is private; use shared_view<X> ([[= gse::shared]] fields) or channels"
		);
		static_assert(
			std::is_const_v<std::remove_pointer_t<U>>,
			"external resources must be const"
		);
		return static_cast<const Pointee*>(ctx.resources_store.resources_ptr(id_of<Pointee>()));
	}
	else {
		static_assert(
			!is_system_data_v<U>,
			"cross-system state is private; use shared_view<X> ([[= gse::shared]] fields) or channels"
		);
		static_assert(
			std::is_const_v<std::remove_reference_t<Arg>>,
			"external resources must be const"
		);
		return external_resource_ref<U>(ctx);
	}
}

auto gse::noop_shutdown(void*) -> void {
}

template <typename S>
auto gse::noop_dispatchers::noop_frame_for(context&, void*) -> async::task<> {
	co_return;
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

template <typename Arg>
auto gse::append_arg_view_deps(std::vector<id>& required_out, std::vector<id>& optional_out) -> void {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (is_shared_view_v<U>) {
		required_out.push_back(id_of<shared_view_target_t<U>>());
	}
	else if constexpr (is_optional_shared_view_v<U>) {
		optional_out.push_back(id_of<optional_shared_view_target_t<U>>());
	}
}

template <typename Arg, typename S>
auto gse::append_arg_run_metadata(run_signature_metadata& out) -> void {
	append_arg_state_dep<Arg, S>(out.state_deps);
	append_arg_view_deps<Arg>(out.state_deps, out.optional_state_deps);

	using ArgT = std::remove_cvref_t<Arg>;
	if constexpr (is_access_v<ArgT> && is_read_access_v<ArgT>) {
		out.component_reads.push_back(id_of<access_element_t<ArgT>>());
	}

	if constexpr (is_access_v<ArgT> && !is_read_access_v<ArgT>) {
		out.component_writes.push_back(id_of<access_element_t<ArgT>>());
	}
	else if constexpr (is_structural_v<ArgT>) {
		out.component_writes.push_back(id_of<structural_element_t<ArgT>>());
	}
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
auto gse::extract_init_state_deps() -> phase_state_deps {
	phase_state_deps out;
	if constexpr (names_init<S>) {
		[&]<std::size_t... Is>(std::index_sequence<Is...>) {
			(([&] {
				using ArgT = arg_type_of<^^S::init, Is>;
				append_arg_state_dep<ArgT, S>(out.required);
				append_arg_view_deps<ArgT>(out.required, out.optional);
			}()), ...);
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
auto gse::invoke_shutdown_for(void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	if constexpr (shutdown_takes_state<S>) {
		S::shutdown(d.state);
		return;
	}
	else if constexpr (shutdown_takes_nothing<S>) {
		S::shutdown();
		return;
	}
	else {
		static_assert(!names_shutdown<S>, "System declares shutdown but no overload matched the dispatcher");
	}
}

template <typename S>
auto gse::invoke_run_for(context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);

	if constexpr (names_run_phased<S>) {
		return run_phases_for<S>(ctx, d.state);
	}
	else {
		return call_member<^^S::run, S>(ctx, d.state);
	}
}

template <auto Member, typename S>
auto gse::invoke_member(context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	return call_member<Member, S>(ctx, d.state);
}

template <typename S>
auto gse::invoke_snapshot_for(void* data_ptr) -> void {
	auto& d = *static_cast<system_node_data<S>*>(data_ptr);
	copy_shared_fields(d.state, d.snapshot);
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
		node.invoke_init_fn = &invoke_member<^^S::init, S>;
		node.resume_event = std::make_unique<async::manual_event>();
		node.paused_event = std::make_unique<async::manual_event>();
	}
	else {
		node.init_done = true;
	}
	if constexpr (names_frame<S>) {
		node.invoke_frame_fn = &invoke_member<^^S::frame, S>;
	}
	else {
		node.invoke_frame_fn = &noop_dispatchers::noop_frame_for<S>;
	}
	node.invoke_snapshot_fn = &invoke_snapshot_for<S>;

	auto run_metadata = collect_run_metadata<S>();
	node.run_state_deps = std::move(run_metadata.state_deps);
	node.optional_run_state_deps = std::move(run_metadata.optional_state_deps);
	auto init_deps = extract_init_state_deps<S>();
	node.init_state_deps = std::move(init_deps.required);
	node.optional_init_state_deps = std::move(init_deps.optional);
	node.frame_state_deps = extract_frame_state_deps<S>();
	node.component_reads = std::move(run_metadata.component_reads);
	node.component_writes = std::move(run_metadata.component_writes);

	node.state_ptr = &d->state;
	node.state_snapshot_ptr = &d->snapshot;
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
