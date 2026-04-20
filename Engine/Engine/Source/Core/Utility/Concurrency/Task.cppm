module;


#include <new>
#include <oneapi/tbb.h>

export module gse.utility:task;

import std;

import :scope_exit;
import :lambda_traits;
import :id;
import :move_only_function;
import :trace;

import gse.log;

export namespace gse {
	using job = std::function<void()>;
}

namespace gse::task {
	using parallel_for_fn = gse::move_only_function<void(std::size_t)>;

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
	) -> size_t;

	auto current_worker(
	) noexcept -> std::optional<std::size_t>;

	auto wait_idle(
	) -> void;

	template <typename F>
	auto isolate(
		F&& fn
	) -> std::invoke_result_t<F&>;

	auto in_arena(
		std::function<void()> fn
	) -> void;

	auto parallel_invoke_range(
		std::size_t first,
		std::size_t last,
		std::function<void(std::size_t)> func
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
		) -> void;

	private:
		id m_label;
		std::uint64_t m_outer_parent = 0;
		std::uint64_t m_parent_eid = 0;
		tbb::task_group m_tbb_group;
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
		tbb::concurrent_queue<T> m_queue;
	};
}

template <typename T>
auto gse::task::concurrent_queue<T>::push(T value) -> void {
	m_queue.push(std::move(value));
}

template <typename T>
auto gse::task::concurrent_queue<T>::try_pop(T& out) -> bool {
	return m_queue.try_pop(out);
}

template <typename T>
auto gse::task::concurrent_queue<T>::drain() -> std::vector<T> {
	std::vector<T> result;
	T value;
	while (m_queue.try_pop(value)) {
		result.push_back(std::move(value));
	}
	return result;
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

	struct parent {
		std::uint64_t forced_eid = 0;
	};

	auto enqueue(
		job j,
		id id,
		parent parent
	) -> void;

	auto likely_idle(
	) noexcept -> bool;

	auto async_key(
		const void* p
	) -> std::uint64_t;
}

gse::task::group::group(const id label) : m_label(label) {
	m_outer_parent = trace::current_eid();
	m_parent_eid = trace::begin_block(m_label, m_outer_parent);
}

gse::task::group::~group() noexcept {
	wait();
	trace::end_block(m_label, m_parent_eid, m_outer_parent);
}

auto gse::task::group::wait() -> void {
	const bool already_in_arena = tbb::this_task_arena::current_thread_index() != tbb::task_arena::not_initialized;
	if (arena && !already_in_arena) {
		arena->execute([this] {
			m_tbb_group.wait();
		});
	}
	else {
		m_tbb_group.wait();
	}
}

auto gse::task::group::post(job j, const id id) -> void {
	const std::uint64_t parent_eid = m_parent_eid;

	auto body = [j = std::move(j), id, parent_eid] {
		try {
			trace::scope(id, [&] {
				j();
			}, parent_eid);
		}
		catch (const std::exception& e) {
			log::println(log::level::error, log::category::task, "Exception in task: {}", e.what());
		}
		catch (...) {
			log::println(log::level::error, log::category::task, "Exception in task");
		}
	};

	const bool already_in_arena = tbb::this_task_arena::current_thread_index() != tbb::task_arena::not_initialized;
	if (arena && !already_in_arena) {
		arena->execute([this, body = std::move(body)] {
			m_tbb_group.run(body);
		});
	}
	else {
		m_tbb_group.run(std::move(body));
	}
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
			} else {
				auto r = fn();
				return r;
			}
		});
	}

	if (started.exchange(true)) {
		if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
			return;
		} else {
			return {};
		}
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
	} else {
		using r = std::invoke_result_t<F&>;
		r ret{};
		trace::scope(generate_id("task.start.body"), [&] {
			ret = fn();
		});
		return ret;
	}
}

auto gse::task::post(job j, const id id) -> void {
	const parent parent{
		.forced_eid = trace::current_eid()
	};

	enqueue(std::move(j), id, parent);
}

template <std::input_iterator It>
auto gse::task::post_range(It first, It last, const id id) -> void {
	for (; first != last; ++first) {
		parent parent{
			.forced_eid = trace::current_eid()
		};

		enqueue(job(*first), id, std::move(parent));
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

		arena->execute([&] {
			tbb::parallel_for(
				tbb::blocked_range(first, last, chunk_size),
				[&](const tbb::blocked_range<std::size_t>& r) {
					trace::scope(id, [&] {
						for (std::size_t i = r.begin(); i != r.end(); ++i) {
							trace::scope(id, [&] {
								func(i);
							});
						}
					});
				}
			);
		});
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

auto gse::task::thread_count() -> size_t {
	if (arena) {
		return static_cast<size_t>(arena->max_concurrency());
	}

	return std::max<std::size_t>(2, std::thread::hardware_concurrency());
}

auto gse::task::current_worker() noexcept -> std::optional<std::size_t> {
	const auto idx = tbb::this_task_arena::current_thread_index();
	if (idx == tbb::task_arena::not_initialized) {
		return std::nullopt;
	}
	return static_cast<std::size_t>(idx);
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

template <typename F>
auto gse::task::isolate(F&& fn) -> std::invoke_result_t<F&> {
	return tbb::this_task_arena::isolate(std::forward<F>(fn));
}

auto gse::task::in_arena(std::function<void()> fn) -> void {
	const bool already_in_arena = tbb::this_task_arena::current_thread_index() != tbb::task_arena::not_initialized;
	if (arena && !already_in_arena) {
		arena->execute(fn);
	}
	else {
		fn();
	}
}

auto gse::task::parallel_invoke_range(const std::size_t first, const std::size_t last, std::function<void(std::size_t)> func) -> void {
	if (last <= first) {
		return;
	}

	in_arena([&] {
		tbb::parallel_for(
			tbb::blocked_range<std::size_t>(first, last, 1),
			[&](const tbb::blocked_range<std::size_t>& r) {
				for (std::size_t i = r.begin(); i != r.end(); ++i) {
					func(i);
				}
			}
		);
	});
}

auto gse::task::enqueue(job j, id id, parent parent) -> void {
	in_flight.fetch_add(1, std::memory_order_relaxed);

	const auto key = async_key(&j);
	trace::begin_async(id, key);

	arena->enqueue([j = std::move(j), id, key, parent = std::move(parent)] {
		trace::end_async(id, key);

		auto flight_done = make_scope_exit([] {
			if (in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				std::scoped_lock lk(idle);
				idle_cv.notify_all();
			}
		});

		const std::uint64_t parent_eid = parent.forced_eid;

		try {
			trace::scope(id, [&] {
				j();
			}, parent_eid);
		} catch (const std::exception& e) {
			log::println(log::level::error, log::category::task, "Exception in task: {}", e.what());
		} catch (...) {
			log::println(log::level::error, log::category::task, "Exception in task");
		}
	});
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
