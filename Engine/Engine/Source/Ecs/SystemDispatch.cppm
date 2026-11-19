export module gse.ecs:system_dispatch;

import gse.concurrency;
import gse.core;
import gse.diag;
import std;

import :access;
import :context;
import :registries;
import :registry;
import :settings;
import :shared_view;
import :system_anno;
import :traits;

export namespace gse {
	template <typename T>
	using dep_pointee_t = std::remove_cv_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

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

	enum class arg_kind : std::uint8_t {
		context,
		state,
		access,
		entities,
		channel_read,
		channel_write,
		shared_view,
		optional_shared_view,
		resource
	};

	struct hook_signature {
		std::vector<id> views;
		std::vector<id> optional_views;
		std::vector<id> order;
		std::vector<id> optional_order;
		std::vector<id> component_reads;
		std::vector<id> component_writes;
		std::vector<id> resource_reads;
		std::vector<id> resource_writes;
		std::vector<id> channel_reads;
		std::vector<id> channel_writes;
		std::vector<id> channel_lends;
		std::vector<void (registry::*)()> ensure_storage_fns;
		bool entity_structural = false;
	};

	template <typename Arg, typename S>
	consteval auto arg_kind_of() -> arg_kind;

	template <typename Arg, typename S>
	auto append_arg_signature(
		hook_signature& out
	) -> void;

	template <auto Fn, typename S>
	auto hook_signature_of() -> hook_signature;

	template <typename Arg, typename S>
	auto resolve_run_arg(
		context& ctx,
		S& state
	) -> decltype(auto);

	template <typename Data>
	auto live_system_state(
		context& ctx
	) -> Data*;
}

template <typename Data>
auto gse::live_system_state(context& ctx) -> Data* {
	return static_cast<Data*>(ctx.m_states.state_ptr(id_of<Data>()));
}

template <typename Arg, typename S>
consteval auto gse::arg_kind_of() -> arg_kind {
	using U = std::remove_cvref_t<Arg>;
	if constexpr (std::is_same_v<U, context>) {
		return arg_kind::context;
	}
	else if constexpr (std::is_same_v<U, S>) {
		return arg_kind::state;
	}
	else if constexpr (is_access_v<U>) {
		return arg_kind::access;
	}
	else if constexpr (is_entities_v<U>) {
		return arg_kind::entities;
	}
	else if constexpr (is_channel_read_v<U>) {
		return arg_kind::channel_read;
	}
	else if constexpr (is_channel_write_v<U>) {
		return arg_kind::channel_write;
	}
	else if constexpr (is_shared_view_v<U>) {
		return arg_kind::shared_view;
	}
	else if constexpr (is_optional_shared_view_v<U>) {
		return arg_kind::optional_shared_view;
	}
	else {
		static_assert(
			!is_system_data_v<dep_pointee_t<Arg>> && !meta::has_system_state<dep_pointee_t<Arg>>(),
			"cross-system state is private; use shared_view<X> ([[= gse::shared]] fields) or channels"
		);
		static_assert(
			std::is_pointer_v<U> || std::is_reference_v<Arg>,
			"external resources must be taken by reference: const T& to read it, T& to write it"
		);
		return arg_kind::resource;
	}
}

template <typename Arg, typename S>
auto gse::append_arg_signature(hook_signature& out) -> void {
	using U = std::remove_cvref_t<Arg>;
	constexpr arg_kind kind = arg_kind_of<Arg, S>();
	if constexpr (kind == arg_kind::access) {
		using T = access_element_t<U>;
		(void)trace_id<T>();
		out.ensure_storage_fns.push_back(&registry::ensure_storage<T>);
		if constexpr (is_read_access_v<U>) {
			out.component_reads.push_back(id_of<T>());
		}
		else {
			static_assert(
				meta::may_write(^^access_element_t<U>, ^^S),
				std::string(meta::qualified_name_of(std::meta::dealias(^^S))) + " declares write<" + std::string(meta::qualified_name_of(std::meta::dealias(^^access_element_t<U>))) + ">, which is written_by another namespace; send that namespace a request instead"
			);
			out.component_writes.push_back(id_of<T>());
		}
	}
	else if constexpr (kind == arg_kind::entities) {
		out.entity_structural = true;
	}
	else if constexpr (kind == arg_kind::channel_read) {
		channel_pack_ids<U>::append(out.channel_reads);
	}
	else if constexpr (kind == arg_kind::channel_write) {
		channel_pack_ids<U>::append(out.channel_writes);
		channel_pack_ids<U>::append_lending(out.channel_lends);
	}
	else if constexpr (kind == arg_kind::shared_view) {
		out.views.push_back(id_of<shared_view_target_t<U>>());
	}
	else if constexpr (kind == arg_kind::optional_shared_view) {
		out.optional_views.push_back(id_of<optional_shared_view_target_t<U>>());
	}
	else if constexpr (kind == arg_kind::resource) {
		using Pointee = dep_pointee_t<Arg>;
		(void)trace_id<Pointee>();
		if constexpr (is_read_resource_arg_v<Arg>) {
			out.resource_reads.push_back(id_of<Pointee>());
		}
		else {
			out.resource_writes.push_back(id_of<Pointee>());
		}
	}
}

