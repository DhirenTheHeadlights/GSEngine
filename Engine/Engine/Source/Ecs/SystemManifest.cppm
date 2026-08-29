export module gse.system_manifest;

import std;

import gse.core;
import gse.meta;
import gse.concurrency;
import gse.ecs;

export namespace gse {
	template <typename R>
	concept system_node_registrar = requires(R& r, system_node n) {
		r.add_system_node(std::move(n));
	};

	template <std::meta::info... Entries>
	struct system_manifest {
		template <system_node_registrar R>
		auto register_with(
			R& registrar
		) const -> void;
	};

	template <std::meta::info... Namespaces, system_node_registrar R>
	auto register_systems(
		R& registrar
	) -> void;
}

namespace gse {
	template <typename State>
	struct annotated_system_data {
		State state{};
		[[no_unique_address]] shared_snapshot<State> snapshot{};
	};

	template <std::meta::info Ns>
	struct stateless_system {};

	template <typename T>
	struct stateless_traits {
		static constexpr bool is_stateless = false;
	};

	template <std::meta::info Ns>
	struct stateless_traits<stateless_system<Ns>> {
		static constexpr bool is_stateless = true;
		static constexpr std::meta::info ns = Ns;
	};

	template <std::meta::info FnInfo, typename State>
	auto call_annotated_fn(
		context& ctx,
		State& state
	) -> async::task<>;

	template <std::meta::info FnInfo, typename State>
	auto invoke_annotated_fn(
		context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <std::meta::info FnInfo, typename State>
	auto invoke_annotated_shutdown(
		void* data_ptr
	) -> void;

	template <typename State>
	auto invoke_annotated_snapshot(
		void* data_ptr
	) -> void;

	template <typename State>
	auto invoke_annotated_apply_settings(
		void* data_ptr,
		channel_registry& channels_store
	) -> void;

	template <typename State>
	auto annotated_data_delete(
		void* data_ptr
	) -> void;

	template <std::meta::info FnInfo, typename State>
	auto extract_fn_init_deps() -> phase_state_deps;

	template <std::meta::info FnInfo, typename State>
	auto extract_fn_frame_deps() -> std::vector<id>;

	template <std::meta::info FnInfo, typename State>
	auto wire_annotated_fn(
		system_node& node,
		run_signature_metadata& run_meta,
		phase_state_deps& init_deps,
		std::vector<id>& frame_deps
	) -> void;

	consteval auto is_run_phase_fn(
		std::meta::info fn
	) -> bool;

	template <typename State, std::meta::info... RunInfos>
	auto invoke_run_phases(
		context& ctx,
		void* data_ptr
	) -> async::task<>;

	template <typename State, std::meta::info First, std::meta::info... Rest>
	auto run_phase_chain(
		context& ctx,
		State& state
	) -> async::task<>;

	template <std::meta::info FnInfo>
	auto collect_fn_shared_views(
		std::vector<id>& out
	) -> void;

	template <std::meta::info FnInfo>
	auto collect_fn_channels(
		std::vector<id>& reads_out,
		std::vector<id>& writes_out
	) -> void;

	template <typename Arg>
	auto ensure_arg_storage(
		registry& reg
	) -> void;

	template <std::meta::info FnInfo>
	auto ensure_fn_storages(
		registry& reg
	) -> void;

	template <std::meta::info... FnInfos>
	auto ensure_node_storages(
		registry& reg
	) -> void;

	template <std::meta::info StateInfo, std::meta::info... FnInfos>
	auto make_annotated_system_node(
		settings::draw_page_thunk page_thunk = nullptr
	) -> system_node;

	consteval auto collect_namespace_systems(
		std::meta::info ns
	) -> std::vector<std::meta::info>;

	template <std::meta::info... Namespaces>
	consteval auto collect_all_namespace_systems() -> std::vector<std::meta::info>;

	template <std::meta::info... Namespaces>
	struct namespace_system_entries {
		static constexpr auto value = std::define_static_array(collect_all_namespace_systems<Namespaces...>());
	};

	consteval auto is_system_state_entry(
		std::meta::info entry
	) -> bool;

	consteval auto state_entry_count_for_namespace(
		std::meta::info ns,
		std::vector<std::meta::info> entries
	) -> std::size_t;

	consteval auto system_namespaces(
		std::vector<std::meta::info> entries
	) -> std::vector<std::meta::info>;

	consteval auto state_entry_for_namespace(
		std::meta::info ns,
		std::vector<std::meta::info> entries
	) -> std::meta::info;

	consteval auto hook_fns_in_namespace(
		std::meta::info ns,
		std::vector<std::meta::info> entries
	) -> std::vector<std::meta::info>;

	consteval auto page_fn_for_state(
		std::meta::info state,
		std::vector<std::meta::info> entries
	) -> std::meta::info;

	template <std::meta::info PageInfo>
	auto page_thunk_for(
		void* builder,
		void* panel_state,
		settings::change_request_writer channels,
		const void* entry
	) -> void;
}

namespace gse::settings {
	template <std::meta::info M, std::meta::info... Rest>
	auto resolve_settings_member(
		auto&& s
	) -> decltype(auto);

