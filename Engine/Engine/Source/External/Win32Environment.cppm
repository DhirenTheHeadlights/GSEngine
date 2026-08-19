export module gse.win32.environment;

import std;
import gse.win32;

#ifdef _WIN32
export namespace gse::win32 {
	auto environment_with_path_prefix(
		std::wstring_view path
	) -> std::vector<wchar_t>;

	auto environment_with_variable(
		std::wstring_view name,
		std::wstring_view value
	) -> std::vector<wchar_t>;

	auto user_environment_value(
		std::wstring_view name
	) -> std::wstring;
}

namespace gse::win32 {
	constexpr std::size_t environment_value_capacity = 32768;

	auto environment_character(const wchar_t value) -> wchar_t;

	auto environment_less(
		std::wstring_view lhs,
		std::wstring_view rhs
	) -> bool;

	auto environment_entry_named(
		std::wstring_view entry,
		std::wstring_view name
	) -> bool;

	auto environment_name_is_path(
		std::wstring_view entry
	) -> bool;

	auto same_environment_path(
		std::wstring_view lhs,
		std::wstring_view rhs
	) -> bool;

	auto current_environment_entries() -> std::vector<std::wstring>;

	auto environment_block(
		std::vector<std::wstring>& entries
	) -> std::vector<wchar_t>;
}

auto gse::win32::environment_character(const wchar_t value) -> wchar_t {
	return static_cast<wchar_t>(std::towlower(value));
}

auto gse::win32::environment_less(const std::wstring_view lhs, const std::wstring_view rhs) -> bool {
	return std::ranges::lexicographical_compare(
		lhs,
		rhs,
		std::ranges::less{},
		environment_character,
		environment_character
	);
}

auto gse::win32::environment_entry_named(const std::wstring_view entry, const std::wstring_view name) -> bool {
	return entry.size() > name.size() && entry[name.size()] == L'=' && std::ranges::equal(
		entry.substr(0, name.size()),
		name,
		std::ranges::equal_to{},
		environment_character,
		environment_character
	);
}

auto gse::win32::environment_name_is_path(const std::wstring_view entry) -> bool {
	return environment_entry_named(entry, L"PATH");
}

auto gse::win32::same_environment_path(std::wstring_view lhs, std::wstring_view rhs) -> bool {
	while (!lhs.empty() && (lhs.back() == L'\\' || lhs.back() == L'/')) {
		lhs.remove_suffix(1);
	}
	while (!rhs.empty() && (rhs.back() == L'\\' || rhs.back() == L'/')) {
		rhs.remove_suffix(1);
	}
	return std::ranges::equal(
		lhs,
		rhs,
		std::ranges::equal_to{},
		environment_character,
		environment_character
	);
}

auto gse::win32::current_environment_entries() -> std::vector<std::wstring> {
	wchar_t* current = GetEnvironmentStringsW();
	if (!current) {
		return {};
	}

	std::vector<std::wstring> entries;
	for (const wchar_t* entry = current; *entry; entry += std::wcslen(entry) + 1) {
		entries.emplace_back(entry);
	}
	FreeEnvironmentStringsW(current);
	return entries;
}

auto gse::win32::environment_block(std::vector<std::wstring>& entries) -> std::vector<wchar_t> {
	std::ranges::sort(entries, environment_less);
	std::size_t size = 1;
	for (const std::wstring& entry : entries) {
		size += entry.size() + 1;
	}
	std::vector<wchar_t> result;
	result.reserve(size);
	for (const std::wstring& entry : entries) {
		result.insert(result.end(), entry.begin(), entry.end());
		result.push_back(L'\0');
	}
	result.push_back(L'\0');
	return result;
}

auto gse::win32::environment_with_path_prefix(const std::wstring_view path) -> std::vector<wchar_t> {
	std::vector<std::wstring> entries = current_environment_entries();
	if (entries.empty()) {
		return {};
	}

	if (!path.empty()) {
		const auto existing = std::ranges::find_if(entries, environment_name_is_path);
		if (existing == entries.end()) {
			entries.emplace_back(L"PATH=" + std::wstring(path));
		}
		else {
			constexpr std::size_t value_offset = 5;
			const std::wstring_view value(*existing);
			const std::size_t separator = value.find(L';', value_offset);
			const std::wstring_view first = value.substr(value_offset, separator == std::wstring_view::npos ? separator : separator - value_offset);
			if (!same_environment_path(first, path)) {
				*existing = L"PATH=" + std::wstring(path) + L';' + existing->substr(value_offset);
			}
		}
	}

	return environment_block(entries);
}

auto gse::win32::environment_with_variable(const std::wstring_view name, const std::wstring_view value) -> std::vector<wchar_t> {
	std::vector<std::wstring> entries = current_environment_entries();
	if (entries.empty() || name.empty()) {
		return {};
	}

	std::wstring assignment = std::wstring(name) + L'=' + std::wstring(value);
	std::wstring* existing = nullptr;
	for (std::wstring& entry : entries) {
		if (environment_entry_named(entry, name)) {
			existing = &entry;
			break;
		}
	}

	if (existing) {
		*existing = std::move(assignment);
	}
	else {
		entries.push_back(std::move(assignment));
	}

	return environment_block(entries);
}

auto gse::win32::user_environment_value(const std::wstring_view name) -> std::wstring {
	const std::wstring key(name);
	std::vector<wchar_t> raw(environment_value_capacity);
	if (!read_user_environment(key.c_str(), raw.data(), static_cast<DWORD>(raw.size()))) {
		return {};
	}

	std::wstring value(raw.data());
	if (value.find(L'%') == std::wstring::npos) {
		return value;
	}

	std::vector<wchar_t> expanded(environment_value_capacity);
	const DWORD written = ExpandEnvironmentStringsW(value.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));
	return written > 0 && written <= expanded.size() ? std::wstring(expanded.data()) : value;
}
#endif
