export module gse.utility:phase_context;

import std;

import :concepts;
import :registry;
import :id;
import :n_buffer;
import :channel_base;

export namespace gse {
	class system_provider {
	public:
		virtual ~system_provider() = default;

		virtual auto system_ptr(
			std::type_index idx
		) -> void* = 0;

		virtual auto system_ptr(
			std::type_index idx
		) const -> const void* = 0;

		virtual auto ensure_channel(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base& = 0;
	};

	class state_snapshot_provider {
	public:
		virtual ~state_snapshot_provider() = default;

		virtual auto snapshot_ptr(
			std::type_index type
		) const -> const void* = 0;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;
	};

	template <typename T>
	class channel;

	class channel_writer {
	public:
		using push_fn = std::function<void(std::type_index, std::any, channel_factory_fn)>;

		explicit channel_writer(
			push_fn fn
		);

		template <typename T>
		auto push(
			T item
		) -> void;

	private:
		push_fn m_push;
	};

	class channel_reader_provider {
	public:
		virtual ~channel_reader_provider() = default;

		virtual auto channel_snapshot_ptr(
			std::type_index type
		) const -> const void* = 0;

		template <typename T>
		auto read(
		) const -> const std::vector<T>&;
	};

	struct registry_access {
		registry* reg;

		template <is_component T>
		auto view(
		) const -> std::span<const T>;

		template <is_component T>
		auto try_read(
			id owner
		) const -> const T*;

		auto entity_active(
			id owner
		) const -> bool;

		auto ensure_active(
			id owner
		) const -> void;

		auto ensure_exists(
			id owner
		) const -> void;
	};

	struct phase_gpu_access {
		void* gpu_ctx = nullptr;

		template <typename T>
		auto get(
		) const -> T&;

		template <typename T>
		auto try_get(
		) const -> T*;
	};

	struct initialize_phase : phase_gpu_access {
		registry_access& registry;
		const state_snapshot_provider& snapshots;
		channel_writer& channels;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;
	};


	struct begin_frame_phase : phase_gpu_access {
		const state_snapshot_provider& snapshots;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;
	};

	struct prepare_render_phase : phase_gpu_access {
		const state_snapshot_provider& snapshots;
		const channel_reader_provider& channel_reader;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;

		template <typename T>
		auto read_channel(
		) const -> channel_read_guard<T>;
	};

	struct render_phase : phase_gpu_access {
		const registry_access& registry;
		const state_snapshot_provider& snapshots;
		const channel_reader_provider& channel_reader;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;

		template <typename T>
		auto read_channel(
		) const -> channel_read_guard<T>;
	};

	struct end_frame_phase : phase_gpu_access {
		const state_snapshot_provider& snapshots;
		channel_writer& channels;

		template <typename State>
		auto state_of(
		) const -> const State&;

		template <typename State>
		auto try_state_of(
		) const -> const State*;
	};

	struct shutdown_phase : phase_gpu_access {
		registry_access& registry;
	};

	template <typename S, typename State>
	concept has_initialize = requires(initialize_phase& p, State& s) {
		{ S::initialize(p, s) } -> std::same_as<void>;
	};


	template <typename S, typename State>
	concept has_begin_frame = requires(begin_frame_phase& p, State& s) {
		{ S::begin_frame(p, s) } -> std::same_as<bool>;
	};