	template <typename State, std::meta::info... Path>
	auto format_annotated_field(
		const void* settings_ptr
	) -> std::string;

	template <typename State, std::meta::info... Path>
	auto annotated_field_options(
		const void* settings_ptr
	) -> std::vector<std::string>;

	template <typename State, std::meta::info... Path>
	auto push_annotated_field_change(
		channel_write<change_request> channels,
		std::string_view raw
	) -> bool;

	template <typename State, std::meta::info... Path>
	auto make_annotated_field(
		std::string key
	) -> settings_field;

	template <typename State, typename Cur, scope_kind Inherited, std::meta::info... Prefix>
	auto collect_annotated_fields_into(
		std::vector<settings_field>& out,
		std::string_view prefix
	) -> void;

	template <typename State>
	auto collect_annotated_fields() -> std::vector<settings_field>;

	template <typename State>
	auto reset_annotated_defaults(
		channel_write<change_request> channels
	) -> void;

	template <typename State>
	auto build_annotated_settings_record(
		State& obj,
		std::string_view category,
		draw_page_thunk page
	) -> register_settings_type;
}

template <std::meta::info FnInfo, typename State>
auto gse::call_annotated_fn(context& ctx, State& state) -> async::task<> {
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		return [:FnInfo:](resolve_run_arg<arg_type_of<FnInfo, Is>, State>(ctx, state)...);
	}(std::make_index_sequence<arity_of<FnInfo>>{});
}

template <std::meta::info FnInfo, typename State>
auto gse::invoke_annotated_fn(context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<annotated_system_data<State>*>(data_ptr);
	return call_annotated_fn<FnInfo, State>(ctx, d.state);
}

template <std::meta::info FnInfo, typename State>
auto gse::invoke_annotated_shutdown(void* data_ptr) -> void {
	auto& d = *static_cast<annotated_system_data<State>*>(data_ptr);
	[:FnInfo:](d.state);
}

template <typename State>
auto gse::invoke_annotated_snapshot(void* data_ptr) -> void {
	auto& d = *static_cast<annotated_system_data<State>*>(data_ptr);
	copy_shared_fields(d.state, d.snapshot);
}

template <typename State>
auto gse::invoke_annotated_apply_settings(void* data_ptr, channel_registry& channels_store) -> void {
	auto& d = *static_cast<annotated_system_data<State>*>(data_ptr);
	const auto& reqs = channels_store.ensure_typed<settings::change_request>().data.read_raw();
	for (const auto& req : reqs) {
		if (!req.apply || req.state_type != id_of<State>()) {
			continue;
		}
		req.apply(&d.state);
	}
}

template <typename State>
auto gse::annotated_data_delete(void* data_ptr) -> void {
	delete static_cast<annotated_system_data<State>*>(data_ptr);
}

