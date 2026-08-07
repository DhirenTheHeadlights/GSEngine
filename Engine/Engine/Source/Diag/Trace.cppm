export module gse.diag:trace;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.math;
import gse.meta;

export namespace gse::trace {
	using tick_step = time_t<std::uint64_t>;

	struct config {
		bool enable_browser_dump = false;
	};

	constexpr id untraced{};

	constexpr std::uint64_t first_span_eid = 1024;

	constexpr std::size_t max_loc_tag_length = 192;

	consteval auto strip_function_signature(
		std::string_view fn
	) -> std::string_view;

	consteval auto current_loc_tag(
		std::source_location loc = std::source_location::current()
	) -> fixed_string<max_loc_tag_length>;

	auto start(
		const config& cfg = {}
	) -> void;

	auto current_eid() -> std::uint64_t;

	class open_span : non_copyable {
	public:
		open_span() = default;

		open_span(
			id id,
			std::uint64_t parent
		);

		~open_span();

		open_span(
			open_span&& other
		) noexcept;

		auto operator=(
			open_span&& other
		) noexcept -> open_span&;

		auto close() -> void;

		[[nodiscard]] auto eid() const -> std::uint64_t;

	private:
		id m_id;
		std::uint64_t m_eid = 0;
		std::uint64_t m_parent = 0;
		std::uint32_t m_tid = 0;
	};

	class scope_guard : non_copyable, non_movable {
	public:
		explicit scope_guard(
			id id
		);

		scope_guard(
			id id,
			std::uint64_t parent
		);

		~scope_guard();

	private:
		auto enter(
			std::uint64_t parent
		) -> void;

		id m_id;
		std::uint64_t m_parent = 0;
		std::uint64_t m_eid = 0;
		std::size_t m_depth = 0;
		std::uint32_t m_tid = 0;
	};

	auto begin_async(
		id id,
		std::uint64_t key
	) -> void;

	auto end_async(
		id id,
		std::uint64_t key
	) -> void;

	auto allocate_async_key() -> std::uint64_t;

	auto mark(
		id id
	) -> void;

	auto counter(
		id id,
		double value
	) -> void;

	constexpr std::uint32_t gpu_virtual_tid = 0xFFFFFFFFu;
	constexpr std::uint32_t gpu_stats_virtual_tid = 0xFFFFFFFEu;
	constexpr std::uint32_t gpu_compute_virtual_tid = 0xFFFFFFFDu;

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

	auto virtual_thread_name(
		std::uint32_t tid
	) -> std::optional<std::string>;

	auto hidden_ids_snapshot() -> std::unordered_set<id>;

	auto register_main_thread() -> void;

	auto main_tid() -> std::uint32_t;

	auto mark_hidden(
		id id
	) -> void;

	auto is_hidden(
		id id
	) -> bool;

	struct node {
		id id;
		std::uint32_t trace_id = 0;
		time_t<std::uint64_t> start;
		time_t<std::uint64_t> stop;
		time_t<std::uint64_t> self;
		std::uint32_t children_first = 0;
		std::uint32_t children_count = 0;
		bool open = false;
	};

	struct frame_view {
		std::span<const node> nodes;
		std::span<const std::uint32_t> children;
		std::span<const std::uint32_t> roots;
		std::uint64_t generation = 0;

		[[nodiscard]] auto child_indices(
			const node& n
		) const -> std::span<const std::uint32_t>;
	};

	auto finalize_frame() -> void;

	auto view() -> frame_view;

	auto dropped_events() -> std::uint64_t;

	auto abandoned_spans() -> std::uint64_t;

	struct thread_pause {
		thread_pause();
		~thread_pause();
	};

	auto paused() -> bool;

	auto set_enabled(
		bool enable
	) -> void;

	auto enabled() -> bool;
}

namespace gse::trace {
	enum struct event_type : std::uint8_t {
		begin,
		end,
		instant,
		async_begin,
		async_end,
		counter
	};

	struct event {
		event_type type = event_type::instant;
		id id;
		std::uint64_t eid = 0;
		std::uint64_t parent_eid = 0;
		std::uint64_t tid = 0;
		time_t<std::uint64_t> ts;
		double value = 0.0;
		std::uint64_t key = 0;
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

		auto ensure_storage() -> void;

		[[nodiscard]] auto dropped() const noexcept -> std::uint64_t;

	private:
		static constexpr std::uint32_t capacity = 1u << 18;
		static constexpr std::uint32_t capacity_mask = capacity - 1;

		alignas(64) std::atomic<std::uint32_t> m_w{ 0 };
		alignas(64) std::atomic<std::uint32_t> m_r{ 0 };
		alignas(64) std::atomic<std::uint64_t> m_dropped{ 0 };

		std::unique_ptr<event[]> m_events;
	};

	struct thread_buffer {
		~thread_buffer();

		scsp_events events;
		std::vector<std::uint64_t> stack;
		std::uint32_t tid = 0;
		bool registered = false;
	};

	struct thread_registry {
		std::mutex mutex;
		std::vector<thread_buffer*> buffers;
	};

