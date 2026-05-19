export module gse.concurrency:task;

import std;
import gse.std_meta;

import gse.core;
import gse.diag;
import gse.log;
import gse.stacktrace;

import :work_stealing_queue;

export namespace gse {
	using job = move_only_function<void()>;
}

namespace gse::task {
	template <typename F>
	using first_arg_t =
		typename[:std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<F>::operator())[0]):];

	using parallel_for_fn = move_only_function<void(std::size_t)>;

	auto parallel_for_impl(std::size_t first, std::size_t last, parallel_for_fn func, id id) -> void;
}

export namespace gse::task {
	class group;

	template <typename F>
	auto start(F&& fn, std::size_t worker_count = std::thread::hardware_concurrency()) -> std::invoke_result_t<F&>;

	auto post(job j, id id = trace::loc_id<trace::current_loc_tag()>()) -> void;

	template <std::forward_iterator It>
	auto post_range(It first, It last, id id = trace::loc_id<trace::current_loc_tag()>()) -> void;

	template <typename F>
	auto parallel_for(
		first_arg_t<F> first,
		first_arg_t<F> last,
		F&& func,
		id id = trace::loc_id<trace::current_loc_tag()>()
	) -> void;

	auto thread_count() -> std::size_t;

	auto current_worker() noexcept -> std::optional<std::size_t>;

	auto wait_idle() -> void;

	auto try_run_one() -> bool;

	auto parallel_invoke_range(
		std::size_t first,
		std::size_t last,
		move_only_function<void(std::size_t)> func,
		id id = trace::loc_id<trace::current_loc_tag()>()
	) -> void;

	template <typename Fn>
	auto coarse_parallel(
		std::size_t n,
		std::size_t min_chunk_items,
		Fn&& fn,
		id label = trace::loc_id<trace::current_loc_tag()>()
	) -> void;

	class group : non_copyable, non_movable {
	public:
		explicit group(id label = trace::loc_id<trace::current_loc_tag()>());

		~group() noexcept override;

		auto post(job j, id id = trace::loc_id<trace::current_loc_tag()>()) -> void;

		template <std::input_iterator It>
		auto post_range(It first, It last, id id = trace::loc_id<trace::current_loc_tag()>()) -> void;

		auto wait() const -> void;

	private:
		friend struct job_entry;
		friend auto run_job(struct job_entry& entry) -> void;
		friend auto submit_to_group(group& gp, job j, id trace_id, std::uint64_t parent_eid) -> void;

		id m_label;
		std::uint64_t m_outer_parent = 0;
		std::uint64_t m_parent_eid = 0;
		std::atomic<std::size_t> m_counter{ 0 };
		std::atomic<std::size_t> m_inflight_notifies{ 0 };
	};

	template <typename T>
	class concurrent_queue {
	public:
		auto push(T value) const -> void;

		auto try_pop(T& out) const -> bool;

		auto drain() const -> std::vector<T>;

		[[nodiscard]] auto size() const -> std::size_t;

	private:
		mutable std::mutex m_mutex;
		mutable std::queue<T> m_queue;
	};
}

template <typename T>
auto gse::task::concurrent_queue<T>::push(T value) const -> void {
	const std::scoped_lock lock(m_mutex);
	m_queue.push(std::move(value));
}

template <typename T>
auto gse::task::concurrent_queue<T>::try_pop(T& out) const -> bool {
	const std::scoped_lock lock(m_mutex);
	if (m_queue.empty()) {
		return false;
	}
	out = std::move(m_queue.front());
	m_queue.pop();
	return true;
}

template <typename T>
auto gse::task::concurrent_queue<T>::drain() const -> std::vector<T> {
	const std::scoped_lock lock(m_mutex);
	std::vector<T> result;
	result.reserve(m_queue.size());
	while (!m_queue.empty()) {
		result.push_back(std::move(m_queue.front()));
		m_queue.pop();
	}
	return result;
}

