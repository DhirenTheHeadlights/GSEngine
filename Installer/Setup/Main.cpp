import std;

import gse.sdk;
import gse.win32;
import gse.win32.environment;

namespace setup {
	constexpr std::array<std::string_view, 3> bootstrap_prefixes = { "Bin/", "Engine/Resources/", "Engine/Baked/" };
	constexpr std::string_view installer_path = "Bin/Installer.exe";

	auto own_executable() -> std::filesystem::path;

	auto stage_directory(
		const gse::sdk::pack_table& table
	) -> std::expected<std::filesystem::path, std::string>;

	auto launch(
		const std::filesystem::path& executable,
		const std::filesystem::path& payload
	) -> std::expected<void, std::string>;

	auto bootstrap(
		const std::filesystem::path& payload
	) -> std::expected<void, std::string>;
}

auto setup::own_executable() -> std::filesystem::path {
	wchar_t buffer[gse::win32::max_path]{};
	const auto length = gse::win32::GetModuleFileNameW(nullptr, buffer, gse::win32::max_path);
	return length == 0 ? std::filesystem::path{} : std::filesystem::path(std::wstring_view(buffer, length));
}

auto setup::stage_directory(const gse::sdk::pack_table& table) -> std::expected<std::filesystem::path, std::string> {
	const char* local = std::getenv("LOCALAPPDATA");
	if (local == nullptr || *local == '\0') {
		return std::unexpected("LOCALAPPDATA is not set");
	}
	return std::filesystem::path(local) / "GSE" / "cache" / "installer" / table.version / table.preset;
}

auto setup::launch(const std::filesystem::path& executable, const std::filesystem::path& payload) -> std::expected<void, std::string> {
	std::wstring line = L"\"" + executable.wstring() + L"\" --payload \"" + payload.wstring() + L"\"";
	line.push_back(L'\0');
	const std::wstring directory = executable.parent_path().wstring();
	gse::win32::STARTUPINFOW startup{ .cb = sizeof(gse::win32::STARTUPINFOW) };
	gse::win32::PROCESS_INFORMATION process{};
	if (!gse::win32::CreateProcessW(executable.c_str(), line.data(), nullptr, nullptr, 0, 0, nullptr, directory.c_str(), &startup, &process)) {
		return std::unexpected(std::format("could not start {}", executable.generic_display_string()));
	}
	gse::win32::CloseHandle(process.hProcess);
	gse::win32::CloseHandle(process.hThread);
	return {};
}

auto setup::bootstrap(const std::filesystem::path& payload) -> std::expected<void, std::string> {
	const auto pack = gse::sdk::read_pack(payload);
	if (!pack) {
		return std::unexpected(pack.error());
	}
	const auto stage = stage_directory(pack->table);
	if (!stage) {
		return std::unexpected(stage.error());
	}
	std::error_code ec;
	std::filesystem::remove_all(*stage, ec);

	std::ifstream in(payload, std::ios::binary);
	for (const gse::sdk::pack_entry& entry : pack->table.entries) {
		const bool wanted = std::ranges::any_of(bootstrap_prefixes, [&entry](const std::string_view prefix) {
			return entry.path.starts_with(prefix);
		});
		if (!wanted) {
			continue;
		}
		if (const auto extracted = gse::sdk::extract_entry(in, entry, *stage); !extracted) {
			return extracted;
		}
	}

	std::ofstream manifest(*stage / "gse.manifest", std::ios::binary);
	manifest << "mode = installed\n";
	if (!manifest) {
		return std::unexpected(std::format("could not write the installer manifest under {}", stage->generic_display_string()));
	}
	manifest.close();
	return launch(*stage / installer_path, payload);
}

auto main() -> int {
	const auto outcome = setup::bootstrap(setup::own_executable());
	if (outcome) {
		return 0;
	}
	gse::win32::show_error_box(L"GSEngine SDK Setup", gse::win32::widen(outcome.error()).c_str());
	return 1;
}
