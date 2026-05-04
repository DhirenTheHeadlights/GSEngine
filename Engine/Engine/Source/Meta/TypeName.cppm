export module gse.meta:type_name;

import std;
import gse.std_meta;

export namespace gse::meta {
	template <typename T>
	consteval auto qualified_name(
	) -> std::string_view;

	template <typename T>
	consteval auto unqualified_name(
	) -> std::string_view;

	template <typename T>
	consteval auto namespace_name(
	) -> std::string_view;
}

template <typename T>
consteval auto gse::meta::qualified_name() -> std::string_view {
	auto walk = [](this auto self, std::meta::info entity) -> std::string {
		if (!std::meta::has_identifier(entity)) {
			return std::string(std::meta::display_string_of(entity));
		}

		const auto parent = std::meta::has_template_arguments(entity)
			? std::meta::parent_of(std::meta::template_of(entity))
			: std::meta::parent_of(entity);

		std::string parent_name = self(parent);
		std::string my_name = std::string(std::meta::identifier_of(entity));

		if (std::meta::has_template_arguments(entity)) {
			my_name += "<";
			bool first = true;
			for (auto arg : std::meta::template_arguments_of(entity)) {
				if (!first) {
					my_name += ", ";
				}
				first = false;
				if (std::meta::is_type(arg)) {
					my_name += self(arg);
				}
				else {
					my_name += std::meta::display_string_of(arg);
				}
			}
			my_name += ">";
		}

		if (parent_name.empty()) {
			return my_name;
		}
		return parent_name + "::" + my_name;
	};

	auto to_str = [](std::size_t n) -> std::string {
		if (n == 0) {
			return "0";
		}
		std::string out;
		while (n != 0) {
			out.push_back(static_cast<char>('0' + (n % 10)));
			n /= 10;
		}
		std::ranges::reverse(out);
		return out;
	};

	const auto walked = walk(std::meta::dealias(^^T));
	return std::define_static_string(walked + "#sz" + to_str(sizeof(T)) + ".al" + to_str(alignof(T)));
}

template <typename T>
consteval auto gse::meta::unqualified_name() -> std::string_view {
	constexpr auto entity = std::meta::dealias(^^T);
	if constexpr (std::meta::has_identifier(entity)) {
		return std::define_static_string(std::meta::identifier_of(entity));
	}
	else {
		return std::define_static_string(std::meta::display_string_of(entity));
	}
}

template <typename T>
consteval auto gse::meta::namespace_name() -> std::string_view {
	const auto full = qualified_name<T>();
	const auto last_colon = full.rfind("::");
	if (last_colon == std::string_view::npos) {
		return std::string_view{};
	}
	return std::define_static_string(full.substr(0, last_colon));
}
