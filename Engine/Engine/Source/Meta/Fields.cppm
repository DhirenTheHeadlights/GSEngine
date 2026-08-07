export module gse.meta:fields;

import std;

import :annotations;
import :fixed_string;
import :parse;
import :settings_anno;

export namespace gse {
	template <fixed_string V>
	struct field_key {
		static constexpr std::string_view value = V;
	};
}

export namespace gse::meta {
	template <typename T>
	constexpr bool is_optional_field = false;

	template <typename T>
	constexpr bool is_optional_field<std::optional<T>> = true;

	template <typename T>
	constexpr bool is_list_field = false;

	template <typename T, typename A>
	constexpr bool is_list_field<std::vector<T, A>> = true;

	consteval auto find_key(
		std::meta::info m
	) -> std::meta::info;

	template <std::meta::info M>
	consteval auto field_key_of() -> std::string_view;

	template <typename T>
	auto read_field(
		std::string_view raw,
		T& out
	) -> bool;

	template <typename T>
	auto write_field(
		const T& value
	) -> std::string;

	template <typename T, typename Values>
	auto read_fields(
		const Values& values,
		T& out
	) -> void;

	template <typename T, typename Values>
	auto write_fields(
		const T& value,
		Values& values
	) -> void;
}

consteval auto gse::meta::find_key(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^field_key);
}

template <std::meta::info M>
consteval auto gse::meta::field_key_of() -> std::string_view {
	constexpr auto found = find_key(M);
	if constexpr (found != std::meta::info{}) {
		using K = [:found:];
		return K::value;
	}
	else {
		return member_name(M);
	}
}

template <typename T>
auto gse::meta::read_field(const std::string_view raw, T& out) -> bool {
	if constexpr (is_optional_field<T>) {
		typename T::value_type inner{};
		if (!read_field(raw, inner)) {
			return false;
		}
		out = std::move(inner);
		return true;
	}
	else if constexpr (is_list_field<T>) {
		out.clear();
		for (const auto& part : std::views::split(raw, ',')) {
			typename T::value_type inner{};
			if (const std::string_view piece(part); !piece.empty() && read_field(piece, inner)) {
				out.push_back(std::move(inner));
			}
		}
		return true;
	}
	else {
		return parse(raw, out);
	}
}

template <typename T>
auto gse::meta::write_field(const T& value) -> std::string {
	if constexpr (is_optional_field<T>) {
		return value ? write_field(*value) : std::string{};
	}
	else if constexpr (is_list_field<T>) {
		std::string out;
		for (const auto& element : value) {
			if (!out.empty()) {
				out.push_back(',');
			}
			out.append(write_field(element));
		}
		return out;
	}
	else {
		return std::format("{}", value);
	}
}

template <typename T, typename Values>
auto gse::meta::read_fields(const Values& values, T& out) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		constexpr std::string_view name = field_key_of<m>();
		if (const auto it = values.find(std::string(name)); it != values.end()) {
			read_field(it->second, out.[:m:]);
		}
	}
}

template <typename T, typename Values>
auto gse::meta::write_fields(const T& value, Values& values) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		constexpr std::string_view name = field_key_of<m>();
		if (std::string formatted = write_field(value.[:m:]); !formatted.empty()) {
			values[std::string(name)] = std::move(formatted);
		}
	}
}
