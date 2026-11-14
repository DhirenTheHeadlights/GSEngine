module gse.runtime:settings_setup;

import std;

import gse.save;

import :engine;

namespace gse {
	auto configure_settings(
		save::registry& save,
		const engine_config& options,
		std::string_view title
	) -> void;
}
