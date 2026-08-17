module gse.diag:alloc_track_impl;

import std;

import :alloc_track;

import gse.win32;

namespace gse::alloc {
	constexpr std::size_t site_capacity = 1u << 13;
	constexpr std::size_t sample_capacity = 1u << 20;
	constexpr std::size_t max_probe = 32;
	constexpr std::size_t default_interval = 256u * 1024u;
	constexpr std::size_t default_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

	struct site_slot {
		std::atomic<std::uint64_t> pc;
		std::atomic<std::int64_t> live_bytes;
		std::atomic<std::int64_t> live_samples;
		std::atomic<std::int64_t> mark_bytes;
	};

	struct sample_entry {
		std::atomic<std::uintptr_t> key;
		std::atomic<std::int64_t> weight;
		std::atomic<std::uint32_t> slot;
	};

	site_slot slots[site_capacity] = {};

	std::atomic<sample_entry*> samples{ nullptr };
	std::atomic<std::int64_t> total_bytes{ 0 };
	std::atomic<std::int64_t> total_samples{ 0 };
	std::atomic<std::int64_t> total_evicted{ 0 };
	std::atomic<std::size_t> interval_bytes{ default_interval };
	std::atomic<std::uint64_t> seed_source{ 0x243f6a8885a308d3ull };
	std::atomic<bool> tracking{ false };

	thread_local std::int64_t sample_countdown = 0;
	thread_local std::uint64_t rng_state = 0;

	auto mix(
		std::uint64_t value
	) -> std::uint64_t;

	auto slot_for(
		std::uint64_t pc
	) -> std::uint32_t;

	auto next_countdown() -> std::int64_t;

	auto should_sample(
		std::size_t size
	) -> bool;

	auto credit(
		std::uint32_t site,
		std::int64_t bytes,
		std::int64_t count
	) -> void;

	auto remember(
		void* block,
		std::size_t size,
		const void* site_pc
	) -> void;

	auto forget(
		void* block
	) -> void;
}

auto gse::alloc::mix(std::uint64_t value) -> std::uint64_t {
	value *= 0x9e3779b97f4a7c15ull;
	value ^= value >> 31;
	return value;
}

auto gse::alloc::slot_for(const std::uint64_t pc) -> std::uint32_t {
	std::size_t index = static_cast<std::size_t>(mix(pc)) & (site_capacity - 1);
	if (index == 0) {
		index = 1;
	}

	for (std::size_t probe = 0; probe < max_probe; ++probe) {
		const std::uint64_t existing = slots[index].pc.load(std::memory_order_acquire);
		if (existing == pc) {
			return static_cast<std::uint32_t>(index);
		}
		if (existing == 0) {
			std::uint64_t expected = 0;
			if (slots[index].pc.compare_exchange_strong(expected, pc, std::memory_order_acq_rel) || expected == pc) {
				return static_cast<std::uint32_t>(index);
			}
		}
		index = (index + 1) & (site_capacity - 1);
		if (index == 0) {
			index = 1;
		}
	}

	return 0;
}

auto gse::alloc::next_countdown() -> std::int64_t {
	if (rng_state == 0) {
		rng_state = seed_source.fetch_add(0x9e3779b97f4a7c15ull, std::memory_order_relaxed) | 1ull;
	}

	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;

	const double uniform = static_cast<double>(rng_state >> 11) * 0x1p-53;
	const auto interval = static_cast<double>(interval_bytes.load(std::memory_order_relaxed));
	return static_cast<std::int64_t>(-interval * std::log(1.0 - uniform)) + 1;
}

auto gse::alloc::should_sample(const std::size_t size) -> bool {
	if (interval_bytes.load(std::memory_order_relaxed) == 0) {
		return true;
	}

	sample_countdown -= static_cast<std::int64_t>(size);
	if (sample_countdown > 0) {
		return false;
	}

	sample_countdown = next_countdown();
	return true;
}

auto gse::alloc::credit(const std::uint32_t site, const std::int64_t bytes, const std::int64_t count) -> void {
	total_bytes.fetch_add(bytes, std::memory_order_relaxed);
	total_samples.fetch_add(count, std::memory_order_relaxed);

	if (site != 0) {
		slots[site].live_bytes.fetch_add(bytes, std::memory_order_relaxed);
		slots[site].live_samples.fetch_add(count, std::memory_order_relaxed);
	}
}

