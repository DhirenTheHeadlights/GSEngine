module;

#include <moodycamel/concurrentqueue.h>
#include <meta>

export module gse.concurrency:task;

import std;

import gse.core;
import gse.diag;
import gse.log;

export namespace gse {
	using job = std::function<void()>;
}

namespace gse::task {
	template <typename F>
	using first_arg_t = typename [: std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<F>::operator())[0]) :];

	using parallel_for_fn = move_only_function<void(std::size_t)>;

	auto parallel_for_impl(
		std::size_t first,
		std::size_t last,
		parallel_for_fn func,
		id id
	) -> void;
}

export namespace gse::task {
	class group;

	template <typename F>
	auto start(
		F&& fn = [] {},
		std::size_t worker_count = std::thread::hardware_concurrency()
	) -> std::invoke_result_t<F&>;

	auto post(
		job j,
		id id = trace::make_loc_id(std::source_location::current())
	) -> void;

	template <std::input_iterator It>
	auto post_range(
		It first,
		It last,
		id id = trace::make_loc_id(std::source_location::current())
	) -> void;

	template <typename F>
	auto parallel_for(
		first_arg_t<F> first,
		first_arg_t<F> last,
		F&& func,
		id id = trace::make_loc_id(std::source_location::current())
	) -> void;

	auto thread_count(
	) -> std::size_t;

	auto current_worker(
	) noexcept -> std::optional<std::size_t>;

	auto wait_idle(
	) -> void;

	auto in_arena(
		const std::function<void()>& fn
	) -> void;

	auto parallel_invoke_range(
		std::size_t first,
		std::size_t last,
		std::function<void(std::size_t)> func,
		id id = trace::make_loc_id(std::source_location::current())
	) -> void;

	class group : non_copyable, non_movable {
	public:
		explicit group(
			id label = trace::make_loc_id(std::source_location::current())
		);

		~group() noexcept override;

		auto post(
			job j,
			id id = trace::make_loc_id(std::source_location::current())
		) -> void;

		template <std::input_iterator It>
		auto post_range(
			It first,
			It last,
			id id = trace::make_loc_id(std::source_location::current())
		) -> void;

		auto wait(
		) const -> void;

	private:
		friend struct job_entry;
		friend auto run_job(
			struct job_entry& entry
		) -> void;
		friend auto submit_to_group(
			group& gp,
			job j,
			id trace_id,
			std::uint64_t parent_eid
		) -> void;

		id m_label;
		std::uint64_t m_outer_parent = 0;
		std::uint64_t m_parent_eid = 0;
		std::atomic<std::size_t> m_counter{ 0 };
	};

	template <typename T>
	class concurrent_queue {
	public:
		auto push(
			T value
		) -> void;

		auto try_pop(
			T& out
		) -> bool;

		auto drain(
		) -> std::vector<T>;

	private:
		moodycamel::ConcurrentQueue<T> m_queue;
	};
}

template <typename T>
auto gse::task::concurrent_queue<T>::push(T value) -> void {
	m_queue.enqueue(std::move(value));
}

template <typename T>
auto gse::task::concurrent_queue<T>::try_pop(T& out) -> bool {
	return m_queue.try_dequeue(out);
}

template <typename T>
auto gse::task::concurrent_queue<T>::drain() -> std::vector<T> {
	std::vector<T> result;
	T value;
	while (m_queue.try_dequeue(value)) {
		result.push_back(std::move(value));
	}
	return result;
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

	inline std::atomic started{ false };
	inline std::atomic stopping{ false };
	inline std::atomic<std::size_t> in_flight{ 0 };
	inline std::atomic<std::size_t> worker_count_value{ 0 };

	inline std::vector<std::jthread> workers;
	inline moodycamel::ConcurrentQueue<job_entry> submission_queue;
	inline std::counting_semaphore work_available{ 0 };

	inline std::mutex idle_mutex;
	inline std::condition_variable idle_cv;

	inline thread_local std::optional<std::size_t> t_worker_index;

	inline constexpr std::size_t coalesce_threshold = 64;
	inline constexpr std::size_t min_chunks_per_worker = 4;

	auto run_job(
		job_entry& entry
	) -> void;

	auto worker_loop(
		const std::stop_token& st,
		std::size_t index
	) -> void;

	auto submit_async(
		job j,
		id trace_id,
		std::uint64_t parent_eid
	) -> void;

	auto submit_to_group(
		group& gp,
		job j,
		id trace_id,
		std::uint64_t parent_eid
	) -> void;

	auto pool_start(
		std::size_t worker_count
	) -> void;

	auto pool_shutdown(
	) -> void;

	auto likely_idle(
	) noexcept -> bool;

	auto async_key_for(
		const void* p
	) -> std::uint64_t;

	auto compute_chunk_size(
		std::size_t n,
		std::size_t workers
	) -> std::size_t;
}

gse::task::group::group(const id label) : m_label(label) {
	m_outer_parent = trace::current_eid();
	m_parent_eid = trace::begin_block(m_label, m_outer_parent);
}

gse::task::group::~group() noexcept {
	wait();
	trace::end_block(m_label, m_parent_eid, m_outer_parent);
}

