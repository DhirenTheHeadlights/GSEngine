export module gse.meta:args;

import std;

import :annotations;
import :parse;
import :settings_anno;

export namespace gse {
	template <typename Config>
	auto parse_args(
		int argc,
		char** argv,
		std::vector<std::string>& passed
	) -> Config;

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

	template <typename F>
	constexpr bool arg_is_flag = std::same_as<F, bool>;

	template <typename F>
	constexpr bool arg_is_valued = !arg_is_flag<F> && arg_value_parseable<F>;

	template <typename F>
	constexpr bool arg_is_group = !arg_is_flag<F> && !arg_value_parseable<F> && std::is_class_v<F> && std::is_aggregate_v<F>;

	enum class arg_match : std::uint8_t {
		none,
		consumed,
		bad_value,
	};

	struct arg_flag_info {
		std::string flag;
		std::string value_hint;
	};

	auto build_arg_flag(
		std::string_view prefix,
		std::string_view name
	) -> std::string;

	template <typename F>
	consteval auto arg_value_hint() -> std::string_view;

	template <typename Config>
	auto collect_arg_flags(
		std::string_view prefix,
		std::vector<arg_flag_info>& out
	) -> void;

	auto arg_edit_distance(
		std::string_view a,
		std::string_view b
	) -> std::size_t;

	auto print_arg_usage(
		std::span<const arg_flag_info> flags
	) -> void;

	[[noreturn]] auto fail_unrecognized_arg(
		std::string_view arg,
		std::span<const arg_flag_info> flags
	) -> void;

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
	) -> arg_match;
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

template <typename F>
consteval auto gse::arg_value_hint() -> std::string_view {
	if constexpr (std::same_as<F, bool>) {
		return "[true|false]";
	}
	else if constexpr (std::is_enum_v<F>) {
		return "<name>";
	}
	else if constexpr (std::is_floating_point_v<F>) {
		return "<number>";
	}
	else if constexpr (std::is_integral_v<F>) {
		return "<int>";
	}
	else if constexpr (std::same_as<F, std::vector<std::string>>) {
		return "<text>  (repeatable)";
	}
	else {
		return "<text>";
	}
}

template <typename Config>
auto gse::collect_arg_flags(const std::string_view prefix, std::vector<arg_flag_info>& out) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Config, std::meta::access_context::unchecked()))) {
		using F = [:std::meta::type_of(m):];
		const std::string flag = build_arg_flag(prefix, std::meta::identifier_of(m));

		if constexpr (arg_is_flag<F>) {
			out.push_back({ .flag = "--" + flag, .value_hint = std::string(arg_value_hint<F>()) });
			out.push_back({ .flag = "--no-" + flag, .value_hint = {} });
		}
		if constexpr (arg_is_valued<F>) {
			out.push_back({ .flag = "--" + flag, .value_hint = std::string(arg_value_hint<F>()) });
		}
		if constexpr (arg_is_group<F>) {
			collect_arg_flags<F>(flag, out);
		}
	}
}

auto gse::arg_edit_distance(const std::string_view a, const std::string_view b) -> std::size_t {
	std::vector<std::size_t> previous(b.size() + 1);
	std::vector<std::size_t> current(b.size() + 1);
	std::ranges::iota(previous, std::size_t{ 0 });

	for (std::size_t i = 0; i < a.size(); ++i) {
		current[0] = i + 1;
		for (std::size_t j = 0; j < b.size(); ++j) {
			const std::size_t substitution = previous[j] + (a[i] == b[j] ? 0 : 1);
			current[j + 1] = std::min({ current[j] + 1, previous[j + 1] + 1, substitution });
		}
		previous.swap(current);
	}
	return previous[b.size()];
}

auto gse::print_arg_usage(const std::span<const arg_flag_info> flags) -> void {
	std::cout << "options:\n";
	for (const auto& info : flags) {
		if (info.value_hint.empty()) {
			std::cout << std::format("  {}\n", info.flag);
		}
		else {
			std::cout << std::format("  {} {}\n", info.flag, info.value_hint);
		}
	}
}

