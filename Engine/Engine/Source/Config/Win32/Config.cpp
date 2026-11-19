module gse.config;

import gse.win32;
import std;

namespace gse::config {
	auto folder_id(
		known_directory folder
	) -> const win32::GUID&;
}

auto gse::config::folder_id(const known_directory folder) -> const win32::GUID& {
	switch (folder) {
		case known_directory::roaming_app_data:
			return win32::FOLDERID_RoamingAppData;
		case known_directory::user_profile:
			return win32::FOLDERID_Profile;
		case known_directory::local_app_data:
			return win32::FOLDERID_LocalAppData;
	}
	return win32::FOLDERID_LocalAppData;
}

auto gse::config::executable_path() -> std::filesystem::path {
	wchar_t buffer[win32::max_path]{};
	const win32::DWORD length = win32::GetModuleFileNameW(nullptr, buffer, win32::max_path);
	if (length == 0 || length >= win32::max_path) {
		return {};
	}
	return { std::wstring_view(buffer, length) };
}

auto gse::config::known_folder(const known_directory folder) -> std::filesystem::path {
	wchar_t* buffer = nullptr;
	const auto status = win32::SHGetKnownFolderPath(folder_id(folder), win32::kf_flag_default, nullptr, &buffer);
	std::filesystem::path result;
	if (status == 0) {
		result = std::filesystem::path(std::wstring_view(buffer));
	}
	win32::CoTaskMemFree(buffer);
	return result;
}