auto gse::task::group::wait() const -> void {
	while (m_counter.load(std::memory_order_acquire) > 0) {
		if (job_entry entry; submission_queue.try_dequeue(entry)) {
			run_job(entry);
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
			trace::scope(generate_id("task.start.reentrant"), [&] {
				fn();
			});
			return;
		}
		else {
			std::invoke_result_t<F&> r{};
			trace::scope(generate_id("task.start.reentrant"), [&] {
				r = fn();
			});
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
		trace::scope(generate_id("task.start.body"), [&] {
			fn();
		});
		return;
	}
	else {
		using r = std::invoke_result_t<F&>;
		r ret{};
		trace::scope(generate_id("task.start.body"), [&] {
			ret = fn();
		});
		return ret;
	}
}

auto gse::task::post(job j, const id id) -> void {
	submit_async(std::move(j), id, trace::current_eid());
}

template <std::input_iterator It>
auto gse::task::post_range(It first, It last, const id id) -> void {
	const std::uint64_t parent_eid = trace::current_eid();
	for (; first != last; ++first) {
		submit_async(job(*first), id, parent_eid);
	}
}

auto gse::task::parallel_for_impl(const std::size_t first, const std::size_t last, parallel_for_fn func, const id id) -> void {
	if (last <= first) {
		return;
	}

	const std::size_t n = last - first;

	trace::scope(id, [&] {
		if (n <= coalesce_threshold) {
			for (std::size_t i = first; i < last; ++i) {
				trace::scope(id, [&] {
					func(i);
				});
			}
			return;
		}

		const std::size_t workers = std::max<std::size_t>(1, worker_count_value.load(std::memory_order_acquire));
		const std::size_t chunk = compute_chunk_size(n, workers);

		group g(id);
		for (std::size_t chunk_start = first; chunk_start < last; chunk_start += chunk) {
			const std::size_t chunk_stop = std::min(chunk_start + chunk, last);
			g.post([chunk_start, chunk_stop, id, &func] {
				trace::scope(id, [&] {
					for (std::size_t i = chunk_start; i < chunk_stop; ++i) {
						trace::scope(id, [&] {
							func(i);
						});
					}
				});
			}, id);
		}
		g.wait();
	});
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

template <std::input_iterator It>
auto gse::task::group::post_range(It first, It last, const id id) -> void {
	for (; first != last; ++first) {
		this->post(job(*first), id);
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

auto gse::task::in_arena(const std::function<void()>& fn) -> void {
	fn();
}

auto gse::task::parallel_invoke_range(const std::size_t first, const std::size_t last, std::function<void(std::size_t)> func, const id id) -> void {
	if (last <= first) {
		return;
	}

	parallel_for_impl(
		first,
		last,
		parallel_for_fn([f = std::move(func)](std::size_t i) mutable {
			f(i);
		}),
		id
	);
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
			entry.gp->m_counter.fetch_sub(1, std::memory_order_release);
		}
	});

	if (entry.async_trace) {
		trace::end_async(entry.trace_id, entry.async_key);
	}

	try {
		trace::scope(entry.trace_id, [&] {
			entry.fn();
		}, entry.parent_eid);
	}
	catch (const std::exception& e) {
		log::println(log::level::error, log::category::task, "Exception in task: {}", e.what());
	}
	catch (...) {
		log::println(log::level::error, log::category::task, "Exception in task");
	}
}

auto gse::task::worker_loop(const std::stop_token& st, std::size_t index) -> void {
	t_worker_index = index;

	while (!st.stop_requested()) {
		work_available.acquire();
		if (st.stop_requested()) {
			return;
		}

		if (job_entry entry; submission_queue.try_dequeue(entry)) {
			run_job(entry);
		}
	}
}

auto gse::task::submit_async(job j, const id trace_id, const std::uint64_t parent_eid) -> void {
	in_flight.fetch_add(1, std::memory_order_relaxed);

	const auto key = async_key_for(&j);
	trace::begin_async(trace_id, key);

	job_entry entry{
		.fn = move_only_function<void()>(std::move(j)),
		.trace_id = trace_id,
		.parent_eid = parent_eid,
		.async_key = key,
		.async_trace = true,
		.counts_in_flight = true,
		.gp = nullptr,
	};

	submission_queue.enqueue(std::move(entry));
	work_available.release();
}

auto gse::task::submit_to_group(group& gp, job j, const id trace_id, const std::uint64_t parent_eid) -> void {
	gp.m_counter.fetch_add(1, std::memory_order_relaxed);

	job_entry entry{
		.fn = move_only_function<void()>(std::move(j)),
		.trace_id = trace_id,
		.parent_eid = parent_eid,
		.async_key = 0,
		.async_trace = false,
		.counts_in_flight = false,
		.gp = &gp,
	};

	submission_queue.enqueue(std::move(entry));
	work_available.release();
}

auto gse::task::pool_start(const std::size_t worker_count) -> void {
	worker_count_value.store(worker_count, std::memory_order_release);
	workers.clear();

	const std::size_t background_workers = worker_count - 1;
	workers.reserve(background_workers);

	for (std::size_t i = 0; i < background_workers; ++i) {
		workers.emplace_back([i](std::stop_token st) {
			worker_loop(st, i);
		});
	}

	t_worker_index = worker_count - 1;
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
	worker_count_value.store(0, std::memory_order_release);
	t_worker_index.reset();
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