template <std::meta::info FnInfo, typename State>
auto gse::extract_fn_init_deps() -> phase_state_deps {
	phase_state_deps out;
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		(([&] {
			using arg_t = arg_type_of<FnInfo, Is>;
			append_arg_state_dep<arg_t, State>(out.required);
			append_arg_view_deps<arg_t>(out.required, out.optional);
		}()), ...);
	}(std::make_index_sequence<arity_of<FnInfo>>{});
	return out;
}

template <std::meta::info FnInfo, typename State>
auto gse::extract_fn_frame_deps() -> std::vector<id> {
	std::vector<id> out;
	std::vector<id> optional_discard;
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		(([&] {
			using arg_t = arg_type_of<FnInfo, Is>;
			append_arg_state_dep<arg_t, State>(out);
			append_arg_view_deps<arg_t>(out, optional_discard);
		}()), ...);
	}(std::make_index_sequence<arity_of<FnInfo>>{});

	template for (constexpr auto s : std::define_static_array(meta::required_order_deps_of(FnInfo))) {
		out.push_back(id_of<typename[:s:]>());
	}

	return out;
}

template <std::meta::info FnInfo, typename State>
auto gse::wire_annotated_fn(system_node& node, run_signature_metadata& run_meta, phase_state_deps& init_deps, std::vector<id>& frame_deps) -> void {
	constexpr auto hook = meta::hook_kind_of(meta::find_system_hook_anno(FnInfo));
	if constexpr (hook == system_hook::init) {
		node.invoke_init_fn = &invoke_annotated_fn<FnInfo, State>;
		node.resume_event = std::make_unique<async::manual_event>();
		node.paused_event = std::make_unique<async::manual_event>();
		auto deps = extract_fn_init_deps<FnInfo, State>();
		init_deps.required.insert(init_deps.required.end(), deps.required.begin(), deps.required.end());
		init_deps.optional.insert(init_deps.optional.end(), deps.optional.begin(), deps.optional.end());
	}
	else if constexpr (hook == system_hook::run) {
		append_signature_run_metadata<FnInfo, State>(run_meta);
	}
	else if constexpr (hook == system_hook::frame) {
		node.invoke_frame_fn = &invoke_annotated_fn<FnInfo, State>;
		node.has_frame = true;
		auto deps = extract_fn_frame_deps<FnInfo, State>();
		frame_deps.insert(frame_deps.end(), deps.begin(), deps.end());
	}
	else {
		node.invoke_shutdown_fn = &invoke_annotated_shutdown<FnInfo, State>;
	}
}

consteval auto gse::is_run_phase_fn(const std::meta::info fn) -> bool {
	const auto anno = meta::find_system_hook_anno(fn);
	return anno != std::meta::info{} && meta::hook_kind_of(anno) == system_hook::run;
}

template <typename State, std::meta::info First, std::meta::info... Rest>
auto gse::run_phase_chain(context& ctx, State& state) -> async::task<> {
	co_await call_annotated_fn<First, State>(ctx, state);
	if constexpr (sizeof...(Rest) > 0) {
		co_await run_phase_chain<State, Rest...>(ctx, state);
	}
}

template <typename State, std::meta::info... RunInfos>
auto gse::invoke_run_phases(context& ctx, void* data_ptr) -> async::task<> {
	auto& d = *static_cast<annotated_system_data<State>*>(data_ptr);
	co_await run_phase_chain<State, RunInfos...>(ctx, d.state);
}

template <std::meta::info FnInfo>
auto gse::collect_fn_shared_views(std::vector<id>& out) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((append_arg_shared_view<arg_type_of<FnInfo, Is>>(out)), ...);
	}(std::make_index_sequence<arity_of<FnInfo>>{});
}

template <std::meta::info FnInfo>
auto gse::collect_fn_channels(std::vector<id>& reads_out, std::vector<id>& writes_out) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((append_arg_channel_ids<arg_type_of<FnInfo, Is>>(reads_out, writes_out)), ...);
	}(std::make_index_sequence<arity_of<FnInfo>>{});
}

