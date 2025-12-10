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

	class group : non_copyable, non_movable {
	public:
		explicit group(
			id label = trace::make_loc_id(std::source_location::current())
		);

		~group() override;

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
	private:
		id m_label;
	    std::uint64_t m_outer_parent = 0;
	    std::uint64_t m_parent_eid   = 0;
	    std::atomic<std::size_t> m_task_count{0};
	    bool m_waited = false;
	};
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

	thread_local std::optional<std::size_t> local_worker_id = std::nullopt;

	struct parent {
		std::uint64_t forced_eid = 0;
	};

	auto enqueue(
		std::shared_ptr<job> p,
		id id,
		parent parent,
		std::atomic<std::size_t>* task_count
	) -> void;

	auto current_worker_id(
	) noexcept -> std::optional<std::size_t>;

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

gse::task::group::~group() {
	while (m_task_count.load(std::memory_order_acquire) > 0) {
		std::this_thread::yield();
	}

    trace::end_block(m_label, m_parent_eid, m_outer_parent);
}

auto gse::task::group::post(job j, const id id) -> void {
	m_task_count.fetch_add(1, std::memory_order_relaxed);

	auto p = std::make_shared<job>(std::move(j));
	const parent parent{
		.forced_eid = m_parent_eid
	};

    enqueue(std::move(p), id, parent, &m_task_count);
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
		if constexpr (std::is_void_v<std::invoke_result_t<F&>>) {
			return;
		}
		else {
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
	auto p = std::make_shared<job>(std::move(j));

	const parent parent{
		.forced_eid = trace::current_eid()
	};

	enqueue(
		std::move(p),
		id,
		parent,
		nullptr
	);
}

template <std::input_iterator It>
auto gse::task::post_range(It first, It last, const id id) -> void {
	for (; first != last; ++first) {
		auto p = std::make_shared<job>(job(*first));

		parent parent{
			.forced_eid = trace::current_eid()
		};

		enqueue(std::move(p), id, std::move(parent), nullptr);
	}
}

template <typename F>
auto gse::task::parallel_for(first_arg_t<F> first, first_arg_t<F> last, F&& func, id id) -> void {
	using index = first_arg_t<F>;

	if (last <= first) {
		return;
	}

	const index n = last - first;

	trace::scope(id, [&] {
		if (n <= static_cast<index>(coalesce_threshold)) {
			for (index i = first; i < last; ++i) {
				trace::scope(id, [&]{
					func(i);
				});
			}
			return;
		}

		arena->execute([&] {
			tbb::parallel_for(
				tbb::blocked_range<index>(first, last, static_cast<index>(chunk_size)),
				[&](const tbb::blocked_range<index>& r) {
					trace::scope(id, [&] {
						for (index i = r.begin(); i != r.end(); ++i) {
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

auto gse::task::enqueue(std::shared_ptr<job> p, id id, parent parent, std::atomic<std::size_t>* task_count) -> void {
	in_flight.fetch_add(1, std::memory_order_relaxed);

	const auto key = async_key(p.get());
	trace::begin_async(id, key);

	arena->enqueue([p, id, key, parent = std::move(parent), task_count] {
		trace::end_async(id, key);

		auto group_done = make_scope_exit([&]{
			if (task_count) {
				task_count->fetch_sub(1, std::memory_order_acq_rel);
			}
		});
		auto flight_done = make_scope_exit([]{
			if (in_flight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				std::scoped_lock lk(idle);
				idle_cv.notify_all();
			}
		});

		const std::uint64_t parent_eid = parent.forced_eid;

		try {
			trace::scope(id, [&] {
				(*p)();
			}, parent_eid);
		} catch (const std::exception& e) {
			std::println("Exception in task: {}", e.what());
		} catch (...) {
			std::println("Exception in task");
		}
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
