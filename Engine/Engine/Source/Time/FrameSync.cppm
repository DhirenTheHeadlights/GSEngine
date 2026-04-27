export module gse.time:frame_sync;

import std;
import gse.moodycamel;

export namespace gse::frame_sync {
	using callback = std::function<void()>;

	auto on_begin(
		callback cb
	) -> void;

	auto on_end(
		callback cb
	) -> void;

	auto begin(
	) -> void;

	auto end(
	) -> void;
}

namespace gse::frame_sync {
	moodycamel::ConcurrentQueue<callback> begin_pending;
	moodycamel::ConcurrentQueue<callback> end_pending;

	std::vector<callback> begin_list;
	std::vector<callback> end_list;

	auto drain(
		moodycamel::ConcurrentQueue<callback>& q,
		std::vector<callback>& out
	) -> void;
}

auto gse::frame_sync::on_begin(callback cb) -> void {
	begin_pending.enqueue(std::move(cb));
}

auto gse::frame_sync::on_end(callback cb) -> void {
	end_pending.enqueue(std::move(cb));
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

auto gse::frame_sync::drain(moodycamel::ConcurrentQueue<callback>& q, std::vector<callback>& out) -> void {
	callback cb;
	while (q.try_dequeue(cb)) {
		out.push_back(std::move(cb));
	}
}
