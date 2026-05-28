export module ide:command_registry;

import std;
import gse;
import gse.std_meta;

import :annotations;

export namespace ide {
	struct command_descriptor {
		std::string_view name;
		std::string_view default_shortcut;
		std::string_view description;
		std::string_view when;
		std::function<void()> invoke;
	};

	struct command_registry {
		std::vector<command_descriptor> all;
		std::unordered_map<std::string, std::size_t> by_name;
		std::unordered_map<std::string, std::size_t> by_shortcut;

		auto add(command_descriptor desc) -> void;
		auto invoke_by_name(std::string_view name) const -> bool;
		auto invoke_by_shortcut(std::string_view shortcut) const -> bool;
	};

	template <std::meta::info NamespaceInfo>
	auto discover_commands(command_registry& reg) -> void;
}

auto ide::command_registry::add(command_descriptor desc) -> void {
	const std::size_t index = all.size();
	if (!desc.name.empty()) {
		by_name.emplace(std::string(desc.name), index);
	}
	if (!desc.default_shortcut.empty()) {
		by_shortcut.emplace(std::string(desc.default_shortcut), index);
	}
	all.push_back(std::move(desc));
}

auto ide::command_registry::invoke_by_name(std::string_view name) const -> bool {
	const auto it = by_name.find(std::string(name));
	if (it == by_name.end()) {
		return false;
	}
	all[it->second].invoke();
	return true;
}

auto ide::command_registry::invoke_by_shortcut(std::string_view shortcut) const -> bool {
	const auto it = by_shortcut.find(std::string(shortcut));
	if (it == by_shortcut.end()) {
		return false;
	}
	all[it->second].invoke();
	return true;
}

template <std::meta::info NamespaceInfo>
auto ide::discover_commands(command_registry& reg) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::members_of(NamespaceInfo, std::meta::access_context::unchecked()))) {
		constexpr auto ann_name = gse::meta::find_class_template_annotation(m, ^^ide::name);
		if constexpr (ann_name != std::meta::info{}) {
			command_descriptor cmd{};
			cmd.name = [:ann_name:]::value;
			cmd.invoke = []() {
				[:m:]();
			};

			constexpr auto ann_shortcut = gse::meta::find_class_template_annotation(m, ^^ide::shortcut);
			if constexpr (ann_shortcut != std::meta::info{}) {
				cmd.default_shortcut = [:ann_shortcut:]::value;
			}

			constexpr auto ann_description = gse::meta::find_class_template_annotation(m, ^^ide::description);
			if constexpr (ann_description != std::meta::info{}) {
				cmd.description = [:ann_description:]::value;
			}

			constexpr auto ann_when = gse::meta::find_class_template_annotation(m, ^^ide::when);
			if constexpr (ann_when != std::meta::info{}) {
				cmd.when = [:ann_when:]::value;
			}

			reg.add(std::move(cmd));
		}
	}
}
