export module gse.diag:trace;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.math;
import gse.meta;

export namespace gse::trace {
	struct config {
		std::size_t per_thread_event_cap = 262144;
		bool enable_browser_dump = false;
	};

	auto start(
		const config& cfg = {}
	) -> void;

	auto begin_block(
		id id,
		std::uint64_t parent
	) -> std::uint64_t;

	auto end_block(
		id id,
		std::uint64_t eid,
		std::uint64_t parent
	) -> void;

	auto begin_async(
		id id,
		std::uint64_t key
	) -> void;

	auto end_async(
		id id,
		std::uint64_t key
	) -> void;

	auto allocate_async_key(
	) -> std::uint64_t;

	class scope_guard {
	public:
		explicit scope_guard(
			id id
		);

		scope_guard(
			id id,
			std::uint64_t parent
		);

		~scope_guard(
		);

		scope_guard(
			const scope_guard&
		) = delete;

		scope_guard(
			scope_guard&&
		) = delete;

		auto operator=(
			const scope_guard&
		) -> scope_guard& = delete;

		auto operator=(
			scope_guard&&
		) -> scope_guard& = delete;

	private:
		auto enter(
			std::uint64_t parent
		) -> void;

		id m_id;
		std::uint32_t m_tid = 0;
		std::uint64_t m_parent = 0;
		std::uint64_t m_eid = 0;
		bool m_pushed_parent = false;
	};

	auto mark(
		id id
	) -> void;

	auto counter(
		id id,
		double value
	) -> void;

	constexpr std::uint32_t gpu_virtual_tid = 0xFFFFFFFFu;
	constexpr std::uint32_t gpu_stats_virtual_tid = 0xFFFFFFFEu;

	auto begin_async_at(
		id id,
		std::uint64_t key,
		std::uint32_t tid,
		time_t<std::uint64_t> ts
	) -> void;

	auto end_async_at(
		id id,
		std::uint64_t key,
		std::uint32_t tid,
		time_t<std::uint64_t> ts
	) -> void;

	auto counter_at(
		id id,
		double value,
		std::uint32_t tid,
		time_t<std::uint64_t> ts
	) -> void;

	auto register_virtual_thread(
		std::uint32_t tid,
		std::string_view name
	) -> void;

	auto make_loc_id(
		const std::source_location& loc
	) -> id;

	auto mark_hidden(
		id id
	) -> void;

	auto is_hidden(
		id id
	) -> bool;

	auto mark_pool_root(
		id id
	) -> void;

	auto is_pool_root(
		id id
	) -> bool;

	auto current_eid(
	) -> std::uint64_t;

	struct node {
		id id;
		std::uint32_t trace_id;
		time_t<std::uint64_t> start;
		time_t<std::uint64_t> stop;
		time_t<std::uint64_t> self;
		const node* children_first = nullptr;
		std::size_t children_count = 0;
	};

	using node_view = std::span<const node>;

	struct frame_view {
		std::span<const node> roots;
		const std::byte* storage;
	};

	auto finalize_frame(
	) -> void;

	auto view(
	) -> frame_view;

	struct thread_pause {
		thread_pause();
		~thread_pause();
	};

	auto paused(
	) -> bool;

	auto set_enabled(
		bool enable
	) -> void;

	auto enabled(
	) -> bool;

	auto set_finalize_paused(
		bool pause
	) -> void;

	auto finalize_paused(
	) -> bool;
}

namespace gse::trace {
	export using tick_step = time_t<std::uint64_t>;

	enum struct event_type : std::uint8_t {
		begin,
		end,
		instant,
		async_begin,
		async_end,
		counter
	};

	struct event {
		event_type type;
		id id;
		std::uint64_t eid = 0;
		std::uint64_t parent_eid = 0;
		std::uint64_t tid = 0;
		time_t<std::uint64_t> ts;
		double value;
		std::uint64_t key;
	};

	class scsp_events {
	public:
		auto push(
			const event& e
		) noexcept -> void;

		template <typename Out>
		auto drain_to(
			Out& out
		) noexcept -> void;

		auto clear(
		) noexcept -> void;

		auto size(
		) const noexcept -> std::size_t;
	private:
		static constexpr std::uint32_t capacity_pow_2 = 1u << 18;
		static constexpr std::uint32_t capacity = capacity_pow_2;
		static constexpr std::uint32_t capacity_mask = capacity_pow_2 - 1;

		alignas(64) std::atomic<std::uint32_t> m_w{ 0 };
		alignas(64) std::uint32_t m_r{ 0 };

		std::unique_ptr<event[]> m_events = std::make_unique<event[]>(capacity);
	};

	struct thread_buffer {
		scsp_events events;
		std::vector<std::uint64_t> stack;
		std::uint32_t tid = 0;
		bool registered = false;
	};

	inline thread_local int tls_pause_depth = 0;

	std::atomic trace_enabled = true;
	std::atomic finalize_paused_flag = false;

	std::atomic<std::uint64_t> next_eid{ 1024 };
	std::atomic<std::uint64_t> next_async_key{ 1 };
	std::atomic<std::uint32_t> next_tid{ 0 };

	config global_config;

	inline thread_local thread_buffer tls;

	std::mutex tid_map_mutex;
	std::unordered_map<std::thread::id, std::uint32_t> tid_map;

	std::mutex tls_registry_mutex;
	std::vector<thread_buffer*> tls_registry;

	struct span_info {
		id id;
		std::uint32_t tid;
		time_t<std::uint64_t> t0;
		time_t<std::uint64_t> t1;
		std::uint64_t parent;
	};

	struct frame_storage {
		std::vector<event> merged;

		struct flat_node {
			id id;
			std::uint32_t tid;
			time_t<std::uint64_t> start = {};
			time_t<std::uint64_t> end = {};
			time_t<std::uint64_t> self = {};
			std::vector<std::size_t> children_idx;
		};

		std::vector<flat_node> flat;
		std::vector<node> node_pool;
		std::vector<node> roots;
	};

	double_buffer<frame_storage> frames;
	std::unordered_map<std::uint64_t, span_info> global_open_spans;

	std::shared_mutex hidden_ids_mutex;
	std::unordered_set<id> hidden_ids;

	std::shared_mutex pool_root_ids_mutex;
	std::unordered_set<id> pool_root_ids;

	std::shared_mutex virtual_thread_mutex;
	std::unordered_map<std::uint32_t, std::string> virtual_thread_names;

	auto ensure_tls_registered(
	) -> void;

	auto make_tid(
	) -> std::uint32_t;

	auto emit(
		const event& e
	) -> void;

	auto current_parent_eid(
	) -> std::uint64_t;

	auto build_tree(
		frame_storage& fs
	) -> void;

	auto allocate_span_eid(
	) -> std::uint64_t;
}