template <typename Arg>
auto gse::ensure_arg_storage(registry& reg) -> void {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (is_access_v<U>) {
		reg.ensure_storage<access_element_t<U>>();
	}
	else if constexpr (is_structural_v<U>) {
		reg.ensure_storage<structural_element_t<U>>();
	}
}

template <std::meta::info FnInfo>
auto gse::ensure_fn_storages(registry& reg) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((ensure_arg_storage<arg_type_of<FnInfo, Is>>(reg)), ...);
	}(std::make_index_sequence<arity_of<FnInfo>>{});
}

template <std::meta::info... FnInfos>
auto gse::ensure_node_storages(registry& reg) -> void {
	(ensure_fn_storages<FnInfos>(reg), ...);
}

template <std::meta::info StateInfo, std::meta::info... FnInfos>
auto gse::make_annotated_system_node(settings::draw_page_thunk page_thunk) -> system_node {
	using state_t = [:StateInfo:];
	(void)trace_id<state_t>();
	auto* d = new annotated_system_data<state_t>();

	system_node node;
	node.data = std::unique_ptr<void, void (*)(void*)>(d, &annotated_data_delete<state_t>);

	run_signature_metadata run_meta;
	phase_state_deps init_deps;
	std::vector<id> frame_deps;
	(wire_annotated_fn<FnInfos, state_t>(node, run_meta, init_deps, frame_deps), ...);

	constexpr auto run_phases = std::define_static_array([] consteval {
		std::vector<std::meta::info> rs;
		for (const auto fn : std::array<std::meta::info, sizeof...(FnInfos)>{ FnInfos... }) {
			if (is_run_phase_fn(fn)) {
				rs.push_back(fn);
			}
		}
		std::ranges::sort(rs, {}, [](const std::meta::info fn) {
			return meta::run_order_of(meta::find_system_hook_anno(fn));
		});
		return rs;
	}());

	static_assert(
		run_phases.size() <= 4,
		"a system namespace declares more than four [[= gse::system_run]] phases; extend the invoke_run_phases chain in make_annotated_system_node, otherwise the extra phases would never be dispatched"
	);

	if constexpr (run_phases.size() == 1) {
		node.invoke_run_fn = &invoke_annotated_fn<run_phases[0], state_t>;
	}
	else if constexpr (run_phases.size() == 2) {
		node.invoke_run_fn = &invoke_run_phases<state_t, run_phases[0], run_phases[1]>;
	}
	else if constexpr (run_phases.size() == 3) {
		node.invoke_run_fn = &invoke_run_phases<state_t, run_phases[0], run_phases[1], run_phases[2]>;
	}
	else if constexpr (run_phases.size() == 4) {
		node.invoke_run_fn = &invoke_run_phases<state_t, run_phases[0], run_phases[1], run_phases[2], run_phases[3]>;
	}

	node.invoke_ensure_storages_fn = &ensure_node_storages<FnInfos...>;

	node.declared_run_state_deps = std::move(run_meta.state_deps);
	node.optional_run_state_deps = std::move(run_meta.optional_state_deps);
	node.component_reads = std::move(run_meta.component_reads);
	node.component_writes = std::move(run_meta.component_writes);
	node.component_structural = std::move(run_meta.component_structural);
	node.entity_structural = run_meta.entity_structural;
	node.init_state_deps = std::move(init_deps.required);
	node.optional_init_state_deps = std::move(init_deps.optional);
	node.frame_state_deps = std::move(frame_deps);

	std::vector<id> channel_reads;
	std::vector<id> channel_writes;
	(collect_fn_channels<FnInfos>(channel_reads, channel_writes), ...);
	node.channel_consumes = std::move(channel_reads);
	node.channel_publishes = std::move(channel_writes);

	std::vector<id> shared_views;
	(collect_fn_shared_views<FnInfos>(shared_views), ...);
	std::ranges::sort(shared_views, [](const id a, const id b) {
		return a.number() < b.number();
	});
	const auto shared_dup = std::ranges::unique(shared_views);
	shared_views.erase(shared_dup.begin(), shared_dup.end());
	node.shared_view_reads = std::move(shared_views);

	if (!node.invoke_shutdown_fn) {
		node.invoke_shutdown_fn = &noop_shutdown;
	}
	if (!node.invoke_frame_fn) {
		node.invoke_frame_fn = &noop_dispatchers::noop_frame_for<state_t>;
	}
	if (!node.invoke_init_fn) {
		node.init_done = true;
	}
	node.invoke_snapshot_fn = &invoke_annotated_snapshot<state_t>;

	node.state_ptr = &d->state;
	node.state_snapshot_ptr = &d->snapshot;
	node.state_id = id_of<state_t>();
	node.update_wall_id = find_or_generate_id(std::format("update_wall:{}", type_tag<state_t>()));
	node.frame_wall_id = find_or_generate_id(std::format("frame_wall:{}", type_tag<state_t>()));
	node.frame_start_id = find_or_generate_id(std::format("frame_start:{}", type_tag<state_t>()));
	node.trace_id = trace_id<state_t>();
	if constexpr (stateless_traits<state_t>::is_stateless) {
		constexpr auto ns_name = meta::qualified_name_of(stateless_traits<state_t>::ns);
		constexpr auto ns_display = std::define_static_string(std::meta::identifier_of(stateless_traits<state_t>::ns));
		node.system_name = std::string(ns_name);
		node.display_name = std::string(ns_display);
	}
	else {
		node.system_name = std::string(meta::system_qualified_name<state_t>());
		node.display_name = std::string(meta::system_state_name<state_t>());
	}

	constexpr auto def_info = [] consteval {
		if constexpr (stateless_traits<state_t>::is_stateless && sizeof...(FnInfos) > 0) {
			return std::array<std::meta::info, sizeof...(FnInfos)>{ FnInfos... }[0];
		}
		else {
			return StateInfo;
		}
	}();
	constexpr auto state_loc = std::meta::source_location_of(def_info);
	node.def_file = state_loc.file_name();
	node.def_line = state_loc.line();
	node.def_column = state_loc.column();
	node.deferred = meta::is_deferred_system(StateInfo);

	constexpr std::string_view settings_category = settings::category_of<state_t>().empty()
		? meta::system_state_name<state_t>()
		: settings::category_of<state_t>();

	if constexpr (has_describe_fields<state_t>()) {
		node.invoke_apply_settings_fn = &invoke_annotated_apply_settings<state_t>;
		node.settings_record = settings::build_annotated_settings_record<state_t>(d->state, settings_category, page_thunk);
	}
	else if (page_thunk) {
		node.settings_record = settings::build_annotated_settings_record<state_t>(d->state, settings_category, page_thunk);
	}

	return node;
}