template <auto Fn, typename S>
auto gse::hook_signature_of() -> hook_signature {
	hook_signature out;
	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		(append_arg_signature<arg_type_of<Fn, Is>, S>(out), ...);
	}(std::make_index_sequence<arity_of<Fn>>{});
	template for (constexpr auto s : std::define_static_array(meta::required_order_deps_of(Fn))) {
		out.order.push_back(id_of<typename [:s:]>());
	}
	template for (constexpr auto s : std::define_static_array(meta::optional_order_deps_of(Fn))) {
		out.optional_order.push_back(id_of<typename [:s:]>());
	}
	return out;
}

template <typename Arg, typename S>
auto gse::resolve_run_arg(context& ctx, S& state) -> decltype(auto) {
	using U = std::remove_cvref_t<Arg>;
	constexpr arg_kind kind = arg_kind_of<Arg, S>();
	if constexpr (kind == arg_kind::context) {
		return (ctx);
	}
	else if constexpr (kind == arg_kind::state) {
		return (state);
	}
	else if constexpr (kind == arg_kind::access) {
		return ctx.template make_access<U>();
	}
	else if constexpr (kind == arg_kind::entities) {
		return ctx.make_entities();
	}
	else if constexpr (kind == arg_kind::channel_read) {
		return U(ctx.m_channels);
	}
	else if constexpr (kind == arg_kind::channel_write) {
		return U(ctx.m_channels, id_of<S>());
	}
	else if constexpr (kind == arg_kind::shared_view) {
		using Target = shared_view_target_t<U>;
		constexpr id lookup_id = id_of<Target>();
		if (ctx.m_hook != system_hook::frame) {
			const void* p = ctx.m_states.state_ptr(lookup_id);
			assert(p != nullptr, "shared_view target system not registered");
			return make_shared_view_live<Target>(*static_cast<const Target*>(p));
		}
		const void* sp = ctx.m_states.state_snapshot_ptr(lookup_id);
		assert(sp != nullptr, "shared_view target system not registered");
		return make_shared_view_snapshot<Target>(*static_cast<const shared_snapshot<Target>*>(sp));
	}
	else if constexpr (kind == arg_kind::optional_shared_view) {
		using Target = optional_shared_view_target_t<U>;
		constexpr id lookup_id = id_of<Target>();
		if (ctx.m_hook != system_hook::frame) {
			const void* p = ctx.m_states.state_ptr(lookup_id);
			if (!p) {
				return std::optional<shared_view<Target>>{};
			}
			return std::optional<shared_view<Target>>{ make_shared_view_live<Target>(*static_cast<const Target*>(p)) };
		}
		const void* sp = ctx.m_states.state_snapshot_ptr(lookup_id);
		if (!sp) {
			return std::optional<shared_view<Target>>{};
		}
		return std::optional<shared_view<Target>>{ make_shared_view_snapshot<Target>(*static_cast<const shared_snapshot<Target>*>(sp)) };
	}
	else if constexpr (std::is_pointer_v<U>) {
		using Pointee = dep_pointee_t<Arg>;
		if constexpr (is_read_resource_arg_v<Arg>) {
			return static_cast<const Pointee*>(ctx.m_resources.resources_ptr(id_of<Pointee>()));
		}
		else {
			return static_cast<Pointee*>(ctx.m_resources.resources_ptr(id_of<Pointee>()));
		}
	}
	else {
		auto* resource = static_cast<U*>(ctx.m_resources.resources_ptr(id_of<U>()));
		assert(resource != nullptr, "external resource not found; register it with register_external_resource");
		if constexpr (is_read_resource_arg_v<Arg>) {
			return static_cast<const U&>(*resource);
		}
		else {
			return (*resource);
		}
	}
}