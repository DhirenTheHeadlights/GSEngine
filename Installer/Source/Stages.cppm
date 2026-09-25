export module installer:stages;

import std;

import gse;
import gse.win32;
import gse.win32.environment;

export namespace installer {
	struct arguments {
		std::string payload;
		bool uninstall = false;
	};

	constexpr std::string_view uninstaller_path = "Bin/Installer.exe";
	constexpr std::string_view record_name = "gse.install";

	auto own_executable() -> std::filesystem::path;

	auto default_destination(
		const gse::sdk::pack_table& table
	) -> std::filesystem::path;

	auto image_path(
		const std::filesystem::path& destination,
		const gse::sdk::pack_table& table
	) -> std::filesystem::path;

	auto install(
		const gse::sdk::pack_view& pack,
		const std::filesystem::path& destination,
		std::atomic<std::size_t>& done
	) -> std::expected<std::filesystem::path, std::string>;

	auto uninstall(
		const std::filesystem::path& image
	) -> std::expected<void, std::string>;

	auto report(
		const std::expected<void, std::string>& outcome
	) -> int;
}

namespace installer {
	auto uninstall_key(
		std::string_view version,
		std::string_view preset
	) -> std::wstring;

	auto write_uninstall_entry(
		const gse::sdk::pack_table& table,
		const std::filesystem::path& image
	) -> std::expected<void, std::string>;

	auto schedule_directory_removal(
		const std::filesystem::path& image
	) -> std::expected<void, std::string>;
}

auto installer::own_executable() -> std::filesystem::path {
	wchar_t buffer[gse::win32::max_path]{};
	const auto length = gse::win32::GetModuleFileNameW(nullptr, buffer, gse::win32::max_path);
	return length == 0 ? std::filesystem::path{} : std::filesystem::path(std::wstring_view(buffer, length));
}

auto installer::default_destination(const gse::sdk::pack_table& table) -> std::filesystem::path {
	return gse::config::user_state_dir() / "sdk" / table.version;
}

auto installer::image_path(const std::filesystem::path& destination, const gse::sdk::pack_table& table) -> std::filesystem::path {
	return destination / table.preset;
}

auto installer::install(const gse::sdk::pack_view& pack, const std::filesystem::path& destination, std::atomic<std::size_t>& done) -> std::expected<std::filesystem::path, std::string> {
	if (destination.empty() || !destination.is_absolute()) {
		return std::unexpected("the destination must be an absolute path");
	}
	const std::filesystem::path image = image_path(destination, pack.table);
	std::error_code ec;
	std::filesystem::remove_all(image, ec);

	std::ifstream payload(pack.file, std::ios::binary);
	if (!payload) {
		return std::unexpected(std::format("could not open {}", pack.file.generic_display_string()));
	}
	std::string record;
	for (const gse::sdk::pack_entry& entry : pack.table.entries) {
		if (const auto extracted = gse::sdk::extract_entry(payload, entry, image); !extracted) {
			return std::unexpected(extracted.error());
		}
		record += entry.path;
		record += '\n';
		done.fetch_add(1, std::memory_order_release);
	}

	if (const auto written = gse::fs::write_text(image / record_name, record); !written) {
		return std::unexpected(written.error());
	}

	gse::sdk::register_image(pack.table.version, destination);
	if (const auto registered = write_uninstall_entry(pack.table, image); !registered) {
		return std::unexpected(registered.error());
	}
	return image;
}

auto installer::uninstall_key(const std::string_view version, const std::string_view preset) -> std::wstring {
	return gse::win32::widen(std::format("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GSEngineSDK-{}-{}", version, preset));
}

auto installer::write_uninstall_entry(const gse::sdk::pack_table& table, const std::filesystem::path& image) -> std::expected<void, std::string> {
	const std::wstring key = uninstall_key(table.version, table.preset);
	const std::wstring location = std::filesystem::path(image).make_preferred().wstring();
	const std::wstring command = L"\"" + (image / uninstaller_path).make_preferred().wstring() + L"\" --uninstall";
	const std::array<std::pair<const wchar_t*, std::wstring>, 5> values = {{
		{ L"DisplayName", gse::win32::widen(std::format("GSEngine SDK {} ({})", table.version, table.preset)) },
		{ L"DisplayVersion", gse::win32::widen(table.version) },
		{ L"Publisher", L"GSEngine" },
		{ L"InstallLocation", location },
		{ L"UninstallString", command },
	}};
	for (const auto& [name, value] : values) {
		if (!gse::win32::write_user_registry_string(key.c_str(), name, value.c_str())) {
			return std::unexpected("could not write the Windows uninstall entry");
		}
	}
	return {};
}

auto installer::uninstall(const std::filesystem::path& image) -> std::expected<void, std::string> {
	const std::string record = gse::fs::read_text(image / record_name);
	if (record.empty()) {
		return std::unexpected(std::format("{} is not an installed SDK image ({} missing)", image.generic_display_string(), record_name));
	}
	const std::string version = gse::config::manifest_value(gse::fs::read_text(image / "gse.manifest"), "version");
	const std::string preset = image.filename().generic_native_encoded_string();
	gse::sdk::unregister_image(version, image.parent_path());
	gse::win32::delete_user_registry_key(uninstall_key(version, preset).c_str());

	std::error_code ec;
	for (const auto line : std::views::split(record, '\n')) {
		const std::string_view path(line);
		if (!path.empty()) {
			std::filesystem::remove(image / path, ec);
		}
	}
	std::filesystem::remove(image / record_name, ec);
	std::filesystem::remove(image / "gse.manifest", ec);
	return schedule_directory_removal(image);
}

auto installer::schedule_directory_removal(const std::filesystem::path& image) -> std::expected<void, std::string> {
	std::error_code ec;
	const std::filesystem::path script = std::filesystem::temp_directory_path(ec) / std::format("gse-uninstall-{}.cmd", image.filename().generic_native_encoded_string());
	if (ec) {
		return std::unexpected("could not resolve the temp directory");
	}
	const std::string body = std::format(
		"@echo off\r\n"
		"set tries=0\r\n"
		":retry\r\n"
		"timeout /t 1 /nobreak >nul\r\n"
		"rmdir /s /q \"{0}\" 2>nul\r\n"
		"if not exist \"{0}\" goto done\r\n"
		"set /a tries+=1\r\n"
		"if %tries% lss 30 goto retry\r\n"
		":done\r\n"
		"rmdir \"{1}\" 2>nul\r\n"
		"del \"%~f0\"\r\n",
		image.native_encoded_string(),
		image.parent_path().native_encoded_string()
	);
	if (const auto written = gse::fs::write_text(script, body); !written) {
		return written;
	}
	gse::app::relaunch_on_exit(script);
	gse::app::run_pending_relaunch();
	return {};
}

auto installer::report(const std::expected<void, std::string>& outcome) -> int {
	if (outcome) {
		return 0;
	}
	gse::win32::show_error_box(L"GSEngine SDK Setup", gse::win32::widen(outcome.error()).c_str());
	return 1;
}
