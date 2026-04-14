export module gse.utility:system_node;

import std;

import :phase_context;
import :update_context;
import :frame_context;
import :async_task;
import :id;

export namespace gse {
	class system_node_base {
	public:
		virtual ~system_node_base() = default;

		virtual auto initialize(
			init_context&
		) -> void = 0;

		virtual auto shutdown(
			shutdown_context&
		) -> void = 0;

		virtual auto graph_update(
			update_context&
		) -> void = 0;

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

	template <typename State, bool = std::is_trivially_copyable_v<State>>
	struct snapshot_storage {};

	template <typename State>
	struct snapshot_storage<State, true> {
		State value;
	};

	template <typename S, typename State>
	class system_node final : public system_node_base, non_copyable {
	public:
		~system_node() override = default;

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
		) -> void override;

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
	) -> void;

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

template <typename S, typename State>
auto gse::dispatch_initialize(init_context& phase, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, State& state) -> void {
	if constexpr (has_resources<S> && has_update_data<S> && has_frame_data<S>) {
		if constexpr (requires { S::initialize(phase, resources.value, update_data.value, frame_data.value, state); }) {
			S::initialize(phase, resources.value, update_data.value, frame_data.value, state);
			return;
		}
	}
	if constexpr (has_resources<S> && has_frame_data<S>) {
		if constexpr (requires { S::initialize(phase, resources.value, frame_data.value, state); }) {
			S::initialize(phase, resources.value, frame_data.value, state);
			return;
		}
	}
	if constexpr (has_resources<S> && has_update_data<S>) {
		if constexpr (requires { S::initialize(phase, resources.value, update_data.value, state); }) {
			S::initialize(phase, resources.value, update_data.value, state);
			return;
		}
	}
	if constexpr (has_resources<S>) {
		if constexpr (requires { S::initialize(phase, resources.value, state); }) {
			S::initialize(phase, resources.value, state);
			return;
		}
	}
	if constexpr (!has_resources<S> && has_update_data<S> && has_frame_data<S>) {
		if constexpr (requires { S::initialize(phase, update_data.value, frame_data.value, state); }) {
			S::initialize(phase, update_data.value, frame_data.value, state);
			return;
		}
	}
	if constexpr (!has_resources<S> && has_frame_data<S>) {
		if constexpr (requires { S::initialize(phase, frame_data.value, state); }) {
			S::initialize(phase, frame_data.value, state);
			return;
		}
	}
	if constexpr (!has_resources<S> && has_update_data<S>) {
		if constexpr (requires { S::initialize(phase, update_data.value, state); }) {
			S::initialize(phase, update_data.value, state);
			return;
		}
	}
	if constexpr (has_initialize<S, State>) {
		S::initialize(phase, state);
	}
}

template <typename S, typename State>
auto gse::dispatch_update(update_context& ctx, resource_storage<S>& resources, update_data_storage<S>& update_data, frame_data_storage<S>& frame_data, State& state) -> void {
	if constexpr (has_resources<S> && has_update_data<S> && has_frame_data<S>) {
		if constexpr (requires { S::update(ctx, std::as_const(resources.value), update_data.value, frame_data.value, state); }) {
			S::update(ctx, std::as_const(resources.value), update_data.value, frame_data.value, state);
			return;
		}
	}
	if constexpr (has_resources<S> && has_frame_data<S>) {
		if constexpr (requires { S::update(ctx, std::as_const(resources.value), frame_data.value, state); }) {
			S::update(ctx, std::as_const(resources.value), frame_data.value, state);
			return;
		}
	}
	if constexpr (has_resources<S> && has_update_data<S>) {
		if constexpr (requires { S::update(ctx, std::as_const(resources.value), update_data.value, state); }) {
			S::update(ctx, std::as_const(resources.value), update_data.value, state);
			return;
		}
	}
	if constexpr (has_resources<S>) {
		if constexpr (requires { S::update(ctx, std::as_const(resources.value), state); }) {
			S::update(ctx, std::as_const(resources.value), state);
			return;
		}
	}
	if constexpr (!has_resources<S> && has_update_data<S> && has_frame_data<S>) {
		if constexpr (requires { S::update(ctx, update_data.value, frame_data.value, state); }) {
			S::update(ctx, update_data.value, frame_data.value, state);
			return;
		}
	}
	if constexpr (!has_resources<S> && has_frame_data<S>) {
		if constexpr (requires { S::update(ctx, frame_data.value, state); }) {
			S::update(ctx, frame_data.value, state);
			return;
		}
	}
	if constexpr (has_update_data<S>) {
		if constexpr (requires { S::update(ctx, update_data.value, state); }) {
			S::update(ctx, update_data.value, state);
			return;
		}
	}
	if constexpr (requires { S::update(ctx, state); }) {
		S::update(ctx, state);
	}
}

template <typename S, typename State>
auto gse::dispatch_frame(frame_context& ctx, resource_storage<S>& resources, frame_data_storage<S>& frame_data, const State& state) -> async::task<> {
	if constexpr (has_resources<S> && has_frame_data<S>) {
		if constexpr (requires { S::frame(ctx, std::as_const(resources.value), frame_data.value, state); }) {
			co_await S::frame(ctx, std::as_const(resources.value), frame_data.value, state);
			co_return;
		}
	}
	if constexpr (has_resources<S>) {
		if constexpr (requires { S::frame(ctx, std::as_const(resources.value), state); }) {
			co_await S::frame(ctx, std::as_const(resources.value), state);
			co_return;
		}
	}
	if constexpr (has_frame_data<S>) {
		if constexpr (requires { S::frame(ctx, frame_data.value, state); }) {
			co_await S::frame(ctx, frame_data.value, state);
			co_return;
		}
	}
	if constexpr (requires { S::frame(ctx, state); }) {
		co_await S::frame(ctx, state);
	}
	co_return;
}

template <typename S, typename State>
auto gse::dispatch_has_frame() -> bool {
	if constexpr (has_resources<S> && has_frame_data<S>) {
		if constexpr (requires(frame_context& c, const typename S::resources& r, typename S::frame_data& fd, const State& s) { S::frame(c, r, fd, s); }) {
			return true;
		}
	}
	if constexpr (has_resources<S>) {
		if constexpr (requires(frame_context& c, const typename S::resources& r, const State& s) { S::frame(c, r, s); }) {
			return true;
		}
	}
	if constexpr (has_frame_data<S>) {
		if constexpr (requires(frame_context& c, typename S::frame_data& fd, const State& s) { S::frame(c, fd, s); }) {
			return true;
		}
	}
	if constexpr (requires(frame_context& c, const State& s) { S::frame(c, s); }) {
		return true;
	}
	return false;
}

template <typename S, typename State>
template <typename... Args>
gse::system_node<S, State>::system_node(Args&&... args)
	: m_state(std::forward<Args>(args)...) {}

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
	if constexpr (has_shutdown<S, State>) {
		S::shutdown(phase, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::graph_update(update_context& ctx) -> void {
	dispatch_update<S>(ctx, m_resources, m_update_data, m_frame_data, m_state);
}

template <typename S, typename State>
auto gse::system_node<S, State>::graph_frame(frame_context& ctx) -> async::task<> {
	if constexpr (std::is_trivially_copyable_v<State>) {
		co_await dispatch_frame<S>(ctx, m_resources, m_frame_data, m_snapshot.value);
	}
	else {
		co_await dispatch_frame<S>(ctx, m_resources, m_frame_data, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::has_frame() const -> bool {
	return dispatch_has_frame<S, State>();
}

template <typename S, typename State>
auto gse::system_node<S, State>::snapshot_state() -> void {
	if constexpr (std::is_trivially_copyable_v<State>) {
		m_snapshot.value = m_state;
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_snapshot_ptr() const -> const void* {
	if constexpr (std::is_trivially_copyable_v<State>) {
		return &m_snapshot.value;
	}
	return nullptr;
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