auto gse::fail_unrecognized_arg(const std::string_view arg, const std::span<const arg_flag_info> flags) -> void {
	std::cerr << std::format("error: unrecognized command-line argument '{}'\n", arg);

	constexpr std::size_t max_distance = 3;
	constexpr std::size_t max_suggestions = 5;

	std::vector<std::pair<std::size_t, std::string_view>> scored;
	for (const auto& info : flags) {
		if (const std::size_t distance = arg_edit_distance(arg, info.flag); distance <= max_distance) {
			scored.emplace_back(distance, info.flag);
		}
	}
	std::ranges::sort(scored);
	if (scored.size() > max_suggestions) {
		scored.resize(max_suggestions);
	}

	std::vector<std::string_view> near;
	for (const auto& candidate : std::views::values(scored)) {
		near.push_back(candidate);
	}
	if (!near.empty()) {
		std::cerr << "did you mean:\n";
		for (const auto candidate : near) {
			std::cerr << std::format("  {}\n", candidate);
		}
	}
	else {
		print_arg_usage(flags);
	}
	std::exit(2);
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
auto gse::try_match_arg(Config& cfg, const std::string_view prefix, const int argc, char** argv, int& i) -> arg_match {
	const std::string_view arg(argv[i]);
	arg_match outcome = arg_match::none;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Config, std::meta::access_context::unchecked()))) {
		if (outcome == arg_match::none) {
			using F = [:std::meta::type_of(m):];
			const std::string flag = build_arg_flag(prefix, std::meta::identifier_of(m));
			const std::string positive = "--" + flag;

			if constexpr (arg_is_flag<F>) {
				const std::string negative = "--no-" + flag;
				if (arg == positive) {
					bool value = true;
					if (i + 1 < argc && parser<bool>::parse(argv[i + 1], value)) {
						i += 2;
					}
					else {
						value = true;
						++i;
					}
					cfg.[:m:] = value;
					outcome = arg_match::consumed;
				}
				else if (arg == negative) {
					cfg.[:m:] = false;
					++i;
					outcome = arg_match::consumed;
				}
			}
			if constexpr (arg_is_valued<F>) {
				if (arg == positive) {
					if (i + 1 >= argc) {
						std::cerr << std::format("error: {} expects a value {}\n", positive, arg_value_hint<F>());
						outcome = arg_match::bad_value;
					}
					else if (F tmp = cfg.[:m:]; parse(argv[i + 1], tmp)) {
						apply_arg_clamps<m>(tmp);
						cfg.[:m:] = tmp;
						i += 2;
						outcome = arg_match::consumed;
					}
					else {
						std::cerr << std::format(
							"error: {} could not parse '{}'; expected {}\n",
							positive,
							argv[i + 1],
							arg_value_hint<F>()
						);
						outcome = arg_match::bad_value;
					}
				}
			}
			if constexpr (arg_is_group<F>) {
				outcome = try_match_arg(cfg.[:m:], flag, argc, argv, i);
			}
		}
	}
	return outcome;
}

template <typename Config>
auto gse::parse_args(const int argc, char** argv, std::vector<std::string>& passed) -> Config {
	std::vector<arg_flag_info> flags;
	collect_arg_flags<Config>({}, flags);

	Config result{};
	int i = 1;
	while (i < argc) {
		const std::string_view arg(argv[i]);
		if (arg == "--help" || arg == "-h") {
			print_arg_usage(flags);
			std::exit(0);
		}
		switch (try_match_arg(result, {}, argc, argv, i)) {
			case arg_match::consumed:
				passed.emplace_back(arg);
				break;
			case arg_match::bad_value:
				std::exit(2);
			case arg_match::none:
				fail_unrecognized_arg(arg, flags);
		}
	}
	return result;
}

template <typename Config>
auto gse::parse_args(const int argc, char** argv) -> Config {
	std::vector<std::string> passed;
	return parse_args<Config>(argc, argv, passed);
}