template <typename T>
auto gse::task::concurrent_queue<T>::size() const -> std::size_t {
	const std::scoped_lock lock(m_mutex);
	return m_queue.size();
}

namespace gse::task {
	struct job_entry {
		gse::move_only_function<void()> fn;
		id trace_id;
		std::uint64_t parent_eid = 0;
		std::uint64_t async_key = 0;
		bool async_trace = false;
		bool counts_in_flight = false;
		group* gp = nullptr;
	};

	struct local_queue {
		work_stealing_queue<job_entry> entries;
		std::mutex remote_mtx;
		std::deque<job_entry> remote_entries;
	};

	inline std::atomic started{ false };
	inline std::atomic stopping{ false };
	inline std::atomic<std::size_t> in_flight{ 0 };
	inline std::atomic<std::size_t> worker_count_value{ 0 };

	inline std::vector<std::jthread> workers;
	inline std::vector<std::unique_ptr<local_queue>> per_worker_queues;
	inline std::counting_semaphore work_available{ 0 };
	inline std::atomic<std::size_t> external_post_rotation{ 0 };

	inline std::mutex idle_mutex;
	inline std::condition_variable idle_cv;

	inline thread_local std::optional<std::size_t> t_worker_index;
	inline thread_local bool t_is_main_thread = false;

	inline constexpr std::size_t coalesce_threshold = 64;
	inline constexpr std::size_t min_chunks_per_worker = 4;
	inline constexpr std::size_t hot_spin_yields = 200;

	auto run_job(job_entry& entry) -> void;

	auto worker_loop(const std::stop_token& st, std::size_t index) -> void;

	auto submit_async(job j, id trace_id, std::uint64_t parent_eid) -> void;

	auto submit_to_group(group& gp, job j, id trace_id, std::uint64_t parent_eid) -> void;

	auto pool_start(std::size_t worker_count) -> void;

	auto pool_shutdown() -> void;

	auto likely_idle() noexcept -> bool;

	auto async_key_for(const void* p) -> std::uint64_t;

	auto compute_chunk_size(std::size_t n, std::size_t workers) -> std::size_t;

	auto try_pop_local(std::size_t worker_idx) -> std::optional<job_entry>;

	auto try_steal_from(std::size_t victim_idx) -> std::optional<job_entry>;

	auto try_pop_or_steal(std::optional<std::size_t> my_idx) -> std::optional<job_entry>;

	auto push_to_queue(std::size_t target_idx, job_entry&& entry) -> void;

	auto owns_queue(std::size_t target_idx) noexcept -> bool;

	auto select_post_target() -> std::size_t;
}

gse::task::group::group(const id label) : m_label(label) {
	m_outer_parent = trace::current_eid();
	m_parent_eid = trace::begin_block(m_label, m_outer_parent);
}

gse::task::group::~group() noexcept {
	wait();
	while (m_inflight_notifies.load(std::memory_order_acquire) > 0) {
		std::this_thread::yield();
	}
	trace::end_block(m_label, m_parent_eid, m_outer_parent);
}

auto gse::task::group::wait() const -> void {
	while (m_counter.load(std::memory_order_acquire) > 0) {
		if (auto entry = try_pop_or_steal(t_worker_index)) {
			run_job(*entry);
		}
		else {
			std::this_thread::yield();
		}
	}
}

auto gse::task::group::post(job j, const id id) -> void {
	submit_to_group(*this, std::move(j), id, m_parent_eid);
}

