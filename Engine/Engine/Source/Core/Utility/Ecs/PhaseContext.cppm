export module gse.utility:phase_context;

import std;

import :concepts;
import :registry;
import :id;
import :n_buffer;
import :channel_base;
import :lambda_traits;

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
		auto try_read(const id owner) const -> const T* {
			return reg->try_linked_object_read<T>(owner);
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

	template <typename T>
	class chunk {
	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;

		explicit chunk(std::span<T> span) : m_span(span) {}

		auto begin() -> auto { return m_span.begin(); }
		auto end() -> auto { return m_span.end(); }
		auto begin() const -> auto { return m_span.begin(); }
		auto end() const -> auto { return m_span.end(); }

		auto size() const -> std::size_t { return m_span.size(); }
		auto empty() const -> bool { return m_span.empty(); }
		auto data() -> pointer { return m_span.data(); }
		auto data() const -> const_pointer { return m_span.data(); }

		auto operator[](std::size_t i) -> reference { return m_span[i]; }
		auto operator[](std::size_t i) const -> const_reference { return m_span[i]; }

		auto find(const id owner) -> pointer {
			build_lookup();
			if (const auto it = m_lookup->find(owner); it != m_lookup->end()) {
				return it->second;
			}
			return nullptr;
		}

		auto find(const id owner) const -> const_pointer {
			const_cast<chunk*>(this)->build_lookup();
			if (const auto it = m_lookup->find(owner); it != m_lookup->end()) {
				return it->second;
			}
			return nullptr;
		}

	private:
		auto build_lookup() -> void {
			if (m_lookup) return;
			m_lookup.emplace();
			m_lookup->reserve(m_span.size());
			for (auto& item : m_span) {
				(*m_lookup)[item.owner_id()] = std::addressof(item);
			}
		}

		std::span<T> m_span;
		mutable std::optional<std::unordered_map<id, pointer>> m_lookup;
	};

	struct queued_work {
		id name;
		std::vector<std::type_index> reads;
		std::vector<std::type_index> writes;
		std::move_only_function<void(registry&)> execute;

		auto conflicts_with(const queued_work& other) const -> bool {
			for (const auto& w : writes) {
				if (std::ranges::contains(other.writes, w)) {
					return true;
				}
			}
			for (const auto& r : reads) {
				if (std::ranges::contains(other.writes, r)) {
					return true;
				}
			}
			for (const auto& w : writes) {
				if (std::ranges::contains(other.reads, w)) {
					return true;
				}
			}
			return false;
		}
	};

	class work_queue {
	public:
		template <typename F>
		auto schedule(const id name, F&& action) -> void {
			using traits = lambda_traits<std::decay_t<F>>;

			m_work.push_back(queued_work{
				.name = name,
				.reads = traits::reads(),
				.writes = traits::writes(),
				.execute = make_executor(std::forward<F>(action))
			});
		}

		auto work() -> std::vector<queued_work>& {
			return m_work;
		}

	private:
		template <typename F>
		static constexpr bool is_raw_registry_lambda =
			lambda_traits<std::decay_t<F>>::arity == 1 &&
			std::is_same_v<std::tuple_element_t<0, typename lambda_traits<std::decay_t<F>>::arg_tuple>, registry&>;

		template <typename F>
		auto make_executor(F&& f) -> std::move_only_function<void(registry&)> {
			if constexpr (is_raw_registry_lambda<F>) {
				return [func = std::forward<F>(f)](registry& reg) mutable {
					func(reg);
				};
			}
			else {
				return [func = std::forward<F>(f)](registry& reg) mutable {
					using traits = lambda_traits<std::decay_t<F>>;
					using arg_tuple = typename traits::arg_tuple;
					invoke_with_chunks<arg_tuple>(reg, func, std::make_index_sequence<traits::arity>{});
				};
			}
		}

		template <typename ArgTuple, typename F, std::size_t... Is>
		static auto invoke_with_chunks(registry& reg, F& func, std::index_sequence<Is...>) -> void {
			func(make_chunk<std::tuple_element_t<Is, ArgTuple>>(reg)...);
		}

		template <typename ChunkArg>
		static auto make_chunk(registry& reg) -> ChunkArg {
			using element_t = chunk_element_t<ChunkArg>;

			if constexpr (is_read_chunk_v<ChunkArg>) {
				return ChunkArg(reg.linked_objects_read<element_t>());
			}
			else {
				return ChunkArg(reg.linked_objects_write<element_t>());
			}
		}

		std::vector<queued_work> m_work;
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
		const registry_access& registry;
		const state_snapshot_provider& snapshots;
		channel_writer& channels;
		const channel_reader_provider& channel_reader;
		work_queue& work;

		template <typename State>
		auto state_of() const -> const State& {
			return snapshots.state_of<State>();
		}

		template <typename State>
		auto try_state_of() const -> const State* {
			return snapshots.try_state_of<State>();
		}

		template <typename T>
		auto read_channel() const -> channel_read_guard<T> {
			return channel_read_guard<T>(channel_reader.read<T>());
		}

		template <typename F>
		auto schedule(F&& action, std::source_location loc = std::source_location::current()) -> void {
			auto name = std::string(loc.function_name());
			if (const auto paren = name.find('('); paren != std::string::npos) {
				name = name.substr(0, paren);
			}
			if (const auto space = name.rfind(' '); space != std::string::npos) {
				name = name.substr(space + 1);
			}
			work.schedule(find_or_generate_id(name), std::forward<F>(action));
		}

		template <is_component T, typename... Args>
		auto defer_add(const id entity, Args&&... args) -> void {
			work.schedule(find_or_generate_id("defer_add"), [entity, ...args = std::forward<Args>(args)](gse::registry& reg) mutable {
				reg.ensure_exists(entity);
				if (!reg.active(entity)) {
					reg.ensure_active(entity);
				}
				reg.add_component<T>(entity, std::forward<Args>(args)...);
			});
		}

		template <is_component T>
		auto defer_remove(const id entity) -> void {
			work.schedule(find_or_generate_id("defer_remove"), [entity](gse::registry& reg) {
				reg.remove_link<T>(entity);
			});
		}

		auto defer_activate(const id entity) const -> void {
			work.schedule(find_or_generate_id("defer_activate"), [entity](gse::registry& reg) {
				reg.ensure_active(entity);
			});
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
		auto read_channel() const -> channel_read_guard<T> {
			return channel_read_guard<T>(channel_reader.read<T>());
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