template <std::meta::info M, std::meta::info... Rest>
auto gse::settings::resolve_settings_member(auto&& s) -> decltype(auto) {
	if constexpr (sizeof...(Rest) == 0) {
		return (s.[:M:]);
	}
	else {
		return resolve_settings_member<Rest...>(s.[:M:]);
	}
}

template <typename State, std::meta::info... Path>
auto gse::settings::format_annotated_field(const void* settings_ptr) -> std::string {
	constexpr auto leaf = std::array{ Path... }[sizeof...(Path) - 1];
	using F = [:std::meta::type_of(leaf):];
	const auto& field = resolve_settings_member<Path...>(*static_cast<const State*>(settings_ptr));
	if constexpr (is_choice_v<F>) {
		return std::format("{}", field.value);
	}
	else {
		return std::format("{}", field);
	}
}

template <typename State, std::meta::info... Path>
auto gse::settings::annotated_field_options(const void* settings_ptr) -> std::vector<std::string> {
	constexpr auto leaf = std::array{ Path... }[sizeof...(Path) - 1];
	using F = [:std::meta::type_of(leaf):];
	if constexpr (is_choice_v<F>) {
		return resolve_settings_member<Path...>(*static_cast<const State*>(settings_ptr)).options;
	}
	else {
		return {};
	}
}

