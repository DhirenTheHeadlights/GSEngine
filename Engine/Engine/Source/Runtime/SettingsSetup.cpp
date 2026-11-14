module gse.runtime:settings_setup_impl;

import gse.config;
import gse.fs;
import gse.graphics;
import gse.log;
import gse.os;
import gse.save;
import std;

import :engine;
import :settings_setup;

namespace gse {
	auto seed_project_settings(
		const std::filesystem::path& project_settings_path
	) -> void;
}

auto gse::seed_project_settings(const std::filesystem::path& project_settings_path) -> void {
	const std::filesystem::path shipped_settings = config::shipped_project_settings_path();
	std::error_code shipped_ec;
	if (shipped_settings.empty() || project_settings_path.empty() || !std::filesystem::exists(shipped_settings, shipped_ec) || std::filesystem::exists(project_settings_path, shipped_ec)) {
		return;
	}

	if (const auto seeded = fs::copy_file(shipped_settings, project_settings_path); seeded) {
		log::println(log::category::save_system, "seeded project settings from {}", shipped_settings.generic_display_string());
	}
	else {
		log::println(log::level::warning, log::category::save_system, "could not seed project settings from {}: {}", shipped_settings.generic_display_string(), seeded.error());
	}
}

auto gse::configure_settings(save::registry& save, const engine_config& options, const std::string_view title) -> void {
	seed_project_settings(options.project_settings_path);

	save.set_paths({
		.user = config::user_config_dir() / std::format("{}.ini", config::executable_stem()),
		.project = options.project_settings_path,
	});
	log::println(
		log::category::save_system,
		"Settings paths: user={} project={}",
		save.user_path().generic_display_string(),
		save.project_path().empty() ? std::string("<none>") : save.project_path().generic_display_string()
	);
	if (options.load_settings) {
		save.load();
	}
	save.set_auto_save(options.persist_settings);

	save.pin<^^window::data::title>(std::string(title));
	save.pin<^^gui::data::restore_layout>(options.load_settings);
	save.pin<^^gui::data::persist_layout>(options.persist_settings);
	for (const auto& [category, key, value] : options.defaults) {
		save.set_app_default(category, key, value);
	}
	for (const auto& [category, key, value] : options.pins) {
		save.set_pin(category, key, value);
	}

	if (options.attached) {
		save.pin<^^window::data::cursor_suppressed>(true);
		save.pin<^^window::data::attached>(true);
	}

	save.set_overrides(options.setting);
}