template <typename F>
auto gse::task::start(F&& fn, std::size_t worker_count) -> std::invoke_result_t<F&> {
	if (worker_count < 2) {
		worker_count = 2;
	}

	if (started.load(std::memory_order_acquire)) {
		if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
			{
				trace::scope_guard sg{ generate_id("task.start.reentrant") };
				fn();
			}
			return;
		}
		else {
			std::invoke_result_t<F&> r{};
			{
				trace::scope_guard sg{ generate_id("task.start.reentrant") };
				r = fn();
			}
			return r;
		}
	}

	if (started.exchange(true)) {
		if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
			return;
		}
		else {
			return {};
		}
	}

	pool_start(worker_count);

	auto guard = make_scope_exit([] {
		if (!started.load()) {
			return;
		}
		if (stopping.exchange(true)) {
			return;
		}

		wait_idle();
		pool_shutdown();

		stopping.store(false, std::memory_order_release);
		started.store(false, std::memory_order_release);
	});

	if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
		{
			trace::scope_guard sg{ generate_id("task.start.body") };
			fn();
		}
		return;
	}
	else {
		using r = std::invoke_result_t<F&>;
		r ret{};
		{
			trace::scope_guard sg{ generate_id("task.start.body") };
			ret = fn();
		}
		return ret;
	}
}

auto gse::task::post(job j, const id id) -> void {
	submit_async(std::move(j), id, trace::current_eid());
}

template <std::forward_iterator It>
auto gse::task::post_range(It first, It last, const id id) -> void {
	const std::size_t count = static_cast<std::size_t>(std::distance(first, last));
	if (count == 0) {
		return;
	}

	const std::uint64_t parent_eid = trace::current_eid();
	in_flight.fetch_add(count, std::memory_order_relaxed);

	for (auto it = first; it != last; ++it) {
		if (!*it) {
			log::println(
				log::level::error,
				log::category::task,
				"post_range: null job in input range (trace_id={})",
				id
			);
		}
		const auto key = async_key_for(&*it);
		trace::begin_async(id, key);
		push_to_queue(
			select_post_target(),
			job_entry{
				.fn = std::move(*it),
				.trace_id = id,
				.parent_eid = parent_eid,
				.async_key = key,
				.async_trace = true,
				.counts_in_flight = true,
				.gp = nullptr,
			}
		);
	}

	work_available.release(static_cast<std::ptrdiff_t>(count));
}

auto gse::task::parallel_for_impl(const std::size_t first, const std::size_t last, parallel_for_fn func, const id id)
	-> void {
	if (last <= first) {
		return;
	}

	const std::size_t n = last - first;

	{
		trace::scope_guard sg{ id };
		if (n <= coalesce_threshold) {
			for (std::size_t i = first; i < last; ++i) {
				func(i);
			}
			return;
		}

		const std::size_t workers = std::max<std::size_t>(1, worker_count_value.load(std::memory_order_acquire));
		const std::size_t chunk = compute_chunk_size(n, workers);

		group g(id);
		for (std::size_t chunk_start = first; chunk_start < last; chunk_start += chunk) {
			const std::size_t chunk_stop = std::min(chunk_start + chunk, last);
			g.post(
				[chunk_start, chunk_stop, &func] {
					for (std::size_t i = chunk_start; i < chunk_stop; ++i) {
						func(i);
					}
				},
				id
			);
		}
		g.wait();
	}
}

template <typename F>
auto gse::task::parallel_for(first_arg_t<F> first, first_arg_t<F> last, F&& func, const id id) -> void {
	using index = first_arg_t<F>;

	if (last <= first) {
		return;
	}

	parallel_for_impl(
		static_cast<std::size_t>(first),
		static_cast<std::size_t>(last),
		parallel_for_fn([f = std::forward<F>(func)](std::size_t i) mutable {
			f(static_cast<index>(i));
		}),
		id
	);
}

template <typename Fn>
auto gse::task::coarse_parallel(const std::size_t n, const std::size_t min_chunk_items, Fn&& fn, const id label)
	-> void {
	if (n == 0) {
		return;
	}
	if (n <= min_chunk_items) {
		for (std::size_t i = 0; i < n; ++i) {
			fn(i);
		}
		return;
	}
	const auto workers = std::max<std::size_t>(1, thread_count());
	const auto max_chunks = std::max<std::size_t>(1, n / min_chunk_items);
	const auto chunk_count = std::min(workers * 2, max_chunks);
	if (chunk_count <= 1) {
		for (std::size_t i = 0; i < n; ++i) {
			fn(i);
		}
		return;
	}
	const auto chunk = (n + chunk_count - 1) / chunk_count;

	group g{ label };
	for (std::size_t start = 0; start < n; start += chunk) {
		const auto end = std::min(start + chunk, n);
		g.post(
			[start, end, &fn] {
				for (std::size_t i = start; i < end; ++i) {
					fn(i);
				}
			},
			label
		);
	}
}