template <typename State, std::meta::info... Path>
auto gse::settings::push_annotated_field_change(const channel_write<change_request> channels, const std::string_view raw) -> bool {
	constexpr auto leaf = std::array{ Path... }[sizeof...(Path) - 1];
	using F = [:std::meta::type_of(leaf):];
	if constexpr (is_choice_v<F>) {
		typename F::value_type parsed{};
		if (!parse(raw, parsed)) {
			return false;
		}
		channels.push<change_request>({
			.state_type = id_of<State>(),
			.apply = [parsed](void* p) {
				resolve_settings_member<Path...>(*static_cast<State*>(p)).value = parsed;
			},
		});
		return true;
	}
	else {
		F parsed{};
		if (!parse(raw, parsed)) {
			return false;
		}
		channels.push<change_request>({
			.state_type = id_of<State>(),
			.apply = [parsed = std::move(parsed)](void* p) {
				resolve_settings_member<Path...>(*static_cast<State*>(p)) = parsed;
			},
		});
		return true;
	}
}

template <typename State, std::meta::info... Path>
auto gse::settings::make_annotated_field(std::string key) -> settings_field {
	constexpr auto leaf = std::array{ Path... }[sizeof...(Path) - 1];
	using F = [:std::meta::type_of(leaf):];
	using describe_t = [:meta::find_describe(leaf):];
	settings_field field{
		.key = std::move(key),
		.description = std::string(describe_t::value),
		.widget = field_widget_of<F>(),
		.format = &format_annotated_field<State, Path...>,
		.push_change = &push_annotated_field_change<State, Path...>,
		.hot_reloadable = has_annotation<hot_reloadable_tag>(leaf),
		.restart_required = has_annotation<restart_required>(leaf),
	};
	if constexpr (is_choice_v<F>) {
		field.runtime_options = &annotated_field_options<State, Path...>;
		field.choice_stores_option = std::same_as<typename F::value_type, std::string>;
	}
	if constexpr (std::is_enum_v<F>) {
		template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^F))) {
			field.options.emplace_back(std::meta::identifier_of(e));
		}
	}
	if constexpr (is_dimensioned_field<F>) {
		field.units = field_unit_names<F>();
		field.default_unit = field_default_unit<F>();
		field.convert_unit = &convert_field_unit<F>;
		field.normalize = &normalize_field_value<F>;
	}
	if constexpr (constexpr auto range_t = meta::find_range(leaf); range_t != std::meta::info{}) {
		field.range = make_range_field_from_info(range_t);
	}
	return field;
}

template <typename State, typename Cur, gse::settings::scope_kind Inherited, std::meta::info... Prefix>
auto gse::settings::collect_annotated_fields_into(std::vector<settings_field>& out, const std::string_view prefix) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Cur, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			constexpr scope_kind effective = field_scope_of<m, Inherited>();
			std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);
			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				collect_annotated_fields_into<State, F, effective, Prefix..., m>(out, key);
			}
			else if constexpr (effective != scope_kind::app && field_widget_of<F>() != settings_field_widget::unsupported) {
				out.push_back(make_annotated_field<State, Prefix..., m>(std::move(key)));
			}
		}
	}
}

template <typename State>
auto gse::settings::collect_annotated_fields() -> std::vector<settings_field> {
	std::vector<settings_field> out;
	collect_annotated_fields_into<State, State, scope_of<State>()>(
		out,
		{}
	);
	return out;
}

