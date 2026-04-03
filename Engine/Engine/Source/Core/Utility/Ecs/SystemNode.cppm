export module gse.utility:system_node;

import std;

import :phase_context;
import :id;

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

		virtual auto state_ptr(
		) -> void* = 0;

		virtual auto state_ptr(
		) const -> const void* = 0;

		virtual auto state_type(
		) const -> std::type_index = 0;

		virtual auto trace_id(
		) const -> id = 0;
	};

	template <typename T>
	struct render_state_storage {
		T value;

		template <typename... Args>
		explicit render_state_storage(Args&&... args) : value(std::forward<Args>(args)...) {}
	};

	template <>
	struct render_state_storage<void> {};

	template <typename S, typename State, typename RenderState = void>
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

		auto state_ptr(
		) -> void* override;

		auto state_ptr(
		) const -> const void* override;

		auto state_type(
		) const -> std::type_index override;

		auto state(
			this auto& self
		) -> auto&;

		auto trace_id(
		) const -> id override;

	private:
		State m_state;
		[[no_unique_address]] render_state_storage<RenderState> m_render_state;
	};
}

template <typename S, typename State, typename RenderState>
template <typename... Args>
gse::system_node<S, State, RenderState>::system_node(Args&&... args)
	: m_state(std::forward<Args>(args)...) {}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::initialize(initialize_phase& phase) -> void {
	if constexpr (!std::is_void_v<RenderState> && has_initialize_with_state<S, State, RenderState>) {
		S::initialize(phase, m_state, m_render_state.value);
	} else if constexpr (has_initialize<S, State>) {
		S::initialize(phase, m_state);
	}
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::trace_id() const -> id {
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

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::update(update_phase& phase) -> void {
	if constexpr (has_update<S, State>) {
		S::update(phase, m_state);
	}
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::begin_frame(begin_frame_phase& phase) -> bool {
	if constexpr (!std::is_void_v<RenderState> && has_begin_frame_with_state<S, State, RenderState>) {
		return S::begin_frame(phase, m_state, m_render_state.value);
	} else if constexpr (has_begin_frame<S, State>) {
		return S::begin_frame(phase, m_state);
	}
	return true;
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::render(render_phase& phase) -> void {
	if constexpr (!std::is_void_v<RenderState> && has_render_with_state<S, State, RenderState>) {
		S::render(phase, m_state, m_render_state.value);
	} else if constexpr (has_render<S, State>) {
		S::render(phase, m_state);
	}
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::end_frame(end_frame_phase& phase) -> void {
	if constexpr (!std::is_void_v<RenderState> && has_end_frame_with_state<S, State, RenderState>) {
		S::end_frame(phase, m_state, m_render_state.value);
	} else if constexpr (has_end_frame<S, State>) {
		S::end_frame(phase, m_state);
	}
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::shutdown(shutdown_phase& phase) -> void {
	if constexpr (has_shutdown<S, State>) {
		S::shutdown(phase, m_state);
	}
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::state_ptr() -> void* {
	return &m_state;
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::state_ptr() const -> const void* {
	return &m_state;
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::state_type() const -> std::type_index {
	return std::type_index(typeid(State));
}

template <typename S, typename State, typename RenderState>
auto gse::system_node<S, State, RenderState>::state(this auto& self) -> auto& {
	return self.m_state;
}