template <std::input_iterator It>
auto gse::task::group::post_range(It first, It last, const id id) -> void {
	for (; first != last; ++first) {
		this->post(std::move(*first), id);
	}
}

auto gse::task::thread_count() -> std::size_t {
	if (const auto value = worker_count_value.load(std::memory_order_acquire); value > 0) {
		return value;
	}
	return std::max<std::size_t>(2, std::thread::hardware_concurrency());
}

auto gse::task::current_worker() noexcept -> std::optional<std::size_t> {
	return t_worker_index;
}

auto gse::task::wait_idle() -> void {
	for (int i = 0; i < 1024; ++i) {
		if (likely_idle()) {
			return;
		}
		std::this_thread::yield();
	}

	std::unique_lock lk(idle_mutex);
	idle_cv.wait(lk, [] {
		return likely_idle();
	});
}

auto gse::task::try_run_one() -> bool {
	if (auto entry = try_pop_or_steal(t_worker_index)) {
		run_job(*entry);
		return true;
	}
	return false;
}

auto gse::task::try_pop_local(const std::size_t worker_idx) -> std::optional<job_entry> {
	if (worker_idx >= per_worker_queues.size()) {
		return std::nullopt;
	}
	auto& q = *per_worker_queues[worker_idx];
	job_entry entry;
	if (q.entries.try_pop(entry)) {
		return std::optional<job_entry>{ std::move(entry) };
	}

	std::unique_lock lk(q.remote_mtx, std::try_to_lock);
	if (!lk.owns_lock() || q.remote_entries.empty()) {
		return std::nullopt;
	}
	entry = std::move(q.remote_entries.back());
	q.remote_entries.pop_back();
	return std::optional<job_entry>{ std::move(entry) };
}

auto gse::task::try_steal_from(const std::size_t victim_idx) -> std::optional<job_entry> {
	if (victim_idx >= per_worker_queues.size()) {
		return std::nullopt;
	}
	auto& q = *per_worker_queues[victim_idx];
	job_entry entry;
	if (q.entries.try_steal(entry)) {
		return std::optional<job_entry>{ std::move(entry) };
	}

	std::unique_lock lk(q.remote_mtx, std::try_to_lock);
	if (!lk.owns_lock() || q.remote_entries.empty()) {
		return std::nullopt;
	}
	entry = std::move(q.remote_entries.front());
	q.remote_entries.pop_front();
	return std::optional<job_entry>{ std::move(entry) };
}

auto gse::task::try_pop_or_steal(const std::optional<std::size_t> my_idx) -> std::optional<job_entry> {
	const auto queue_count = per_worker_queues.size();
	if (queue_count == 0) {
		return std::nullopt;
	}

	if (my_idx.has_value() && *my_idx < queue_count) {
		if (auto entry = try_pop_local(*my_idx)) {
			return entry;
		}
	}

	const auto start = my_idx.value_or(0);
	for (std::size_t offset = 1; offset <= queue_count; ++offset) {
		const auto victim = (start + offset) % queue_count;
		if (my_idx.has_value() && victim == *my_idx) {
			continue;
		}
		if (auto entry = try_steal_from(victim)) {
			return entry;
		}
	}

	return std::nullopt;
}

auto gse::task::push_to_queue(const std::size_t target_idx, job_entry&& entry) -> void {
	auto& q = *per_worker_queues[target_idx];
	if (owns_queue(target_idx)) {
		q.entries.push(std::move(entry));
		return;
	}

	std::lock_guard lk(q.remote_mtx);
	q.remote_entries.push_back(std::move(entry));
}