	template <typename S, typename State>
	concept has_prepare_render = requires(prepare_render_phase& p, State& s) {
		{ S::prepare_render(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State>
	concept has_render = requires(render_phase& p, const State& s) {
		{ S::render(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State>
	concept has_end_frame = requires(end_frame_phase& p, State& s) {
		{ S::end_frame(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State>
	concept has_shutdown = requires(shutdown_phase& p, State& s) {
		{ S::shutdown(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State, typename RenderState>
	concept has_prepare_render_with_state = requires(prepare_render_phase& p, State& s, RenderState& rs) {
		{ S::prepare_render(p, s, rs) } -> std::same_as<void>;
	};

	template <typename S, typename State, typename RenderState>
	concept has_render_with_state = requires(render_phase& p, const State& s, RenderState& rs) {
		{ S::render(p, s, rs) } -> std::same_as<void>;
	};

	template <typename S, typename State, typename RenderState>
	concept has_begin_frame_with_state = requires(begin_frame_phase& p, State& s, RenderState& rs) {
		{ S::begin_frame(p, s, rs) } -> std::same_as<bool>;
	};

	template <typename S, typename State, typename RenderState>
	concept has_end_frame_with_state = requires(end_frame_phase& p, State& s, RenderState& rs) {
		{ S::end_frame(p, s, rs) } -> std::same_as<void>;
	};

	template <typename S, typename State, typename RenderState>
	concept has_initialize_with_state = requires(initialize_phase& p, State& s, RenderState& rs) {
		{ S::initialize(p, s, rs) } -> std::same_as<void>;
	};
}

template <typename State>
auto gse::state_snapshot_provider::state_of() const -> const State& {
	const auto* ptr = snapshot_ptr(std::type_index(typeid(State)));
	return *static_cast<const State*>(ptr);
}

template <typename State>
auto gse::state_snapshot_provider::try_state_of() const -> const State* {
	const auto* ptr = snapshot_ptr(std::type_index(typeid(State)));
	return static_cast<const State*>(ptr);
}

gse::channel_writer::channel_writer(push_fn fn) : m_push(std::move(fn)) {}

template <typename T>
auto gse::channel_writer::push(T item) -> void {
	m_push(
		std::type_index(typeid(T)),
		std::any(std::move(item)),
		+[]() -> std::unique_ptr<channel_base> {
			return std::make_unique<typed_channel<T>>();
		}
	);
}

template <typename T>
auto gse::channel_reader_provider::read() const -> const std::vector<T>& {
	const auto* ptr = channel_snapshot_ptr(std::type_index(typeid(T)));
	if (!ptr) {
		static const std::vector<T> empty;
		return empty;
	}
	return *static_cast<const std::vector<T>*>(ptr);
}

template <gse::is_component T>
auto gse::registry_access::view() const -> std::span<const T> {
	return reg->linked_objects_read<T>();
}

template <gse::is_component T>
auto gse::registry_access::try_read(const id owner) const -> const T* {
	return reg->try_linked_object_read<T>(owner);
}

auto gse::registry_access::entity_active(const id owner) const -> bool {
	return reg->active(owner);
}

auto gse::registry_access::ensure_active(const id owner) const -> void {
	reg->ensure_active(owner);
}

auto gse::registry_access::ensure_exists(const id owner) const -> void {
	reg->ensure_exists(owner);
}

template <typename T>
auto gse::phase_gpu_access::get() const -> T& {
	return *static_cast<T*>(gpu_ctx);
}

template <typename T>
auto gse::phase_gpu_access::try_get() const -> T* {
	return static_cast<T*>(gpu_ctx);
}

template <typename State>
auto gse::initialize_phase::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::initialize_phase::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename State>
auto gse::begin_frame_phase::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::begin_frame_phase::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename State>
auto gse::prepare_render_phase::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::prepare_render_phase::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename T>
auto gse::prepare_render_phase::read_channel() const -> channel_read_guard<T> {
	return channel_read_guard<T>(channel_reader.read<T>());
}

template <typename State>
auto gse::render_phase::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::render_phase::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}

template <typename T>
auto gse::render_phase::read_channel() const -> channel_read_guard<T> {
	return channel_read_guard<T>(channel_reader.read<T>());
}

template <typename State>
auto gse::end_frame_phase::state_of() const -> const State& {
	return snapshots.state_of<State>();
}

template <typename State>
auto gse::end_frame_phase::try_state_of() const -> const State* {
	return snapshots.try_state_of<State>();
}
