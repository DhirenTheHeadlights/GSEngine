export module gse.ide.build:configs;

import std;
import gse;
import gse.ide.config;

export namespace gse::ide::build_runner {
	struct build_config {
		std::string name;
		std::string label;
		std::string directory;
		std::string source_relative;
	};

	auto build_configs() -> std::span<const build_config>;

	auto build_config_for(
		std::string_view name
	) -> const build_config*;

	auto active_build_config() -> std::string_view;

	auto build_config_label(
		std::string_view name
	) -> std::string_view;
}

namespace gse::ide::build_runner {
	constexpr std::uint32_t max_preset_depth = 8;

	auto presets_path() -> std::filesystem::path;

	auto preset_binary_dir(
		const json::value& presets,
		std::string_view name,
		std::uint32_t depth
	) -> std::string_view;

	auto read_build_configs() -> std::vector<build_config>;

	auto resolve_active_build_config() -> std::string;
}

auto gse::ide::build_runner::presets_path() -> std::filesystem::path {
	return config::engine_root() / "CMakePresets.json";
}

auto gse::ide::build_runner::preset_binary_dir(const json::value& presets, const std::string_view name, const std::uint32_t depth) -> std::string_view {
	if (depth > max_preset_depth) {
		return {};
	}

	for (const json::value& preset : presets.elements()) {
		const json::value* preset_name = preset.find("name");
		if (preset_name == nullptr || preset_name->text() != name) {
			continue;
		}
		if (const json::value* directory = preset.find("binaryDir")) {
			return directory->text();
		}

		const json::value* inherits = preset.find("inherits");
		if (inherits == nullptr) {
			return {};
		}
		if (inherits->is_string()) {
			return preset_binary_dir(presets, inherits->text(), depth + 1);
		}
		for (const json::value& parent : inherits->elements()) {
			if (const std::string_view found = preset_binary_dir(presets, parent.text(), depth + 1); !found.empty()) {
				return found;
			}
		}
		return {};
	}
	return {};
}

auto gse::ide::build_runner::read_build_configs() -> std::vector<build_config> {
	const std::filesystem::path path = presets_path();
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		log::println(log::level::warning, log::category::task, "build: '{}' is missing, so only the running configuration is offered", path.generic_display_string());
		return {};
	}

	const std::string text{ std::istreambuf_iterator(in), std::istreambuf_iterator<char>() };
	const std::expected<json::value, json::parse_error> root = json::parse(text);
	if (!root) {
		log::println(log::level::warning, log::category::task, "build: '{}' is not readable json at offset {}: {}", path.generic_display_string(), root.error().offset, json::message(root.error().code));
		return {};
	}

	const json::value* presets = root->find("configurePresets");
	if (presets == nullptr || !presets->is_array()) {
		return {};
	}

	std::vector<build_config> out;
	for (const json::value& preset : presets->elements()) {
		const json::value* name = preset.find("name");
		if (name == nullptr || !name->is_string()) {
			continue;
		}
		const json::value* hidden = preset.find("hidden");
		if (hidden != nullptr && hidden->boolean()) {
			continue;
		}

		const std::string_view binary_dir = preset_binary_dir(*presets, name->text(), 0);
		if (binary_dir.empty()) {
			continue;
		}
		const std::size_t slash = binary_dir.find_last_of("/\\");
		const json::value* label = preset.find("displayName");
		constexpr std::string_view source_macro = "${sourceDir}/";

		out.push_back({
			.name = std::string(name->text()),
			.label = label != nullptr && label->is_string() ? std::string(label->text()) : std::string(name->text()),
			.directory = std::string(slash == std::string_view::npos ? binary_dir : binary_dir.substr(slash + 1)),
			.source_relative = binary_dir.starts_with(source_macro) ? std::string(binary_dir.substr(source_macro.size())) : std::string{},
		});
	}
	return out;
}

auto gse::ide::build_runner::build_configs() -> std::span<const build_config> {
	static const std::vector<build_config> value = read_build_configs();
	return value;
}

auto gse::ide::build_runner::build_config_for(const std::string_view name) -> const build_config* {
	const std::span<const build_config> configs = build_configs();
	const auto found = std::ranges::find(configs, name, &build_config::name);
	return found != configs.end() ? &*found : nullptr;
}

auto gse::ide::build_runner::resolve_active_build_config() -> std::string {
	const std::string directory = gse::config::build_root().filename().generic_display_string();
	const std::span<const build_config> configs = build_configs();
	const auto found = std::ranges::find(configs, directory, &build_config::directory);
	return found != configs.end() ? found->name : std::string{};
}

auto gse::ide::build_runner::active_build_config() -> std::string_view {
	static const std::string value = resolve_active_build_config();
	return value;
}

auto gse::ide::build_runner::build_config_label(const std::string_view name) -> std::string_view {
	const build_config* found = build_config_for(name);
	return found != nullptr ? std::string_view(found->label) : name;
}
