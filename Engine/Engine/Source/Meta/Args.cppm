export module gse.meta:args;

import std;
import gse.std_meta;

import :annotations;
import :parse;
import :settings_anno;

export namespace gse {
	template <typename Config>
	auto parse_args(
		int argc,
		char** argv
	) -> Config;
}

namespace gse {
	template <typename T>
	concept arg_value_parseable = requires(std::string_view raw, T v) {
		{ parser<T>::parse(raw, v) } -> std::same_as<bool>; };

	auto build_arg_flag(
		std::string_view prefix,
		std::string_view name
	) -> std::string;

	template <std::meta::info Member, typename T>
	auto apply_arg_clamps(
		T& value
	) -> void;

	template <typename Config>
	auto try_match_arg(
		Config& cfg,
		std::string_view prefix,
		int argc,
		char** argv,
		int& i
	) -> bool;
}

auto gse::build_arg_flag(const std::string_view prefix, const std::string_view name) -> std::string {
	std::string result;
	result.reserve(prefix.size() + 1 + name.size());
	if (!prefix.empty()) {
		result.append(prefix);
		result.push_back('-');
	}
	for (const char c : name) {
		result.push_back(c == '_' ? '-' : c);
	}
	return result;
}

template <std::meta::info Member, typename T>
auto gse::apply_arg_clamps(T& value) -> void {
	{
		constexpr auto ann = meta::find_class_template_annotation(Member, ^^at_least);
		if constexpr (ann != std::meta::info{}) {
			constexpr auto lo = [:ann:] ::value;
			if (value < lo) {
				value = lo;
			}
		}
	}
	{
		constexpr auto ann = meta::find_class_template_annotation(Member, ^^at_most);
		if constexpr (ann != std::meta::info{}) {
			constexpr auto hi = [:ann:] ::value;
			if (value > hi) {
				value = hi;
			}
		}
	}
	{
		constexpr auto ann = meta::find_class_template_annotation(Member, ^^within);
		if constexpr (ann != std::meta::info{}) {
			constexpr auto lo = [:ann:] ::min;
			constexpr auto hi = [:ann:] ::max;
			if (value < lo) {
				value = lo;
			}
			if (value > hi) {
				value = hi;
			}
		}
	}
}

template <typename Config>
auto gse::try_match_arg(Config& cfg, const std::string_view prefix, const int argc, char** argv, int& i) -> bool {
	const std::string_view arg(argv[i]);
	bool matched = false;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Config, std::meta::access_context::unchecked()))) {
		if (!matched) {
			using F = [:std::meta::type_of(m):];
			const std::string flag = build_arg_flag(prefix, std::meta::identifier_of(m));
			const std::string positive = "--" + flag;

			if constexpr (std::same_as<F, bool>) {
				const std::string negative = "--no-" + flag;
				if (arg == positive) {
					cfg.[:m:] = true;
					++i;
					matched = true;
				}
				else if (arg == negative) {
					cfg.[:m:] = false;
					++i;
					matched = true;
				}
			}
			else if constexpr (arg_value_parseable<F>) {
				if (arg == positive && i + 1 < argc) {
					F tmp = cfg.[:m:];
					if (parse(argv[i + 1], tmp)) {
						apply_arg_clamps<m>(tmp);
						cfg.[:m:] = tmp;
						i += 2;
						matched = true;
					}
				}
			}
			else if constexpr (std::is_class_v<F> && std::is_aggregate_v<F>) {
				if (try_match_arg(cfg.[:m:], flag, argc, argv, i)) {
					matched = true;
				}
			}
		}
	}
	return matched;
}

template <typename Config>
auto gse::parse_args(const int argc, char** argv) -> Config {
	Config result{};
	int i = 1;
	while (i < argc) {
		const int prev = i;
		try_match_arg(
			result,
			{},
			argc,
			argv,
			i
		);
		if (i == prev) {
			++i;
		}
	}
	return result;
}
