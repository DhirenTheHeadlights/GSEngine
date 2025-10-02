module;

#include <tbb/concurrent_queue.h>

export module gse.utility:task;

import std;

import :scope_exit;

export namespace gse {
	using job = std::move_only_function<void()>;
}

export namespace gse::task {
	template <typename F>
	auto start(
		F&& fn = [] {},
		std::size_t worker_count = std::thread::hardware_concurrency()
	) -> std::invoke_result_t<F&>;

	auto post(
		job j,
		std::optional<std::size_t> worker_hint = std::nullopt
	) -> void;

	template <std::input_iterator It>
	auto post_range(
		It first,
		It last,
		std::optional<std::size_t> worker_hint = std::nullopt
	) -> void;

	template <typename Index, typename F>
	auto parallel_for(
		Index first,
		Index last,
		F&& func,
		std::optional<std::size_t> worker_hint = std::nullopt
	) -> void;

	auto wait_idle(
	) -> void;

	auto thread_count(
	) -> size_t;
}

namespace gse::task {
	using queue = tbb::concurrent_queue<job>;

	std::vector<std::unique_ptr<queue>> local_queues;
	queue global_queue;

	std::counting_semaphore<std::numeric_limits<std::int32_t>::max()> ready_semaphore{ 0 };

	std::vector<std::jthread> workers;
	std::atomic started = false;
	std::atomic stopping = false;

	std::atomic<std::size_t> in_flight{ 0 };

	std::mutex idle;
	std::condition_variable idle_cv;

	std::size_t chunk_size = 256;
	std::size_t coalesce_threshold = 64;

	thread_local std::optional<std::size_t> local_worker_id = std::nullopt;

	auto current_worker_id(
	) noexcept -> std::optional<std::size_t>;

	auto likely_idle(
	) noexcept -> bool;

	auto try_take(
		std::size_t self_id,
		job& out_job
	) -> bool;

	auto complete_one(
	) -> void;

	auto worker_loop(
		std::size_t self_id
	) -> void;
}

template <typename F>
auto gse::task::start(F&& fn, std::size_t worker_count) -> std::invoke_result_t<F&> {
	if (started.load(std::memory_order_acquire)) {
		if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
			fn();
			return;
		}
		else {
			auto r = fn();
			return r;
		}
	}

	if (started.exchange(true)) {
		return;
	}

	if (worker_count == 0) {
		worker_count = 1;
	}

	local_queues.resize(worker_count);
	for (auto& q_ptr : local_queues) {
		q_ptr = std::make_unique<queue>();
	}

	workers.reserve(worker_count);
	for (std::size_t i = 0; i < worker_count; ++i) {
		workers.emplace_back([i] {
			worker_loop(i);
		});
	}

	auto guard = make_scope_exit([] {
		if (!started.load()) {
			return;
		}

		if (stopping.exchange(true)) {
			return;
		}

		ready_semaphore.release(static_cast<std::ptrdiff_t>(workers.size()));

		for (auto& t : workers) {
			if (t.joinable()) t.join();
		}

		workers.clear();

		stopping.store(false, std::memory_order_release); 
		started.store(false, std::memory_order_release);
	});

	if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
        fn();
    } else {
        return fn();
    }

	return;
}

auto gse::task::post(job j, const std::optional<std::size_t> worker_hint) -> void {
	in_flight.fetch_add(1, std::memory_order_relaxed);

	if (const auto id = current_worker_id(); id.has_value()) {
		local_queues[*id]->push(std::move(j));
	} else if (worker_hint && *worker_hint < local_queues.size()) {
		local_queues[*worker_hint]->push(std::move(j));
	} else {
		global_queue.push(std::move(j));
	}

	ready_semaphore.release();
}

template <std::input_iterator It>
auto gse::task::post_range(It first, It last, const std::optional<std::size_t> worker_hint) -> void {
	const std::size_t n = static_cast<std::size_t>(std::distance(first, last));

	if (n == 0) {
		return;
	}
	in_flight.fetch_add(n, std::memory_order_relaxed);

	if (const auto id = current_worker_id(); id.has_value()) {
		auto* q = local_queues[*id].get();
		for (; first != last; ++first) {
			q->push(job(*first));
		}
	} else if (worker_hint && *worker_hint < local_queues.size()) {
		auto* q = local_queues[*worker_hint].get();
		for (; first != last; ++first) {
			q->push(job(*first));
		}
	} else {
		for (; first != last; ++first) {
			global_queue.push(job(*first));
		}
	}

	ready_semaphore.release(static_cast<std::ptrdiff_t>(n));
}

template <typename Index, typename F>
auto gse::task::parallel_for(Index first, Index last, F&& func, std::optional<std::size_t> worker_hint) -> void {
	if (last <= first) {
		return;
	}

	const Index n = last - first;
	if (n <= static_cast<Index>(coalesce_threshold)) {
		post(
			[=, fn = std::forward<F>(func)]() mutable {
				for (Index i = first; i < last; ++i) {
					fn(i);
				}
			}, 
			worker_hint
		);

		return;
	}

	const Index c = static_cast<Index>(chunk_size);

	for (Index s = first; s < last; s += c) {
		const Index e = std::min(s + c, last);
		post(
			[s, e, fn = std::forward<F>(func)]() mutable {
				for (Index i = s; i < e; ++i) {
					fn(i);
				}
			}, 
			worker_hint
		);
	}
}

auto gse::task::wait_idle() -> void {
	for (int i = 0; i < 1024; ++i) {
		if (likely_idle()) return;
		std::this_thread::yield();
	}

	std::unique_lock lk(idle);
	idle_cv.wait(lk, [] {
		return likely_idle();
	});
}

auto gse::task::thread_count() -> size_t {
	return local_queues.size();
}

auto gse::task::current_worker_id() noexcept -> std::optional<std::size_t> {
	return local_worker_id;
}

auto gse::task::likely_idle() noexcept -> bool {
	return in_flight.load(std::memory_order_acquire) == 0;
}

auto gse::task::try_take(const std::size_t self_id, job& out_job) -> bool {
	if (local_queues[self_id]->try_pop(out_job)) {
		return true;
	}

	if (global_queue.try_pop(out_job)) {
		return true;
	}

	const std::size_t n = local_queues.size();

	for (std::size_t k = 1; k < n; ++k) {
		if (const std::size_t victim = (self_id + k) % n; local_queues[victim]->try_pop(out_job)) {
			return true;
		}
	}

	return false;
}

auto gse::task::complete_one() -> void {
	if (in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		std::scoped_lock lk(idle);
		idle_cv.notify_all();
	}
}

auto gse::task::worker_loop(std::size_t self_id) -> void {
	local_worker_id = self_id;

	while (!stopping.load(std::memory_order_relaxed)) {
		ready_semaphore.acquire();
		job j;
		int spins = 0;

		while (try_take(self_id, j)) {
			j();
			complete_one();

			if (++spins < 256) {
				continue;
			}

			spins = 0;
			std::this_thread::yield();
		}
	}

	job j;
	while (global_queue.try_pop(j)) {
		j(); complete_one();
	}

	for (const auto& q : local_queues) {
		while (q->try_pop(j)) {
			j(); complete_one();
		}
	}

	local_worker_id.reset();
}