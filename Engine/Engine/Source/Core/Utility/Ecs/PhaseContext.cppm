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
		auto state_of() const -> const State& {
			const auto* ptr = snapshot_ptr(std::type_index(typeid(State)));
			return *static_cast<const State*>(ptr);
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			const auto* ptr = snapshot_ptr(std::type_index(typeid(State)));
			return static_cast<const State*>(ptr);
		}
	};

	template <typename T>
	class channel;

	class channel_writer {
	public:
		using push_fn = std::function<void(std::type_index, std::any, channel_factory_fn)>;

		explicit channel_writer(push_fn fn) : m_push(std::move(fn)) {}

		template <typename T>
		auto push(T item) -> void {
			m_push(
				std::type_index(typeid(T)),
				std::any(std::move(item)),
				+[]() -> std::unique_ptr<channel_base> {
					return std::make_unique<typed_channel<T>>();
				}
			);
		}

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
		auto read() const -> const std::vector<T>& {
			const auto* ptr = channel_snapshot_ptr(std::type_index(typeid(T)));
			if (!ptr) {
				static const std::vector<T> empty;
				return empty;
			}
			return *static_cast<const std::vector<T>*>(ptr);
		}
	};

	struct registry_access {
		registry* reg;

		template <is_component T>
		auto view() const -> std::span<const T> {
			return reg->linked_objects_read<T>();
		}

		template <is_component T>
		auto chunk() -> std::span<T> {
			return reg->linked_objects_write<T>();
		}

		template <is_component T>
		auto try_read(const id owner) const -> const T* {
			return reg->try_linked_object_read<T>(owner);
		}

		template <is_component T>
		auto try_write(const id owner) -> T* {
			return reg->try_linked_object_write<T>(owner);
		}

		auto entity_active(const id owner) const -> bool {
			return reg->active(owner);
		}

		auto ensure_active(const id owner) const -> void {
			reg->ensure_active(owner);
		}

		auto ensure_exists(const id owner) const -> void {
			reg->ensure_exists(owner);
		}
	};

	struct pending_work_desc {
		std::vector<std::type_index> component_writes;
		std::type_index channel_write = typeid(void);
	};

	struct initialize_phase {
		registry_access& registry;
		const state_snapshot_provider& snapshots;
		channel_writer& channels;

		template <typename State>
		auto state_of() const -> const State& {
			return snapshots.state_of<State>();
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			return snapshots.try_state_of<State>();
		}
	};

	struct update_phase {
		registry_access& registry;
		const state_snapshot_provider& snapshots;
		channel_writer& channels;
		const channel_reader_provider& channel_reader;

		template <typename State>
		auto state_of() const -> const State& {
			return snapshots.state_of<State>();
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			return snapshots.try_state_of<State>();
		}

		template <typename T>
		auto read_channel() const -> const std::vector<T>& {
			return channel_reader.read<T>();
		}
	};

	struct begin_frame_phase {
		const state_snapshot_provider& snapshots;

		template <typename State>
		auto state_of() const -> const State& {
			return snapshots.state_of<State>();
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			return snapshots.try_state_of<State>();
		}
	};

	struct render_phase {
		const registry_access& registry;
		const state_snapshot_provider& snapshots;
		const channel_reader_provider& channel_reader;

		template <typename State>
		auto state_of() const -> const State& {
			return snapshots.state_of<State>();
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			return snapshots.try_state_of<State>();
		}

		template <typename T>
		auto read_channel() const -> const std::vector<T>& {
			return channel_reader.read<T>();
		}
	};

	struct end_frame_phase {
		const state_snapshot_provider& snapshots;
		channel_writer& channels;

		template <typename State>
		auto state_of() const -> const State& {
			return snapshots.state_of<State>();
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			return snapshots.try_state_of<State>();
		}
	};

	struct shutdown_phase {
		registry_access& registry;
	};

	template <typename S, typename State>
	concept has_initialize = requires(initialize_phase& p, State& s) {
		{ S::initialize(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State>
	concept has_update = requires(update_phase& p, State& s) {
		{ S::update(p, s) } -> std::same_as<void>;
	};

	template <typename S, typename State>
	concept has_begin_frame = requires(begin_frame_phase& p, State& s) {
		{ S::begin_frame(p, s) } -> std::same_as<bool>;
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
}
