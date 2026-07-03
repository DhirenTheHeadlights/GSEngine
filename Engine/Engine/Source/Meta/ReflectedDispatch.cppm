export module gse.meta:reflected_dispatch;

import std;

export namespace gse::meta {
	template <typename Self, typename R, typename... A>
	using dispatch_fn_ptr = R (*)(Self, A...);

	template <typename Recv, auto M, typename R, typename... A>
	struct dispatch_thunk {
		static constexpr dispatch_fn_ptr<typename Recv::arg_type, R, A...> value = [](typename Recv::arg_type self, A... args) -> R {
			return Recv::obj(self).[:M:](static_cast<A&&>(args)...);
		};
	};

	template <typename B>
	struct pointer_receiver {
		using arg_type = void*;

		static consteval auto self_type() -> std::meta::info;

		static consteval auto backend_type() -> std::meta::info;

		static auto obj(
			void* self
		) -> B&;
	};

	template <typename B, typename Handle>
	struct value_receiver {
		using arg_type = Handle;

		static consteval auto self_type() -> std::meta::info;

		static consteval auto backend_type() -> std::meta::info;

		static auto obj(
			Handle handle
		) -> B;
	};

	constexpr std::array<std::string_view, 0> dispatch_no_exclude{};

	consteval auto dispatch_ops(
		std::meta::info contract,
		std::span<const std::string_view> exclude
	) -> std::vector<std::meta::info>;

	consteval auto dispatch_fn_member(
		std::meta::info type,
		std::string_view name
	) -> std::meta::info;

	consteval auto dispatch_slot_member(
		std::meta::info table,
		std::string_view name
	) -> std::meta::info;

	consteval auto dispatch_fn_ptr_type(
		std::meta::info self_type,
		std::meta::info contract_member
	) -> std::meta::info;

	consteval auto dispatch_value_type(
		std::meta::info recv,
		std::meta::info backend_member,
		std::meta::info contract_member
	) -> std::meta::info;

	consteval auto dispatch_specs(
		std::meta::info self_type,
		std::meta::info contract,
		std::span<const std::string_view> exclude
	) -> std::vector<std::meta::info>;

	template <typename Table, typename Contract, typename Recv, const auto& Exclude = dispatch_no_exclude>
	consteval auto build_dispatch() -> Table;
}

template <typename B>
consteval auto gse::meta::pointer_receiver<B>::self_type() -> std::meta::info {
	return std::meta::dealias(^^arg_type);
}

template <typename B>
consteval auto gse::meta::pointer_receiver<B>::backend_type() -> std::meta::info {
	return ^^B;
}

template <typename B>
auto gse::meta::pointer_receiver<B>::obj(void* self) -> B& {
	return *static_cast<B*>(self);
}

template <typename B, typename Handle>
consteval auto gse::meta::value_receiver<B, Handle>::self_type() -> std::meta::info {
	return std::meta::dealias(^^arg_type);
}

template <typename B, typename Handle>
consteval auto gse::meta::value_receiver<B, Handle>::backend_type() -> std::meta::info {
	return ^^B;
}

template <typename B, typename Handle>
auto gse::meta::value_receiver<B, Handle>::obj(Handle handle) -> B {
	return B(handle);
}

consteval auto gse::meta::dispatch_ops(std::meta::info contract, std::span<const std::string_view> exclude) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> ops;
	for (auto m : std::meta::members_of(contract, std::meta::access_context::unchecked())) {
		if (std::meta::is_function(m)
			&& !std::meta::is_static_member(m)
			&& !std::meta::is_special_member_function(m)
			&& std::meta::has_identifier(m)
			&& std::ranges::find(exclude, std::meta::identifier_of(m)) == exclude.end()) {
			ops.push_back(m);
		}
	}
	return ops;
}

consteval auto gse::meta::dispatch_fn_member(std::meta::info type, std::string_view name) -> std::meta::info {
	for (auto m : std::meta::members_of(type, std::meta::access_context::unchecked())) {
		if (std::meta::is_function(m)
			&& !std::meta::is_static_member(m)
			&& !std::meta::is_special_member_function(m)
			&& std::meta::has_identifier(m)
			&& std::meta::identifier_of(m) == name) {
			return m;
		}
	}
	return std::meta::info{};
}

consteval auto gse::meta::dispatch_slot_member(std::meta::info table, std::string_view name) -> std::meta::info {
	for (auto member : std::meta::nonstatic_data_members_of(table, std::meta::access_context::unchecked())) {
		if (std::meta::identifier_of(member) == name) {
			return member;
		}
	}
	return std::meta::info{};
}

consteval auto gse::meta::dispatch_fn_ptr_type(std::meta::info self_type, std::meta::info contract_member) -> std::meta::info {
	std::vector<std::meta::info> args;
	args.push_back(self_type);
	args.push_back(std::meta::return_type_of(contract_member));
	for (auto p : std::meta::parameters_of(contract_member)) {
		args.push_back(std::meta::type_of(p));
	}
	return std::meta::substitute(^^dispatch_fn_ptr, args);
}

consteval auto gse::meta::dispatch_value_type(std::meta::info recv, std::meta::info backend_member, std::meta::info contract_member) -> std::meta::info {
	std::vector<std::meta::info> args;
	args.push_back(recv);
	args.push_back(std::meta::reflect_constant(backend_member));
	args.push_back(std::meta::return_type_of(contract_member));
	for (auto p : std::meta::parameters_of(contract_member)) {
		args.push_back(std::meta::type_of(p));
	}
	return std::meta::substitute(^^dispatch_thunk, args);
}

consteval auto gse::meta::dispatch_specs(std::meta::info self_type, std::meta::info contract, std::span<const std::string_view> exclude) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> specs;
	for (auto m : dispatch_ops(contract, exclude)) {
		specs.push_back(std::meta::data_member_spec(
			dispatch_fn_ptr_type(self_type, m),
			{ .name = std::meta::identifier_of(m) }
		));
	}
	return specs;
}

template <typename Table, typename Contract, typename Recv, const auto& Exclude>
consteval auto gse::meta::build_dispatch() -> Table {
	Table table{};
	template for (constexpr auto op : std::define_static_array(dispatch_ops(^^Contract, Exclude))) {
		table.[: dispatch_slot_member(^^Table, std::meta::identifier_of(op)) :]
			= [: dispatch_value_type(^^Recv, dispatch_fn_member(Recv::backend_type(), std::meta::identifier_of(op)), op) :]::value;
	}
	return table;
}