template <typename State>
auto gse::settings::reset_annotated_defaults(const channel_write<change_request> channels) -> void {
	channels.push<change_request>({
		.state_type = id_of<State>(),
		.apply = [](void* p) {
			State& d = *static_cast<State*>(p);
			State defaults{};
			template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^State, std::meta::access_context::unchecked()))) {
				if constexpr (meta::find_describe(m) != std::meta::info{} && field_scope_of<m, scope_of<State>()>() != scope_kind::app) {
					using F = [:std::meta::type_of(m):];
					if constexpr (is_choice_v<F>) {
						d.[:m:].value = defaults.[:m:].value;
					}
					else {
						d.[:m:] = defaults.[:m:];
					}
				}
			}
		},
	});
}

template <typename State>
auto gse::settings::build_annotated_settings_record(State& obj, const std::string_view category, draw_page_thunk page) -> register_settings_type {
	return {
		.category = std::string(category),
		.type_id = id_of<State>(),
		.settings_ptr = &obj,
		.keys = collect_settings_keys<State>(),
		.fields = collect_annotated_fields<State>(),
		.write = &write_settings_for<State>,
		.read = &read_settings_for<State>,
		.reset_to_defaults = &reset_annotated_defaults<State>,
		.draw_page = page,
	};
}

consteval auto gse::is_system_state_entry(const std::meta::info entry) -> bool {
	return meta::find_system_state_anno(entry) != std::meta::info{};
}

consteval auto gse::state_entry_count_for_namespace(const std::meta::info ns, std::vector<std::meta::info> entries) -> std::size_t {
	std::size_t count = 0;
	for (const auto e : entries) {
		if (is_system_state_entry(e) && std::meta::parent_of(e) == ns) {
			++count;
		}
	}
	return count;
}

consteval auto gse::hook_fns_in_namespace(const std::meta::info ns, std::vector<std::meta::info> entries) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> out;
	for (const auto e : entries) {
		if (meta::find_system_hook_anno(e) != std::meta::info{} && std::meta::parent_of(e) == ns) {
			out.push_back(e);
		}
	}
	return out;
}

consteval auto gse::system_namespaces(std::vector<std::meta::info> entries) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> out;
	for (const auto e : entries) {
		const bool is_system = is_system_state_entry(e) || meta::find_system_hook_anno(e) != std::meta::info{};
		if (is_system) {
			const auto ns = std::meta::parent_of(e);
			if (std::ranges::find(out, ns) == out.end()) {
				out.push_back(ns);
			}
		}
	}
	return out;
}

consteval auto gse::state_entry_for_namespace(const std::meta::info ns, std::vector<std::meta::info> entries) -> std::meta::info {
	for (const auto e : entries) {
		if (is_system_state_entry(e) && std::meta::parent_of(e) == ns) {
			return e;
		}
	}
	return std::meta::info{};
}

consteval auto gse::page_fn_for_state(const std::meta::info state, std::vector<std::meta::info> entries) -> std::meta::info {
	for (const auto e : entries) {
		const auto anno = meta::find_page_for_anno(e);
		if (anno != std::meta::info{} && meta::state_type_of_page_for(anno) == state) {
			return e;
		}
	}
	return std::meta::info{};
}

template <std::meta::info PageInfo>
auto gse::page_thunk_for(void* builder, void* panel_state, const settings::change_request_writer channels, const void* entry) -> void {
	using builder_t = [:std::meta::remove_cvref(std::meta::type_of(std::meta::parameters_of(PageInfo)[0])):];
	using panel_state_t = [:std::meta::remove_cvref(std::meta::type_of(std::meta::parameters_of(PageInfo)[1])):];
	using entry_t = [:std::meta::remove_cvref(std::meta::type_of(std::meta::parameters_of(PageInfo)[2])):];
	[:PageInfo:](
		*static_cast<builder_t*>(builder),
		*static_cast<panel_state_t*>(panel_state),
		*static_cast<const entry_t*>(entry),
		channels
	);
}

