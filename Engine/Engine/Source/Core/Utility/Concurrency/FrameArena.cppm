export module gse.utility:frame_arena;

import std;

export namespace gse::frame_arena {
	constexpr std::size_t default_capacity = 4 * 1024 * 1024;

	auto create(
		std::size_t capacity = default_capacity
	) -> void;

	auto destroy(
	) -> void;

	auto allocate(
		std::size_t size,
		std::size_t align = alignof(std::max_align_t)
	) -> void*;

	auto contains(
		const void* ptr
	) noexcept -> bool;

	auto reset(
	) noexcept -> void;

	auto bytes_used(
	) noexcept -> std::size_t;
}

namespace gse::frame_arena {
	std::byte* buffer = nullptr;
	std::size_t buffer_capacity = 0;
	std::atomic<std::size_t> offset{ 0 };
}

auto gse::frame_arena::create(const std::size_t capacity) -> void {
	buffer = static_cast<std::byte*>(::operator new(capacity, std::align_val_t{ alignof(std::max_align_t) }));
	buffer_capacity = capacity;
	offset.store(0, std::memory_order_relaxed);
}

auto gse::frame_arena::destroy() -> void {
	if (buffer) {
		::operator delete(buffer, std::align_val_t{ alignof(std::max_align_t) });
		buffer = nullptr;
		buffer_capacity = 0;
		offset.store(0, std::memory_order_relaxed);
	}
}

auto gse::frame_arena::allocate(const std::size_t size, const std::size_t align) -> void* {
	if (!buffer) {
		return ::operator new(size);
	}

	const std::size_t aligned_size = (size + align - 1) & ~(align - 1);
	const std::size_t old = offset.fetch_add(aligned_size, std::memory_order_relaxed);

	if (old + aligned_size > buffer_capacity) {
		return ::operator new(size);
	}

	return buffer + old;
}

auto gse::frame_arena::contains(const void* ptr) noexcept -> bool {
	if (!buffer) return false;
	const auto* p = static_cast<const std::byte*>(ptr);
	return p >= buffer && p < buffer + buffer_capacity;
}

auto gse::frame_arena::reset() noexcept -> void {
	offset.store(0, std::memory_order_release);
}

auto gse::frame_arena::bytes_used() noexcept -> std::size_t {
	return offset.load(std::memory_order_relaxed);
}
