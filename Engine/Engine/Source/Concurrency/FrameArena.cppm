export module gse.concurrency:frame_arena;

import std;

export namespace gse::frame_arena {
	auto allocate(
		std::size_t size,
		std::size_t align = alignof(std::max_align_t)
	) -> void*;

	auto deallocate(
		void* ptr,
		std::size_t size
	) -> void;
}

namespace gse::frame_arena {
	constexpr std::size_t bucket_count = 5;
	constexpr std::size_t min_bucket_size = 64;

	struct block {
		block* next = nullptr;
	};

	struct bucket {
		block* free_list = nullptr;
		std::size_t block_size = 0;
	};

	struct thread_pool {
		std::array<bucket, bucket_count> buckets;

		thread_pool() {
			std::size_t size = min_bucket_size;
			for (auto& b : buckets) {
				b.block_size = size;
				size <<= 1;
			}
		}

		~thread_pool() {
			for (auto& b : buckets) {
				while (b.free_list) {
					auto* blk = b.free_list;
					b.free_list = blk->next;
					::operator delete(blk);
				}
			}
		}
	};

	inline thread_local thread_pool pool;

	auto bucket_index(
		std::size_t size
	) -> std::size_t;
}

auto gse::frame_arena::bucket_index(const std::size_t size) -> std::size_t {
	std::size_t s = min_bucket_size;
	for (std::size_t i = 0; i < bucket_count; ++i) {
		if (size <= s) {
			return i;
		}
		s <<= 1;
	}
	return bucket_count;
}

auto gse::frame_arena::allocate(const std::size_t size, const std::size_t) -> void* {
	const auto idx = bucket_index(size);

	if (idx >= bucket_count) {
		return ::operator new(size);
	}

	auto& b = pool.buckets[idx];

	if (b.free_list) {
		auto* blk = b.free_list;
		b.free_list = blk->next;
		return blk;
	}

	return ::operator new(b.block_size);
}

auto gse::frame_arena::deallocate(void* ptr, const std::size_t size) -> void {
	const auto idx = bucket_index(size);

	if (idx >= bucket_count) {
		::operator delete(ptr);
		return;
	}

	auto& b = pool.buckets[idx];
	auto* blk = static_cast<block*>(ptr);
	blk->next = b.free_list;
	b.free_list = blk;
}