auto gse::task::owns_queue(const std::size_t target_idx) noexcept -> bool {
	if (const auto worker = t_worker_index; worker.has_value()) {
		return *worker == target_idx;
	}
	return false;
}

auto gse::task::select_post_target() -> std::size_t {
	const auto queue_count = per_worker_queues.size();
	if (queue_count == 0) {
		return 0;
	}
	if (!t_is_main_thread) {
		if (const auto w = t_worker_index; w.has_value() && *w < queue_count) {
			return *w;
		}
	}
	return external_post_rotation.fetch_add(1, std::memory_order_relaxed) % queue_count;
}

auto gse::task::parallel_invoke_range(
	const std::size_t first,
	const std::size_t last,
	move_only_function<void(std::size_t)> func,
	const id id
) -> void {
	if (last <= first) {
		return;
	}

	const std::size_t n = last - first;

	trace::scope_guard sg{ id };

	if (n == 1) {
		func(first);
		return;
	}

	const std::size_t workers = std::max<std::size_t>(1, worker_count_value.load(std::memory_order_acquire));
	const std::size_t chunk = compute_chunk_size(n, workers);

	group g(id);
	for (std::size_t chunk_start = first; chunk_start < last; chunk_start += chunk) {
		const std::size_t chunk_stop = std::min(chunk_start + chunk, last);
		g.post(
			[chunk_start, chunk_stop, &func] {
				for (std::size_t i = chunk_start; i < chunk_stop; ++i) {
					func(i);
				}
			},
			id
		);
	}
	g.wait();
}

auto gse::task::run_job(job_entry& entry) -> void {
	auto on_exit = make_scope_exit([&] {
		if (entry.counts_in_flight) {
			if (in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				std::scoped_lock lk(idle_mutex);
				idle_cv.notify_all();
			}
		}
		if (entry.gp) {
			entry.gp->m_inflight_notifies.fetch_add(1, std::memory_order_acquire);
			entry.gp->m_counter.fetch_sub(1, std::memory_order_acq_rel);
			entry.gp->m_inflight_notifies.fetch_sub(1, std::memory_order_release);
		}
	});

	if (entry.async_trace) {
		trace::end_async(entry.trace_id, entry.async_key);
	}

	if (!entry.fn.is_invocable()) {
		log::println(
			log::level::error,
			log::category::task,
			"run_job: dequeued job_entry with non-invocable fn (trace_id={}, vtable={}, invoke={}, async_trace={}, "
			"counts_in_flight={}, has_group={})",
			entry.trace_id,
			entry.fn.vtable_address(),
			entry.fn.invoke_address(),
			entry.async_trace,
			entry.counts_in_flight,
			entry.gp != nullptr
		);
		return;
	}

	try {
		{
			trace::scope_guard sg{ entry.trace_id, entry.parent_eid };
			entry.fn();
		}
	}
	catch (const std::exception& e) {
		log::println(
			log::level::error,
			log::category::task,
			"Exception in task (trace_id={}, type={}): {}\nStack:\n{}",
			entry.trace_id,
			typeid(e).name(),
			e.what(),
			capture_stacktrace(1)
		);
	}
	catch (...) {
		log::println(
			log::level::error,
			log::category::task,
			"Unknown exception in task (trace_id={})\nStack:\n{}",
			entry.trace_id,
			capture_stacktrace(1)
		);
	}
}

auto gse::task::worker_loop(const std::stop_token& st, std::size_t index) -> void {
	t_worker_index = index;

	while (!st.stop_requested()) {
		while (auto entry = try_pop_or_steal(t_worker_index)) {
			run_job(*entry);
			if (st.stop_requested()) {
				return;
			}
		}

		bool found_in_spin = false;
		for (std::size_t i = 0; i < hot_spin_yields; ++i) {
			if (auto entry = try_pop_local(index)) {
				run_job(*entry);
				found_in_spin = true;
				break;
			}
			std::this_thread::yield();
		}
		if (found_in_spin) {
			continue;
		}

		if (auto entry = try_pop_or_steal(t_worker_index)) {
			run_job(*entry);
			continue;
		}

		work_available.acquire();
		if (st.stop_requested()) {
			return;
		}
	}
}

