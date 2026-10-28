module gse.diag:alloc_track_platform;

import std;

import gse.math;
import gse.win32;

import :alloc_track;

auto gse::alloc::address_space_usage() -> address_space {
	using namespace gse::win32;

	address_space usage{};
	MEMORY_BASIC_INFORMATION region{};

	for (std::uintptr_t at = 0; VirtualQuery(reinterpret_cast<const void*>(at), &region, sizeof(region)) == sizeof(region); at += region.RegionSize) {
		const auto size = byte_count::from<bytes>(region.RegionSize);

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