auto gse::alloc::remember(void* block, const std::size_t size, const void* site_pc) -> void {
	if (!tracking.load(std::memory_order_relaxed)) {
		return;
	}

	auto* table = samples.load(std::memory_order_acquire);
	if (table == nullptr || !should_sample(size)) {
		return;
	}

	const auto interval = static_cast<std::int64_t>(interval_bytes.load(std::memory_order_relaxed));
	const std::int64_t weight = std::max(static_cast<std::int64_t>(size), interval);
	const auto key = reinterpret_cast<std::uintptr_t>(block);
	auto& entry = table[static_cast<std::size_t>(mix(key)) & (sample_capacity - 1)];

	const std::uintptr_t evicted = entry.key.exchange(key, std::memory_order_acq_rel);
	if (evicted != 0 && evicted != key) {
		credit(entry.slot.load(std::memory_order_relaxed), -entry.weight.load(std::memory_order_relaxed), -1);
		total_evicted.fetch_add(1, std::memory_order_relaxed);
	}

	const std::uint32_t site = slot_for(reinterpret_cast<std::uint64_t>(site_pc));
	entry.weight.store(weight, std::memory_order_relaxed);
	entry.slot.store(site, std::memory_order_release);
	credit(site, weight, 1);
}

auto gse::alloc::forget(void* block) -> void {
	auto* table = samples.load(std::memory_order_acquire);
	if (table == nullptr) {
		return;
	}

	const auto key = reinterpret_cast<std::uintptr_t>(block);
	auto& entry = table[static_cast<std::size_t>(mix(key)) & (sample_capacity - 1)];
	if (entry.key.load(std::memory_order_acquire) != key) {
		return;
	}

	const std::int64_t weight = entry.weight.load(std::memory_order_relaxed);
	const std::uint32_t site = entry.slot.load(std::memory_order_acquire);

	std::uintptr_t expected = key;
	if (entry.key.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
		credit(site, -weight, -1);
	}
}

auto gse::alloc::allocate(const std::size_t size, const void* site_pc) -> void* {
	void* block = std::malloc(size);
	if (block != nullptr) {
		remember(block, size, site_pc);
	}
	return block;
}

auto gse::alloc::release(void* block) -> void {
	if (block == nullptr) {
		return;
	}
	forget(block);
	std::free(block);
}

auto gse::alloc::allocate_aligned(const std::size_t size, const std::align_val_t alignment, const void* site_pc) -> void* {
	const auto align = static_cast<std::size_t>(alignment);
	if (align <= default_alignment) {
		return allocate(size, site_pc);
	}

	void* raw = std::malloc(size + align);
	if (raw == nullptr) {
		return nullptr;
	}

	const auto base = reinterpret_cast<std::uintptr_t>(raw);
	const std::uintptr_t payload = (base + align) & ~static_cast<std::uintptr_t>(align - 1);
	*(reinterpret_cast<std::uint32_t*>(payload) - 1) = static_cast<std::uint32_t>(payload - base);

	auto* block = reinterpret_cast<void*>(payload);
	remember(block, size, site_pc);
	return block;
}

auto gse::alloc::release_aligned(void* block, const std::align_val_t alignment) -> void {
	if (block == nullptr) {
		return;
	}

	if (static_cast<std::size_t>(alignment) <= default_alignment) {
		release(block);
		return;
	}

	forget(block);

	const auto payload = reinterpret_cast<std::uintptr_t>(block);
	const std::uint32_t offset = *(reinterpret_cast<std::uint32_t*>(payload) - 1);
	std::free(reinterpret_cast<void*>(payload - offset));
}