auto gse::task::submit_async(job j, const id trace_id, const std::uint64_t parent_eid) -> void {
	if (!j) {
		log::println(
			log::level::error,
			log::category::task,
			"submit_async: null job submitted (trace_id={})",
			trace_id
		);
	}
	in_flight.fetch_add(1, std::memory_order_relaxed);

	const auto key = async_key_for(&j);
	trace::begin_async(trace_id, key);

	push_to_queue(
		select_post_target(),
		job_entry{
			.fn = std::move(j),
			.trace_id = trace_id,
			.parent_eid = parent_eid,
			.async_key = key,
			.async_trace = true,
			.counts_in_flight = true,
			.gp = nullptr,
		}
	);
	work_available.release();
}

auto gse::task::submit_to_group(group& gp, job j, const id trace_id, const std::uint64_t parent_eid) -> void {
	gp.m_counter.fetch_add(1, std::memory_order_relaxed);

	push_to_queue(
		select_post_target(),
		job_entry{
			.fn = std::move(j),
			.trace_id = trace_id,
			.parent_eid = parent_eid,
			.async_key = 0,
			.async_trace = false,
			.counts_in_flight = false,
			.gp = &gp,
		}
	);
	work_available.release();
}

auto gse::task::pool_start(const std::size_t worker_count) -> void {
	worker_count_value.store(worker_count, std::memory_order_release);
	workers.clear();
	per_worker_queues.clear();
	external_post_rotation.store(0, std::memory_order_relaxed);

	per_worker_queues.reserve(worker_count);
	for (std::size_t i = 0; i < worker_count; ++i) {
		per_worker_queues.emplace_back(std::make_unique<local_queue>());
	}

	const std::size_t background_workers = worker_count - 1;
	workers.reserve(background_workers);

	for (std::size_t i = 0; i < background_workers; ++i) {
		workers.emplace_back([i](std::stop_token st) {
			worker_loop(st, i);
		});
	}

	t_worker_index = worker_count - 1;
	t_is_main_thread = true;
	trace::register_main_thread();
}

auto gse::task::pool_shutdown() -> void {
	for (auto& w : workers) {
		w.request_stop();
	}

	const auto count = worker_count_value.load(std::memory_order_acquire);
	const std::size_t background_workers = count > 0 ? count - 1 : std::size_t{ 0 };
	for (std::size_t i = 0; i < background_workers; ++i) {
		work_available.release();
	}

	workers.clear();
	per_worker_queues.clear();
	worker_count_value.store(0, std::memory_order_release);
	t_worker_index.reset();
	t_is_main_thread = false;
}

auto gse::task::likely_idle() noexcept -> bool {
	return in_flight.load(std::memory_order_acquire) == 0;
}

auto gse::task::async_key_for(const void* p) -> std::uint64_t {
	const std::uint64_t x = reinterpret_cast<std::uintptr_t>(p);

	auto mix = [](std::uint64_t v) {
		v += 0x9E3779B97F4A7C15ull;
		v = (v ^ (v >> 30)) * 0xBF58476D1CE4E5B9ull;
		v = (v ^ (v >> 27)) * 0x94D049BB133111EBull;
		v ^= (v >> 31);
		return v;
	};

	std::uint64_t k = mix(x);

	if (k == 0) {
		static std::atomic<std::uint64_t> seq{ 1 };
		k = mix(seq.fetch_add(1, std::memory_order_relaxed));
	}

	return k;
}

auto gse::task::compute_chunk_size(const std::size_t n, const std::size_t workers) -> std::size_t {
	const std::size_t target_chunks = workers * min_chunks_per_worker;
	if (target_chunks == 0) {
		return n;
	}
	return std::max<std::size_t>(1, (n + target_chunks - 1) / target_chunks);
}
