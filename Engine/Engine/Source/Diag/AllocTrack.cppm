export module gse.diag:alloc_track;

import std;

export namespace gse::alloc {
	struct site {
		std::uint64_t pc;
		std::int64_t live_bytes;
		std::int64_t live_samples;
		std::int64_t since_mark_bytes;
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
		std::size_t bytes
	) -> void;

	auto sample_interval() -> std::size_t;

	struct address_space {
		std::int64_t private_committed;
		std::int64_t image;
		std::int64_t mapped;
		std::int64_t reserved;
	};

	auto address_space_usage() -> address_space;

	auto estimated_live_bytes() -> std::int64_t;

	auto live_samples() -> std::int64_t;

	auto evicted_samples() -> std::int64_t;

	auto mark() -> void;

	auto snapshot(
		std::vector<site>& out
	) -> void;

	auto label_of(
		std::uint64_t pc
	) -> std::string;
}