auto gse::alloc::set_enabled(const bool on) -> void {
	if (on && samples.load(std::memory_order_acquire) == nullptr) {
		auto* table = static_cast<sample_entry*>(std::malloc(sample_capacity * sizeof(sample_entry)));
		if (table == nullptr) {
			return;
		}
		std::memset(table, 0, sample_capacity * sizeof(sample_entry));

		sample_entry* expected = nullptr;
		if (!samples.compare_exchange_strong(expected, table, std::memory_order_acq_rel)) {
			std::free(table);
		}
	}

	tracking.store(on, std::memory_order_relaxed);
}

auto gse::alloc::enabled() -> bool {
	return tracking.load(std::memory_order_relaxed);
}

auto gse::alloc::set_sample_interval(const std::size_t bytes) -> void {
	interval_bytes.store(bytes, std::memory_order_relaxed);
}

auto gse::alloc::sample_interval() -> std::size_t {
	return interval_bytes.load(std::memory_order_relaxed);
}

#ifdef _WIN32
auto gse::alloc::address_space_usage() -> address_space {
	using namespace gse::win32;

	address_space usage{};
	MEMORY_BASIC_INFORMATION region{};

	for (std::uintptr_t at = 0; VirtualQuery(reinterpret_cast<const void*>(at), &region, sizeof(region)) == sizeof(region); at += region.RegionSize) {
		const auto size = static_cast<std::int64_t>(region.RegionSize);

		if (region.State == mem_reserve) {
			usage.reserved += size;
		}
		else if (region.State == mem_commit) {
			if (region.Type == mem_image) {
				usage.image += size;
			}
			else if (region.Type == mem_mapped) {
				usage.mapped += size;
			}
			else {
				usage.private_committed += size;
			}
		}

		if (region.RegionSize == 0) {
			break;
		}
	}

	return usage;
}
#else
auto gse::alloc::address_space_usage() -> address_space {
	return {};
}
#endif

auto gse::alloc::estimated_live_bytes() -> std::int64_t {
	return total_bytes.load(std::memory_order_relaxed);
}

auto gse::alloc::live_samples() -> std::int64_t {
	return total_samples.load(std::memory_order_relaxed);
}

auto gse::alloc::evicted_samples() -> std::int64_t {
	return total_evicted.load(std::memory_order_relaxed);
}

auto gse::alloc::mark() -> void {
	for (auto& slot : slots) {
		slot.mark_bytes.store(slot.live_bytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
}

auto gse::alloc::snapshot(std::vector<site>& out) -> void {
	out.clear();

	for (const auto& slot : slots | std::views::drop(1)) {
		const std::uint64_t pc = slot.pc.load(std::memory_order_relaxed);
		if (pc == 0) {
			continue;
		}

		const std::int64_t bytes = slot.live_bytes.load(std::memory_order_relaxed);
		const std::int64_t count = slot.live_samples.load(std::memory_order_relaxed);
		const std::int64_t since = bytes - slot.mark_bytes.load(std::memory_order_relaxed);

		if (count == 0 && since == 0) {
			continue;
		}

		out.push_back({
			.pc = pc,
			.live_bytes = bytes,
			.live_samples = count,
			.since_mark_bytes = since,
		});
	}
}

#ifdef _WIN32
auto gse::alloc::label_of(const std::uint64_t pc) -> std::string {
	using namespace gse::win32;

	std::string module_name = "?";
	std::uint64_t offset = pc;

	HMODULE handle = nullptr;
	if (pc != 0 && GetModuleHandleExW(get_module_handle_ex_flag_from_address | get_module_handle_ex_flag_unchanged_refcount, reinterpret_cast<const wchar_t*>(pc), &handle) && handle != nullptr) {
		wchar_t path[max_path];
		if (const auto length = GetModuleFileNameW(handle, path, max_path); length > 0) {
			std::wstring_view view(path, length);
			if (const auto slash = view.find_last_of(L"\\/"); slash != std::wstring_view::npos) {
				view = view.substr(slash + 1);
			}
			module_name.clear();
			for (const wchar_t character : view) {
				module_name.push_back(static_cast<char>(character));
			}
		}
		offset = pc - reinterpret_cast<std::uintptr_t>(handle);
	}

	return std::format("{}+0x{:x}", module_name, offset);
}
#else
auto gse::alloc::label_of(const std::uint64_t pc) -> std::string {
	return std::format("0x{:x}", pc);
}
#endif
