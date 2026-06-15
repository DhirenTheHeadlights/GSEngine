export module gse.gpu:device_dispatch;

import std;

export namespace gse::gpu {
	template <typename R, typename... A>
	using device_thunk_ptr = R (*)(void*, A...);

	template <typename B, auto M, typename R, typename... A>
	struct device_thunk {
		static constexpr device_thunk_ptr<R, A...> value = [](void* self, A... args) -> R {
			return static_cast<B*>(self)->[:M:](static_cast<A&&>(args)...);
		};
	};

	template <typename B>
	auto device_backend_delete(
		void* self
	) -> void;

	consteval auto device_op_members(
		std::meta::info backend
	) -> std::vector<std::meta::info>;

	consteval auto device_thunk_ptr_type(
		std::meta::info m
	) -> std::meta::info;

	consteval auto device_thunk_value_type(
		std::meta::info backend,
		std::meta::info m
	) -> std::meta::info;

	consteval auto device_dispatch_specs(
		std::meta::info backend
	) -> std::vector<std::meta::info>;
}

template <typename B>
auto gse::gpu::device_backend_delete(void* self) -> void {
	delete static_cast<B*>(self);
}

consteval auto gse::gpu::device_op_members(std::meta::info backend) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> ops;
	for (auto m : std::meta::members_of(backend, std::meta::access_context::unchecked())) {
		if (std::meta::is_function(m)
			&& !std::meta::is_static_member(m)
			&& !std::meta::is_special_member_function(m)
			&& std::meta::has_identifier(m)) {
			ops.push_back(m);
		}
	}
	return ops;
}

consteval auto gse::gpu::device_thunk_ptr_type(std::meta::info m) -> std::meta::info {
	std::vector<std::meta::info> args;
	args.push_back(std::meta::return_type_of(m));
	for (auto p : std::meta::parameters_of(m)) {
		args.push_back(std::meta::type_of(p));
	}
	return std::meta::substitute(^^device_thunk_ptr, args);
}

consteval auto gse::gpu::device_thunk_value_type(std::meta::info backend, std::meta::info m) -> std::meta::info {
	std::vector<std::meta::info> args;
	args.push_back(backend);
	args.push_back(std::meta::reflect_constant(m));
	args.push_back(std::meta::return_type_of(m));
	for (auto p : std::meta::parameters_of(m)) {
		args.push_back(std::meta::type_of(p));
	}
	return std::meta::substitute(^^device_thunk, args);
}

consteval auto gse::gpu::device_dispatch_specs(std::meta::info backend) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> specs;
	for (auto m : device_op_members(backend)) {
		specs.push_back(std::meta::data_member_spec(
			device_thunk_ptr_type(m),
			{ .name = std::meta::identifier_of(m) }
		));
	}
	return specs;
}
