export module gse.utility:system_node;

import std;

import :phase_context;

export namespace gse {
	class system_node_base {
	public:
		virtual ~system_node_base() = default;

		virtual auto initialize(
			initialize_phase&
		) -> void = 0;

		virtual auto update(
			update_phase&
		) -> void = 0;

		virtual auto begin_frame(
			begin_frame_phase&
		) -> bool = 0;

		virtual auto render(
			render_phase&
		) -> void = 0;

		virtual auto end_frame(
			end_frame_phase&
		) -> void = 0;

		virtual auto shutdown(
			shutdown_phase&
		) -> void = 0;

		virtual auto snapshot(
		) -> void = 0;

		virtual auto state_ptr(
		) -> void* = 0;

		virtual auto state_ptr(
		) const -> const void* = 0;

		virtual auto snapshot_ptr(
		) const -> const void* = 0;

		virtual auto state_type(
		) const -> std::type_index = 0;
	};

	template <typename S, typename State>
	class system_node final : public system_node_base {
	public:
		template <typename... Args>
		explicit system_node(
			Args&&... args
		);

		auto initialize(
			initialize_phase& phase
		) -> void override;

		auto update(
			update_phase& phase
		) -> void override;

		auto begin_frame(
			begin_frame_phase& phase
		) -> bool override;

		auto render(
			render_phase& phase
		) -> void override;

		auto end_frame(
			end_frame_phase& phase
		) -> void override;

		auto shutdown(
			shutdown_phase& phase
		) -> void override;

		auto snapshot(
		) -> void override;

		auto state_ptr(
		) -> void* override;

		auto state_ptr(
		) const -> const void* override;

		auto snapshot_ptr(
		) const -> const void* override;

		auto state_type(
		) const -> std::type_index override;

		auto state(
			this auto& self
		) -> auto&;
	private:
		State m_state;
	};
}

template <typename S, typename State>
template <typename ... Args>
gse::system_node<S, State>::system_node(Args&&... args) : m_state(std::forward<Args>(args)...) {}

template <typename S, typename State>
auto gse::system_node<S, State>::initialize(initialize_phase& phase) -> void {
	if constexpr (has_initialize<S, State>) {
		S::initialize(phase, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::update(update_phase& phase) -> void {
	if constexpr (has_update<S, State>) {
		S::update(phase, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::begin_frame(begin_frame_phase& phase) -> bool {
	if constexpr (has_begin_frame<S, State>) {
		return S::begin_frame(phase, m_state);
	}
	return true;
}

template <typename S, typename State>
auto gse::system_node<S, State>::render(render_phase& phase) -> void {
	if constexpr (has_render<S, State>) {
		S::render(phase, std::as_const(m_state));
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::end_frame(end_frame_phase& phase) -> void {
	if constexpr (has_end_frame<S, State>) {
		S::end_frame(phase, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::shutdown(shutdown_phase& phase) -> void {
	if constexpr (has_shutdown<S, State>) {
		S::shutdown(phase, m_state);
	}
}

template <typename S, typename State>
auto gse::system_node<S, State>::snapshot() -> void {}

template <typename S, typename State>
auto gse::system_node<S, State>::state_ptr() -> void* {
	return &m_state;
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_ptr() const -> const void* {
	return &m_state;
}

template <typename S, typename State>
auto gse::system_node<S, State>::snapshot_ptr() const -> const void* {
	return &m_state;
}

template <typename S, typename State>
auto gse::system_node<S, State>::state_type() const -> std::type_index {
	return std::type_index(typeid(State));
}

template <typename S, typename State>
auto gse::system_node<S, State>::state(this auto& self) -> auto& {
	return self.m_state;
} 