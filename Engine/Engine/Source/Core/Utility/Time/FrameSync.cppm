module;

#include <tbb/concurrent_queue.h>

export module gse.utility:frame_sync;

import std;

export namespace gse::frame_sync {
	using callback = std::function<void()>;

	auto on_begin(
		callback cb
	);

	auto on_end(
		callback cb
	);

	auto begin(
	) -> void;

	auto end(
	) -> void;
}

namespace gse::frame_sync {
	tbb::concurrent_bounded_queue<callback> begin_pending;
	tbb::concurrent_bounded_queue<callback> end_pending;

	std::vector<callback> begin_list;
	std::vector<callback> end_list;

	auto drain(
		tbb::concurrent_bounded_queue<callback>& q,
		std::vector<callback>& out
	) -> void;
}

auto gse::frame_sync::on_begin(callback cb) {
	begin_pending.push(std::move(cb));
}

auto gse::frame_sync::on_end(callback cb) {
	end_pending.push(std::move(cb));
}

auto gse::frame_sync::begin() -> void {
	drain(begin_pending, begin_list);
	for (const auto& cb : begin_list) {
		cb();
	}
}

auto gse::frame_sync::end() -> void {
	drain(end_pending, end_list);
	for (const auto& cb : end_list) {
		cb();
	}
}

auto gse::frame_sync::drain(tbb::concurrent_bounded_queue<callback>& q, std::vector<callback>& out) -> void {
	callback cb;
	while (q.try_pop(cb)) {
		out.push_back(std::move(cb));
	}
}