	struct span_info {
		id id;
		std::uint32_t tid = 0;
		time_t<std::uint64_t> t0;
		time_t<std::uint64_t> t1;
		std::uint64_t parent = 0;
		std::uint64_t opened_frame = 0;
	};

	struct frame_span {
		std::uint64_t eid = 0;
		span_info info;
		bool open = false;
	};

	struct seg {
		time_t<std::uint64_t> a;
		time_t<std::uint64_t> b;
	};

	struct build_scratch {
		std::vector<event> merged;
		std::vector<frame_span> spans;
		std::vector<std::uint32_t> parent_idx;
		std::vector<std::uint32_t> child_counts;
		std::vector<seg> segs;
	};

	struct frame_storage {
		std::vector<node> nodes;
		std::vector<std::uint32_t> children;
		std::vector<std::uint32_t> roots;
		std::uint64_t generation = 0;
	};

	constexpr std::uint64_t max_open_span_frames = 240;

	constexpr std::uint32_t no_parent = std::numeric_limits<std::uint32_t>::max();

	inline thread_local int tls_pause_depth = 0;

	inline thread_local thread_buffer tls;

	inline std::atomic trace_enabled = true;

	inline std::atomic<std::uint64_t> next_eid{ first_span_eid };
	inline std::atomic<std::uint64_t> next_async_key{ 1 };
	inline std::atomic<std::uint32_t> next_tid{ 0 };
	inline std::atomic<std::uint32_t> main_tid_value{ 0 };
	inline std::atomic<std::uint64_t> abandoned_span_count{ 0 };

	inline config global_config;

	inline triple_buffer<frame_storage> frames;
	inline build_scratch scratch;
	inline std::unordered_map<std::uint64_t, span_info> open_spans;
	inline std::vector<frame_span> closed_spans;
	inline std::uint64_t build_frame_index = 0;
	inline std::uint64_t published_generation = 0;

	inline std::shared_mutex hidden_ids_mutex;
	inline std::unordered_set<id> hidden_ids;

	inline std::shared_mutex virtual_thread_mutex;
	inline std::unordered_map<std::uint32_t, std::string> virtual_thread_names;

	auto registry() -> thread_registry&;

	auto ensure_tls_registered() -> void;

	auto make_tid() -> std::uint32_t;

	auto emit(
		const event& e
	) -> void;

	auto allocate_span_eid() -> std::uint64_t;

	auto close_span(
		id id,
		std::uint64_t eid,
		std::uint64_t parent,
		std::uint32_t tid
	) -> void;

	auto drain_events(
		std::vector<event>& out
	) -> void;

	auto absorb_events(
		std::span<const event> events
	) -> void;

	auto evict_stale_open_spans() -> void;

	auto collect_frame_spans(
		std::vector<frame_span>& out
	) -> void;

	auto link_parents(
		std::span<const frame_span> spans,
		std::vector<std::uint32_t>& parent_idx
	) -> void;

	auto build_frame(
		frame_storage& fs
	) -> void;

	auto compute_self_times(
		frame_storage& fs,
		std::vector<seg>& segs
	) -> void;
}

consteval auto gse::trace::strip_function_signature(const std::string_view fn) -> std::string_view {
	std::string_view name = fn;

	if (const auto lp = name.find('('); lp != std::string_view::npos) {
		name = name.substr(0, lp);
	}

	while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
		name.remove_suffix(1);
	}

	if (const auto last_col_col = name.rfind("::"); last_col_col != std::string_view::npos) {
		std::size_t start = name.rfind(' ', last_col_col);
		start = (start == std::string_view::npos) ? 0 : start + 1;
		name = name.substr(start);

		constexpr std::string_view candidates[] = { "__cdecl", "__stdcall", "__thiscall", "__vectorcall", "cdecl", "stdcall", "thiscall", "vectorcall" };
		for (const auto cc : candidates) {
			if (name.size() > cc.size() && name.starts_with(cc)) {
				name.remove_prefix(cc.size());
				while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
					name.remove_prefix(1);
				}
				break;
			}
		}
	}
	else if (const auto last_sp = name.rfind(' '); last_sp != std::string_view::npos) {
		name = name.substr(last_sp + 1);
	}

	return name;
}

consteval auto gse::trace::current_loc_tag(const std::source_location loc) -> fixed_string<max_loc_tag_length> {
	fixed_string<max_loc_tag_length> out{};
	std::size_t at = 0;

	for (const char c : strip_function_signature(loc.function_name())) {
		if (at + 1 >= max_loc_tag_length) {
			break;
		}
		out.data[at++] = c;
	}

	if (at + 1 < max_loc_tag_length) {
		out.data[at++] = ':';
	}

	char digits[16]{};
	std::size_t digit_count = 0;
	auto line = loc.line();

	do {
		digits[digit_count++] = static_cast<char>('0' + line % 10);
		line /= 10;
	} while (line != 0 && digit_count < sizeof(digits));

	while (digit_count > 0 && at + 1 < max_loc_tag_length) {
		out.data[at++] = digits[--digit_count];
	}

	return out;
}

auto gse::trace::frame_view::child_indices(const node& n) const -> std::span<const std::uint32_t> {
	return children.subspan(n.children_first, n.children_count);
}
