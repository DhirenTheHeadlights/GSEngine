export module gse.ide.build:profiles;

import std;
import gse;
import gse.ide.config;

import :configs;

export namespace gse::ide::build_runner {
	enum class build_target : std::uint8_t {
		game,
		editor,
	};

	struct play_session {
		std::uint8_t clients = 1;
		bool dedicated_server = false;
		bool attached = true;
		std::uint16_t base_port = 9000;

		auto operator==(
			const play_session& other
		) const -> bool = default;
	};

	struct build_profile {
		std::string name;
		std::string config;
		play_session session;

		auto operator==(
			const build_profile& other
		) const -> bool = default;
	};

	constexpr std::uint8_t max_profile_clients = 4;

	auto profiles_path() -> std::filesystem::path;

	auto default_profiles() -> std::vector<build_profile>;

	auto load_profiles(
		std::vector<build_profile>& profiles,
		std::string& active
	) -> void;

	auto save_profiles(
		const std::vector<build_profile>& profiles,
		const std::string& active
	) -> void;

	auto profile_for(
		std::span<const build_profile> profiles,
		std::string_view name
	) -> const build_profile*;

	auto profile_index(
		std::span<const build_profile> profiles,
		std::string_view name
	) -> std::size_t;

	auto profile_label(
		const build_profile& profile
	) -> std::string;

	auto unique_profile_name(
		std::span<const build_profile> profiles,
		std::string_view base
	) -> std::string;
}

namespace gse::ide::build_runner {
	constexpr std::uint32_t profiles_magic = 0x47534250;
	constexpr std::uint32_t profiles_version = 1;
}

auto gse::ide::build_runner::profiles_path() -> std::filesystem::path {
	return config::project_state_dir() / "build_profiles.bin";
}

auto gse::ide::build_runner::default_profiles() -> std::vector<build_profile> {
	return {
		{
			.name = "Play",
			.session = {
				.clients = 1,
			},
		},
		{
			.name = "Play 2 + Server",
			.session = {
				.clients = 2,
				.dedicated_server = true,
			},
		},
		{
			.name = "Play Windowed",
			.session = {
				.clients = 2,
				.dedicated_server = true,
				.attached = false,
			},
		},
	};
}

auto gse::ide::build_runner::load_profiles(std::vector<build_profile>& profiles, std::string& active) -> void {
	profiles = default_profiles();
	active = profiles.front().name;

	const std::filesystem::path path = profiles_path();
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return;
	}

	binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;
	if (magic != profiles_magic || epoch != archive_format_epoch) {
		log::println(log::level::warning, log::category::task, "build: '{}' is magic {:#x} epoch {}, expected magic {:#x} epoch {} - the default profiles were restored", path.generic_display_string(), magic, epoch, profiles_magic, archive_format_epoch);
		return;
	}
	if (version > profiles_version) {
		log::println(log::level::warning, log::category::task, "build: '{}' is version {}, newer than this editor's version {} - the default profiles were restored", path.generic_display_string(), version, profiles_version);
		return;
	}

	std::vector<build_profile> restored;
	std::string selected;
	reader & restored & selected;
	if (!reader.valid()) {
		log::println(log::level::error, log::category::task, "build: could not read '{}' - the default profiles were restored", path.generic_display_string());
		return;
	}

	for (const std::string& skipped : reader.skipped_fields()) {
		log::println(log::level::warning, log::category::task, "build: '{}' was written at version {}, dropping field {}", path.generic_display_string(), version, skipped);
	}

	if (restored.empty()) {
		return;
	}

	profiles = std::move(restored);
	active = profile_for(profiles, selected) != nullptr ? selected : profiles.front().name;
}

auto gse::ide::build_runner::save_profiles(const std::vector<build_profile>& profiles, const std::string& active) -> void {
	const std::filesystem::path path = profiles_path();
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "build: could not create '{}': {}", path.parent_path().generic_display_string(), ec.message());
		return;
	}

	std::filesystem::path temporary = path;
	temporary += ".tmp";
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		if (!out) {
			log::println(log::level::error, log::category::task, "build: could not open '{}'", temporary.generic_display_string());
			return;
		}
		binary_writer writer(out, profiles_magic, profiles_version);
		writer & profiles & active;
	}

	std::filesystem::rename(temporary, path, ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "build: could not replace '{}': {}", path.generic_display_string(), ec.message());
		std::filesystem::remove(temporary, ec);
	}
}

auto gse::ide::build_runner::profile_for(const std::span<const build_profile> profiles, const std::string_view name) -> const build_profile* {
	const auto found = std::ranges::find(profiles, name, &build_profile::name);
	return found != profiles.end() ? &*found : nullptr;
}

auto gse::ide::build_runner::profile_index(const std::span<const build_profile> profiles, const std::string_view name) -> std::size_t {
	const auto found = std::ranges::find(profiles, name, &build_profile::name);
	return found != profiles.end() ? static_cast<std::size_t>(std::ranges::distance(profiles.begin(), found)) : 0;
}

auto gse::ide::build_runner::profile_label(const build_profile& profile) -> std::string {
	if (profile.config.empty() || profile.config == active_build_config()) {
		return profile.name;
	}
	return std::format("{}  [{}]", profile.name, build_config_label(profile.config));
}

auto gse::ide::build_runner::unique_profile_name(const std::span<const build_profile> profiles, const std::string_view base) -> std::string {
	if (profile_for(profiles, base) == nullptr) {
		return std::string(base);
	}
	for (std::uint32_t suffix = 2; suffix < 1000; ++suffix) {
		std::string candidate = std::format("{} {}", base, suffix);
		if (profile_for(profiles, candidate) == nullptr) {
			return candidate;
		}
	}
	return std::string(base);
}
