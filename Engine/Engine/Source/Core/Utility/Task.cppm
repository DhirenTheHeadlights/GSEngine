module;

#include <oneapi/tbb.h>

export module gse.utility:task;

import std;

import :scope_exit;
import :lambda_traits;
import :id;
import :trace;

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
		job j
	) -> void;

	template <std::input_iterator It>
	auto post_range(
		It first,
		It last
	) -> void;

	template <typename F>
	auto parallel_for(
		first_arg_t<F> first,
		first_arg_t<F> last,
		F&& func
	) -> void;

	auto thread_count(
	) -> size_t;

	auto current_worker(
	) noexcept -> std::optional<std::size_t>;

	auto wait_idle(
	) -> void;
}

namespace gse::task {
	std::unique_ptr<tbb::task_arena> arena;
	std::unique_ptr<tbb::global_control> control;

	std::atomic started = false;
	std::atomic stopping = false;

	std::atomic<std::size_t> in_flight{ 0 };

	std::mutex idle;
	std::condition_variable idle_cv;

	std::size_t chunk_size = 256;
	std::size_t coalesce_threshold = 64;

	id post_id = generate_id("task.post");
	id post_item_id = generate_id("task.post_item");
	id post_range_id = generate_id("task.post_range");
	id parallel_for_id = generate_id("task.parallel_for");
	id parallel_for_block_id = generate_id("task.parallel_for_block");

	thread_local std::optional<std::size_t> local_worker_id = std::nullopt;

	auto current_worker_id(
	) noexcept -> std::optional<std::size_t>;

	auto likely_idle(
	) noexcept -> bool;

	auto async_key(
		const void* p
	) -> std::uint64_t;
}

template <typename F>
auto gse::task::start(F&& fn, std::size_t worker_count) -> std::invoke_result_t<F&> {
	if (worker_count < 2) {
		worker_count = 2;
	}

	if (started.load(std::memory_order_acquire)) {
		trace::scope(generate_id("task.start.reentrant"), [&] {
			if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
				fn();
			}
			else {
				auto r = fn();
				return r;
			}
		});
	}

	if (started.exchange(true)) {
		return;
	}

	arena.reset();
	control.reset();

	arena = std::make_unique<tbb::task_arena>(static_cast<int>(worker_count));
	control = std::make_unique<tbb::global_control>(
		tbb::global_control::max_allowed_parallelism,
		static_cast<int>(worker_count)
	);

	auto guard = make_scope_exit([] {
		if (!started.load()) return;
		if (stopping.exchange(true)) return;

		wait_idle();
		control.reset();
		arena.reset();

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

auto gse::task::post(job j) -> void {
	in_flight.fetch_add(1, std::memory_order_relaxed);

	auto p = std::make_shared<job>(std::move(j));

	const auto key = async_key(p.get());
	const auto id = trace::make_loc_id();

	trace::begin_async(id, key);

	const auto parent = trace::current_eid();

	arena->enqueue([p, id, key, parent] {
		trace::end_async(id, key);

		auto done = make_scope_exit([] {
			if (in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				std::scoped_lock lk(idle);
				idle_cv.notify_all();
			}
		});
		try {
			trace::scope(id, [&] {
				(*p)();
			}, parent);
		}
		catch (const std::exception& e) {
			std::println("Exception in task: {}", e.what());
		}
		catch (...) {
			std::println("Exception in task");
		}
	});
}

template <std::input_iterator It>
auto gse::task::post_range(It first, It last) -> void {
	const std::size_t n = static_cast<std::size_t>(std::distance(first, last));
	if (n == 0) {
		return;
	}

	in_flight.fetch_add(n, std::memory_order_relaxed);

	for (; first != last; ++first) {
		auto p = std::make_shared<job>(job(*first));

		const auto key = async_key(p.get());
		const auto id = trace::make_loc_id();

		trace::begin_async(id, key);

		const auto parent = trace::current_eid();

		arena->enqueue([p, id, key, parent] {
			trace::end_async(id, key);

			auto done = make_scope_exit([] {
				if (in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
					std::scoped_lock lk(idle);
					idle_cv.notify_all();
				}
			});
			try {
				trace::scope(id, [&] {
					(*p)();
				}, parent);
			}
			catch (const std::exception& e) {
				std::println("Exception in task: {}", e.what());
			}
			catch (...) {
				std::println("Exception in task");
			}
		});
	}
}

template <typename F>
auto gse::task::parallel_for(first_arg_t<F> first, first_arg_t<F> last, F&& func) -> void {
	using index = first_arg_t<F>;

	if (last <= first) return;

	const index n = last - first;
	const id id = trace::make_loc_id();

	trace::scope(id, [&] {
		if (n <= static_cast<index>(coalesce_threshold)) {
			for (index i = first; i < last; ++i) func(i);
			return;
		}

		const auto parent = trace::current_eid();

		arena->execute([&] {
			tbb::parallel_for(
				tbb::blocked_range<index>(first, last, static_cast<index>(chunk_size)),
				[&](const tbb::blocked_range<index>& r) {
					trace::scope(id, [&] {
						for (index i = r.begin(); i != r.end(); ++i) {
							trace::scope(id, [&] {
								func(i);
							}, parent);
						}
					});
				}
			);
		});
	});
}

auto gse::task::thread_count() -> size_t {
	if (arena) {
		return static_cast<size_t>(arena->max_concurrency());
	}

	return std::max<std::size_t>(2, std::thread::hardware_concurrency());
}

auto gse::task::current_worker() noexcept -> std::optional<std::size_t> {
	const auto n = thread_count();
	if (n == 0) return 0;

	if (const auto id = current_worker_id(); id.has_value()) {
		return *id % n;
	}

	const auto h = std::hash<std::thread::id>{}(std::this_thread::get_id());
	return h % n;
}

auto gse::task::wait_idle() -> void {
	for (int i = 0; i < 1024; ++i) {
		if (likely_idle()) {
			return;
		}
		std::this_thread::yield();
	}

	std::unique_lock lk(idle);
	idle_cv.wait(lk, [] {
		return likely_idle();
	});
}

auto gse::task::current_worker_id() noexcept -> std::optional<std::size_t> {
	return local_worker_id;
}

auto gse::task::likely_idle() noexcept -> bool {
	return in_flight.load(std::memory_order_acquire) == 0;
}

auto gse::task::async_key(const void* p) -> std::uint64_t {
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
