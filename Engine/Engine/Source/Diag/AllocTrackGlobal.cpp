import std;

import gse.diag;

auto operator new(const std::size_t size) -> void* {
	auto* block = gse::alloc::allocate(size, __builtin_return_address(0));
	if (block == nullptr) {
		throw std::bad_alloc();
	}
	return block;
}

auto operator new[](const std::size_t size) -> void* {
	auto* block = gse::alloc::allocate(size, __builtin_return_address(0));
	if (block == nullptr) {
		throw std::bad_alloc();
	}
	return block;
}

auto operator new(const std::size_t size, const std::align_val_t alignment) -> void* {
	auto* block = gse::alloc::allocate_aligned(size, alignment, __builtin_return_address(0));
	if (block == nullptr) {
		throw std::bad_alloc();
	}
	return block;
}

auto operator new[](const std::size_t size, const std::align_val_t alignment) -> void* {
	auto* block = gse::alloc::allocate_aligned(size, alignment, __builtin_return_address(0));
	if (block == nullptr) {
		throw std::bad_alloc();
	}
	return block;
}

auto operator new(const std::size_t size, const std::nothrow_t&) noexcept -> void* {
	return gse::alloc::allocate(size, __builtin_return_address(0));
}

auto operator new[](const std::size_t size, const std::nothrow_t&) noexcept -> void* {
	return gse::alloc::allocate(size, __builtin_return_address(0));
}

auto operator new(const std::size_t size, const std::align_val_t alignment, const std::nothrow_t&) noexcept -> void* {
	return gse::alloc::allocate_aligned(size, alignment, __builtin_return_address(0));
}

auto operator new[](const std::size_t size, const std::align_val_t alignment, const std::nothrow_t&) noexcept -> void* {
	return gse::alloc::allocate_aligned(size, alignment, __builtin_return_address(0));
}

auto operator delete(void* block) noexcept -> void {
	gse::alloc::release(block);
}

auto operator delete[](void* block) noexcept -> void {
	gse::alloc::release(block);
}

auto operator delete(void* block, std::size_t) noexcept -> void {
	gse::alloc::release(block);
}

auto operator delete[](void* block, std::size_t) noexcept -> void {
	gse::alloc::release(block);
}

auto operator delete(void* block, const std::align_val_t alignment) noexcept -> void {
	gse::alloc::release_aligned(block, alignment);
}

auto operator delete[](void* block, const std::align_val_t alignment) noexcept -> void {
	gse::alloc::release_aligned(block, alignment);
}

auto operator delete(void* block, std::size_t, const std::align_val_t alignment) noexcept -> void {
	gse::alloc::release_aligned(block, alignment);
}

auto operator delete[](void* block, std::size_t, const std::align_val_t alignment) noexcept -> void {
	gse::alloc::release_aligned(block, alignment);
}

auto operator delete(void* block, const std::nothrow_t&) noexcept -> void {
	gse::alloc::release(block);
}

auto operator delete[](void* block, const std::nothrow_t&) noexcept -> void {
	gse::alloc::release(block);
}

auto operator delete(void* block, const std::align_val_t alignment, const std::nothrow_t&) noexcept -> void {
	gse::alloc::release_aligned(block, alignment);
}

auto operator delete[](void* block, const std::align_val_t alignment, const std::nothrow_t&) noexcept -> void {
	gse::alloc::release_aligned(block, alignment);
}
