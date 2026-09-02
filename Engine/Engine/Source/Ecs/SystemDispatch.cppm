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
import :system_node;
import :shared_view;
import :system_anno;

export namespace gse {
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
		else if constexpr (is_entities_v<U>) {
			return false;
		}
		else if constexpr (is_channel_read_v<U>) {
			return false;
		}
		else if constexpr (is_channel_write_v<U>) {
			return false;
		}
		else {
			return meta::has_system_state<U>();
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

	template <typename Arg, typename S>
	constexpr bool is_external_resource_arg_v = [] consteval {
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
		else if constexpr (is_entities_v<U>) {
			return false;
		}
		else if constexpr (is_channel_read_v<U>) {
			return false;
		}
		else if constexpr (is_channel_write_v<U>) {
			return false;
		}
		else {
			return !meta::has_system_state<U>() && !is_system_data_v<U>;
		}
	}();

	template <typename Arg>
	constexpr bool is_read_resource_arg_v = [] consteval {
		using Bare = std::remove_cvref_t<Arg>;
		if constexpr (std::is_pointer_v<Bare>) {
			return std::is_const_v<std::remove_pointer_t<Bare>>;
		}
		else {
			return std::is_const_v<std::remove_reference_t<Arg>>;
		}
	}();

	template <auto MemberFn>
	constexpr std::size_t arity_of = std::meta::parameters_of(MemberFn).size();

	template <auto MemberFn, std::size_t I>
	using arg_type_of = typename[:std::meta::type_of(std::meta::parameters_of(MemberFn)[I]):];

	struct run_signature_metadata {
		std::vector<id> state_deps;
		std::vector<id> optional_state_deps;
		std::vector<id> component_reads;
		std::vector<id> component_writes;
		std::vector<id> component_structural;
		std::vector<id> resource_reads;
		std::vector<id> resource_writes;
		bool entity_structural = false;
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

	template <typename Arg>
	auto append_arg_shared_view(
		std::vector<id>& shared_out
	) -> void;

	template <typename Arg>
	auto append_arg_channel_ids(
		std::vector<id>& reads_out,
		std::vector<id>& writes_out
	) -> void;

	template <typename Arg, typename S>
	auto append_arg_resource_access(
		run_signature_metadata& out
	) -> void;

	template <typename Arg, typename S>
	auto append_arg_run_metadata(
		run_signature_metadata& out
	) -> void;

	template <auto Fn>
	auto append_fn_order_deps(
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

	template <typename T>
	auto mutable_external_resource_ref(
		task_context& ctx
	) -> T&;

	template <typename Arg, typename S>
	auto resolve_run_arg(
		context& ctx,
		state_of_t<S>& state
	) -> decltype(auto);

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

	template <typename T>
	constexpr id state_dep_id_v = id_of<dep_pointee_t<T>>();

}

template <typename T>
auto gse::external_resource_ref(const task_context& ctx) -> const T& {
	const void* p = ctx.resources_store.resources_ptr(id_of<T>());
	assert(p != nullptr, "external resource not found; register it with register_external_resource");
	return *static_cast<const T*>(p);
}

template <typename T>
auto gse::mutable_external_resource_ref(task_context& ctx) -> T& {
	void* p = ctx.resources_store.resources_ptr(id_of<T>());
	assert(p != nullptr, "external resource not found; register it with register_external_resource");
	return *static_cast<T*>(p);
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
	else if constexpr (is_entities_v<U>) {
		return ctx.make_entities();
	}
	else if constexpr (is_channel_read_v<U>) {
		return U(ctx.channels_store);
	}
	else if constexpr (is_channel_write_v<U>) {
		return U(ctx.channels);
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
		if constexpr (is_read_resource_arg_v<Arg>) {
			return static_cast<const Pointee*>(ctx.resources_store.resources_ptr(id_of<Pointee>()));
		}
		else {
			return static_cast<Pointee*>(ctx.resources_store.resources_ptr(id_of<Pointee>()));
		}
	}
	else {
		static_assert(
			!is_system_data_v<U>,
			"cross-system state is private; use shared_view<X> ([[= gse::shared]] fields) or channels"
		);
		static_assert(
			std::is_reference_v<Arg>,
			"external resources must be taken by reference: const T& to read it, T& to write it"
		);
		if constexpr (is_read_resource_arg_v<Arg>) {
			return external_resource_ref<U>(ctx);
		}
		else {
			return mutable_external_resource_ref<U>(ctx);
		}
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

template <typename Arg>
auto gse::append_arg_shared_view(std::vector<id>& shared_out) -> void {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (is_shared_view_v<U>) {
		shared_out.push_back(id_of<shared_view_target_t<U>>());
	}
	else if constexpr (is_optional_shared_view_v<U>) {
		shared_out.push_back(id_of<optional_shared_view_target_t<U>>());
	}
}

template <typename Arg>
auto gse::append_arg_channel_ids(std::vector<id>& reads_out, std::vector<id>& writes_out) -> void {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (is_channel_read_v<U>) {
		channel_pack_ids<U>::append(reads_out);
	}
	else if constexpr (is_channel_write_v<U>) {
		channel_pack_ids<U>::append(writes_out);
	}
}

template <typename Arg, typename S>
auto gse::append_arg_resource_access(run_signature_metadata& out) -> void {
	if constexpr (is_external_resource_arg_v<Arg, S>) {
		using U = dep_pointee_t<Arg>;
		(void)trace_id<U>();
		if constexpr (is_read_resource_arg_v<Arg>) {
			out.resource_reads.push_back(id_of<U>());
		}
		else {
			out.resource_writes.push_back(id_of<U>());
		}
	}
}

template <typename Arg, typename S>
auto gse::append_arg_run_metadata(run_signature_metadata& out) -> void {
	append_arg_state_dep<Arg, S>(out.state_deps);
	append_arg_view_deps<Arg>(out.state_deps, out.optional_state_deps);
	append_arg_resource_access<Arg, S>(out);

	using ArgT = std::remove_cvref_t<Arg>;
	if constexpr (is_access_v<ArgT> && is_read_access_v<ArgT>) {
		(void)trace_id<access_element_t<ArgT>>();
		out.component_reads.push_back(id_of<access_element_t<ArgT>>());
	}

	if constexpr (is_access_v<ArgT> && !is_read_access_v<ArgT>) {
		(void)trace_id<access_element_t<ArgT>>();
		out.component_writes.push_back(id_of<access_element_t<ArgT>>());
	}
	else if constexpr (is_structural_v<ArgT>) {
		(void)trace_id<structural_element_t<ArgT>>();
		out.component_structural.push_back(id_of<structural_element_t<ArgT>>());
	}

	if constexpr (is_entities_v<ArgT>) {
		out.entity_structural = true;
	}
}

template <auto Fn>
auto gse::append_fn_order_deps(run_signature_metadata& out) -> void {
	template for (constexpr auto s : std::define_static_array(meta::required_order_deps_of(Fn))) {
		out.state_deps.push_back(id_of<typename [:s:]>());
	}
	template for (constexpr auto s : std::define_static_array(meta::optional_order_deps_of(Fn))) {
		out.optional_state_deps.push_back(id_of<typename [:s:]>());
	}
}

template <auto Fn, typename S>
auto gse::append_signature_run_metadata(run_signature_metadata& out) -> void {
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		((append_arg_run_metadata<arg_type_of<Fn, Is>, S>(out)), ...);
	}(std::make_index_sequence<arity_of<Fn>>{});
	append_fn_order_deps<Fn>(out);
}