consteval auto gse::collect_namespace_systems(const std::meta::info ns) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> out;
	std::vector<std::meta::info> pending;
	pending.push_back(ns);
	while (!pending.empty()) {
		const auto current = pending.back();
		pending.pop_back();
		for (const auto m : std::meta::members_of(current, std::meta::access_context::unchecked())) {
			if (std::meta::is_type(m) && meta::find_system_state_anno(m) != std::meta::info{}) {
				out.push_back(m);
			}
			else if (std::meta::is_function(m) && meta::find_system_hook_anno(m) != std::meta::info{}) {
				out.push_back(m);
			}
			else if (std::meta::is_namespace(m)) {
				pending.push_back(m);
			}
		}
	}
	return out;
}

template <std::meta::info... Namespaces>
consteval auto gse::collect_all_namespace_systems() -> std::vector<std::meta::info> {
	std::vector<std::meta::info> out;
	(([&] {
		const auto entries = collect_namespace_systems(Namespaces);
		out.insert(out.end(), entries.begin(), entries.end());
	}()), ...);
	return out;
}

template <std::meta::info... Namespaces, gse::system_node_registrar R>
auto gse::register_systems(R& registrar) -> void {
	using entries = namespace_system_entries<Namespaces...>;
	[&]<std::size_t... I>(std::index_sequence<I...>) {
		system_manifest<entries::value[I]...>{}.register_with(registrar);
	}(std::make_index_sequence<entries::value.size()>{});
}

template <std::meta::info... Entries>
template <gse::system_node_registrar R>
auto gse::system_manifest<Entries...>::register_with(R& registrar) const -> void {
	template for (constexpr auto sys_ns : std::define_static_array(system_namespaces(std::vector<std::meta::info>{ Entries... }))) {
		constexpr auto state_entry = state_entry_for_namespace(sys_ns, std::vector<std::meta::info>{ Entries... });
		constexpr auto fns = std::define_static_array(hook_fns_in_namespace(sys_ns, std::vector<std::meta::info>{ Entries... }));
		constexpr auto state_info = state_entry != std::meta::info{} ? state_entry : ^^stateless_system<sys_ns>;

		static_assert(
			state_entry_count_for_namespace(sys_ns, std::vector<std::meta::info>{ Entries... }) <= 1,
			"a system namespace declares more than one [[= gse::system_state]] struct; every hook function in the namespace would be attributed to the first one and the rest would silently never run"
		);
		static_assert(
			fns.size() <= 6,
			"a system namespace declares more than six annotated hook functions; extend the make_annotated_system_node chain in register_with, otherwise the extra hooks would never be wired"
		);

		settings::draw_page_thunk page = nullptr;
		if constexpr (state_entry != std::meta::info{}) {
			constexpr auto page_info = page_fn_for_state(state_entry, std::vector<std::meta::info>{ Entries... });
			if constexpr (page_info != std::meta::info{}) {
				page = &page_thunk_for<page_info>;
			}
		}

		if constexpr (fns.size() == 0) {
			registrar.add_system_node(make_annotated_system_node<state_info>(page));
		}
		else if constexpr (fns.size() == 1) {
			registrar.add_system_node(make_annotated_system_node<state_info, fns[0]>(page));
		}
		else if constexpr (fns.size() == 2) {
			registrar.add_system_node(make_annotated_system_node<state_info, fns[0], fns[1]>(page));
		}
		else if constexpr (fns.size() == 3) {
			registrar.add_system_node(make_annotated_system_node<state_info, fns[0], fns[1], fns[2]>(page));
		}
		else if constexpr (fns.size() == 4) {
			registrar.add_system_node(make_annotated_system_node<state_info, fns[0], fns[1], fns[2], fns[3]>(page));
		}
		else if constexpr (fns.size() == 5) {
			registrar.add_system_node(make_annotated_system_node<state_info, fns[0], fns[1], fns[2], fns[3], fns[4]>(page));
		}
		else {
			registrar.add_system_node(make_annotated_system_node<state_info, fns[0], fns[1], fns[2], fns[3], fns[4], fns[5]>(page));
		}
	}
}