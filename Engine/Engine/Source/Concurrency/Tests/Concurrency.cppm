export module gse.tests.concurrency;

import std;

import gse.concurrency;
import gse.log;
import gse.math;
import gse.test;
import gse.time;

export namespace gse::tests::concurrency {
	[[= test::unit{ .tags = "concurrency" }]]
	auto mpsc_push_publishes_a_complete_value(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "concurrency" }]]
	auto mpsc_rejects_a_full_buffer(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "concurrency" }]]
	auto spsc_preserves_order(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "concurrency" }]]
	auto work_stealing_pop_and_steal_never_duplicate(
		test::context& ctx
	) -> void;
}

namespace gse::tests::concurrency {
	struct stamped {
		std::uint64_t value = 0;
		std::uint64_t check = 0;
	};

	auto stamp(
		std::uint64_t value
	) -> stamped;

	auto intact(
		const stamped& item
	) -> bool;

	auto expired(
		const time& budget,
		const clock& timer
	) -> bool;
}

auto gse::tests::concurrency::stamp(const std::uint64_t value) -> stamped {
	return {
		.value = value,
		.check = ~value,
	};
}

auto gse::tests::concurrency::intact(const stamped& item) -> bool {
	return item.check == ~item.value;
}

auto gse::tests::concurrency::expired(const time& budget, const clock& timer) -> bool {
	return timer.elapsed() > budget;
}

auto gse::tests::concurrency::mpsc_push_publishes_a_complete_value(test::context& ctx) -> void {
	constexpr std::size_t producer_count = 4;
	constexpr std::uint64_t per_producer = 2000;
	constexpr std::uint64_t expected_total = producer_count * per_producer;

	mpsc_ring_buffer<stamped, 1024> buffer;

	std::vector<task::thread> producers;
	for (std::size_t p = 0; p < producer_count; ++p) {
		producers.push_back(task::spawn(
			log::thread_role::worker,
			[&buffer, p](const std::stop_token& st) {
				for (std::uint64_t i = 0; i < per_producer; ++i) {
					const std::uint64_t value = p * per_producer + i;
					while (!buffer.push(stamp(value))) {
						if (st.stop_requested()) {
							return;
						}
						std::this_thread::yield();
					}
				}
			}
		));
	}

	const time budget = seconds(20.f);
	const clock timer;
	std::vector<bool> seen(expected_total, false);
	std::uint64_t received = 0;
	std::uint64_t torn = 0;
	std::uint64_t duplicated = 0;

	while (received < expected_total && !expired(budget, timer)) {
		stamped item;
		if (!buffer.pop(item)) {
			std::this_thread::yield();
			continue;
		}
		++received;
		if (!intact(item)) {
			++torn;
			continue;
		}
		if (item.value >= expected_total) {
			++torn;
			continue;
		}
		if (seen[item.value]) {
			++duplicated;
		}
		seen[item.value] = true;
	}

	for (task::thread& producer : producers) {
		producer.request_stop();
		producer.join();
	}

	ctx.expect_eq(torn, std::uint64_t{ 0 });
	ctx.expect_eq(duplicated, std::uint64_t{ 0 });
	ctx.expect_eq(received, expected_total);
	ctx.expect(std::ranges::all_of(seen, [](const bool v) { return v; }));
}

auto gse::tests::concurrency::mpsc_rejects_a_full_buffer(test::context& ctx) -> void {
	mpsc_ring_buffer<stamped, 4> buffer;

	for (std::uint64_t i = 0; i < 4; ++i) {
		if (!ctx.expect(buffer.push(stamp(i)))) {
			return;
		}
	}
	ctx.expect(!buffer.push(stamp(4)));

	stamped item;
	if (!ctx.expect(buffer.pop(item))) {
		return;
	}
	ctx.expect_eq(item.value, std::uint64_t{ 0 });
	ctx.expect(buffer.push(stamp(4)));
}

auto gse::tests::concurrency::spsc_preserves_order(test::context& ctx) -> void {
	constexpr std::uint64_t count = 10000;

	spsc_ring_buffer<std::uint64_t, 256> buffer;
	task::thread producer = task::spawn(
		log::thread_role::worker,
		[&buffer](const std::stop_token& st) {
			for (std::uint64_t i = 0; i < count; ++i) {
				while (!buffer.push(i)) {
					if (st.stop_requested()) {
						return;
					}
					std::this_thread::yield();
				}
			}
		}
	);

	const time budget = seconds(20.f);
	const clock timer;
	std::uint64_t expected_next = 0;
	std::uint64_t out_of_order = 0;

	while (expected_next < count && !expired(budget, timer)) {
		std::uint64_t value = 0;
		if (!buffer.pop(value)) {
			std::this_thread::yield();
			continue;
		}
		if (value != expected_next) {
			++out_of_order;
		}
		++expected_next;
	}

	producer.request_stop();
	producer.join();

	ctx.expect_eq(out_of_order, std::uint64_t{ 0 });
	ctx.expect_eq(expected_next, count);
}

auto gse::tests::concurrency::work_stealing_pop_and_steal_never_duplicate(test::context& ctx) -> void {
	constexpr std::size_t stealer_count = 3;
	constexpr std::uint64_t count = 20000;

	task::work_stealing_queue<std::uint64_t> queue;
	for (std::uint64_t i = 0; i < count; ++i) {
		queue.push(i);
	}

	std::array<std::vector<std::uint64_t>, stealer_count> stolen;
	std::atomic<std::uint64_t> taken = 0;
	std::atomic<bool> draining = true;

	std::vector<task::thread> stealers;
	for (std::size_t s = 0; s < stealer_count; ++s) {
		stealers.push_back(task::spawn(
			log::thread_role::worker,
			[&queue, &taken, &draining, &slot = stolen[s]](const std::stop_token&) {
				while (draining.load(std::memory_order_acquire) && taken.load(std::memory_order_acquire) < count) {
					std::uint64_t value = 0;
					if (!queue.try_steal(value)) {
						std::this_thread::yield();
						continue;
					}
					slot.push_back(value);
					taken.fetch_add(1, std::memory_order_release);
				}
			}
		));
	}

	const time budget = seconds(20.f);
	const clock timer;
	std::vector<std::uint64_t> popped;
	while (taken.load(std::memory_order_acquire) < count && !expired(budget, timer)) {
		std::uint64_t value = 0;
		if (!queue.try_pop(value)) {
			std::this_thread::yield();
			continue;
		}
		popped.push_back(value);
		taken.fetch_add(1, std::memory_order_release);
	}

	draining.store(false, std::memory_order_release);
	for (task::thread& stealer : stealers) {
		stealer.join();
	}

	std::vector<std::uint64_t> all = std::move(popped);
	for (const std::vector<std::uint64_t>& slot : stolen) {
		all.insert(all.end(), slot.begin(), slot.end());
	}
	std::ranges::sort(all);

	if (!ctx.expect_eq(all.size(), static_cast<std::size_t>(count))) {
		return;
	}
	ctx.expect(std::ranges::adjacent_find(all) == all.end());
	ctx.expect_eq(all.front(), std::uint64_t{ 0 });
	ctx.expect_eq(all.back(), count - 1);
}
