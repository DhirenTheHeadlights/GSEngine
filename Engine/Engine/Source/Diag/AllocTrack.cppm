export module gse.diag:alloc_track;

import std;

import gse.math;

export namespace gse::alloc {
	struct site {
		std::uint64_t pc;
		byte_count live;
		std::int64_t live_samples;
		byte_count since_mark;
	};

	auto allocate(
		std::size_t size,
		const void* site_pc
	) -> void*;

	auto release(
		void* block
	) -> void;

	auto allocate_aligned(
		std::size_t size,
		std::align_val_t alignment,
		const void* site_pc
	) -> void*;

	auto release_aligned(
		void* block,
		std::align_val_t alignment
	) -> void;

	auto set_enabled(
		bool on
	) -> void;

	auto enabled() -> bool;

	auto set_sample_interval(
		byte_count interval
	) -> void;

	auto sample_interval() -> byte_count;

	struct address_space {
		byte_count private_committed;
		byte_count image;
		byte_count mapped;
		byte_count reserved;
	};

	auto address_space_usage() -> address_space;

	auto estimated_live() -> byte_count;

	auto live_samples() -> std::int64_t;

	auto evicted_samples() -> std::int64_t;

	auto mark() -> void;

	auto snapshot(
		std::vector<site>& out
	) -> void;

	auto label_of(
		std::uint64_t pc
	) -> std::string;

	auto log_report(
		int top_rows
	) -> void;
}
