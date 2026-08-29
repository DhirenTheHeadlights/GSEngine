export module gse.json:reflect;

import std;

import gse.meta;

import :value;
import :parse;
import :write;

export namespace gse::json {
	struct required {};

	struct ignore {};

	struct name {
		char value[48];

		constexpr operator std::string_view() const;
	};

	template <typename E>
	requires std::is_enum_v<E>
	auto name_of(
		E value
	) -> std::string;

	template <typename E>
	requires std::is_enum_v<E>
	auto name_to_enum(
		std::string_view text,
		E& out
	) -> bool;

	template <typename T>
	struct adapter;

	template <typename T>
	concept adapted = requires(const value& node, T& out, const T& in) {
		{ adapter<T>::read(node, out) } -> std::same_as<bool>;
		{ adapter<T>::write(in) } -> std::same_as<value>;
	};

	template <typename T>
	constexpr bool is_string_map = false;

	template <typename V, typename H, typename E, typename A>
	constexpr bool is_string_map<std::unordered_map<std::string, V, H, E, A>> = true;

	template <typename V, typename C, typename A>
	constexpr bool is_string_map<std::map<std::string, V, C, A>> = true;

	template <typename T>
	auto read(
		const value& node,
		T& out
	) -> bool;

	template <typename T>
	auto from(
		const T& in
	) -> value;

	template <typename T>
	auto to(
		const value& node
	) -> std::optional<T>;

	template <typename T>
	auto parse_as(
		std::string_view text
	) -> std::expected<T, parse_error>;

	template <typename T>
	auto stringify(
		const T& in
	) -> std::string;

	template <typename T>
	auto stringify(
		const T& in,
		const write_options& options
	) -> std::string;
}

namespace gse::json {
	template <typename T>
	auto read_members(
		const value& node,
		T& out
	) -> bool;

	template <typename T>
	auto write_members(
		const T& in
	) -> value;
}

constexpr gse::json::name::operator std::string_view() const {
	return value;
}

template <typename E>
requires std::is_enum_v<E>
auto gse::json::name_of(const E value) -> std::string {
	if (enum_has_annotation<name, E>(value)) {
		const name mapped = annotation_from_enum<name>(value, {});
		return std::string(std::string_view(mapped));
	}
	return std::string(enum_to_string(value));
}

template <typename E>
requires std::is_enum_v<E>
auto gse::json::name_to_enum(const std::string_view text, E& out) -> bool {
	if (enum_from_string(text, out)) {
		return true;
	}

	const E candidate = enum_from_annotation<name, std::string_view>(text, E{});
	if (!enum_has_annotation<name, E>(candidate)) {
		return false;
	}

	const name mapped = annotation_from_enum<name>(candidate, {});
	if (std::string_view(mapped) != text) {
		return false;
	}

	out = candidate;
	return true;
}

template <typename T>
auto gse::json::read(const value& node, T& out) -> bool {
	if constexpr (adapted<T>) {
		return adapter<T>::read(node, out);
	}
	else if constexpr (std::same_as<T, value>) {
		out = node;
		return true;
	}
	else if constexpr (std::same_as<T, bool>) {
		if (!node.is_boolean()) {
			return false;
		}
		out = node.boolean();
		return true;
	}
	else if constexpr (std::is_enum_v<T>) {
		if (node.is_string()) {
			return name_to_enum(node.text(), out);
		}
		if (node.is_number()) {
			out = static_cast<T>(node.integer());
			return true;
		}
		return false;
	}
	else if constexpr (std::is_floating_point_v<T>) {
		if (!node.is_number()) {
			return false;
		}
		out = static_cast<T>(node.number());
		return true;
	}
	else if constexpr (std::is_arithmetic_v<T>) {
		if (!node.is_number()) {
			return false;
		}
		out = static_cast<T>(node.integer());
		return true;
	}
	else if constexpr (std::same_as<T, std::string>) {
		if (!node.is_string()) {
			return false;
		}
		out.assign(node.text());
		return true;
	}
	else if constexpr (meta::is_optional_field<T>) {
		if (node.is_null()) {
			out.reset();
			return true;
		}
		typename T::value_type inner{};
		if (!read(node, inner)) {
			return false;
		}
		out = std::move(inner);
		return true;
	}
	else if constexpr (meta::is_list_field<T>) {
		if (!node.is_array()) {
			return false;
		}
		out.clear();
		out.reserve(node.size());
		for (const value& element : node.elements()) {
			typename T::value_type inner{};
			if (!read(element, inner)) {
				return false;
			}
			out.push_back(std::move(inner));
		}
		return true;
	}
	else if constexpr (is_string_map<T>) {
		if (!node.is_object()) {
			return false;
		}
		out.clear();
		const auto keys = node.keys();
		const auto values = node.elements();
		for (std::size_t i = 0; i < keys.size(); ++i) {
			typename T::mapped_type inner{};
			if (!read(values[i], inner)) {
				return false;
			}
			out.emplace(keys[i], std::move(inner));
		}
		return true;
	}
	else if constexpr (std::is_class_v<T>) {
		return read_members(node, out);
	}
	else {
		static_assert(false, "gse::json::read cannot map this type; specialize gse::json::adapter for it");
		return false;
	}
}

template <typename T>
auto gse::json::read_members(const value& node, T& out) -> bool {
	if (!node.is_object()) {
		return false;
	}

	bool complete = true;

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (!has_annotation<ignore>(m)) {
			constexpr std::string_view key = meta::field_key_of<m>();
			if (const value* member = node.find(key)) {
				if (!read(*member, out.[:m:])) {
					complete = false;
				}
			}
			else if constexpr (has_annotation<required>(m)) {
				complete = false;
			}
		}
	}

	return complete;
}

template <typename T>
auto gse::json::from(const T& in) -> value {
	if constexpr (adapted<T>) {
		return adapter<T>::write(in);
	}
	else if constexpr (std::same_as<T, value>) {
		return in;
	}
	else if constexpr (std::same_as<T, bool>) {
		return value(in);
	}
	else if constexpr (std::is_enum_v<T>) {
		return value(name_of(in));
	}
	else if constexpr (std::is_floating_point_v<T>) {
		return value(static_cast<double>(in));
	}
	else if constexpr (std::is_arithmetic_v<T>) {
		return value(static_cast<std::int64_t>(in));
	}
	else if constexpr (std::same_as<T, std::string>) {
		return value(in);
	}
	else if constexpr (meta::is_optional_field<T>) {
		return in ? from(*in) : value{};
	}
	else if constexpr (meta::is_list_field<T>) {
		value out = value::make_array();
		out.reserve(in.size());
		for (const auto& element : in) {
			out.push_back(from(element));
		}
		return out;
	}
	else if constexpr (is_string_map<T>) {
		value out = value::make_object();
		out.reserve(in.size());
		for (const auto& [key, element] : in) {
			out.insert(key, from(element));
		}
		return out;
	}
	else if constexpr (std::is_class_v<T>) {
		return write_members(in);
	}
	else {
		static_assert(false, "gse::json::from cannot map this type; specialize gse::json::adapter for it");
		return {};
	}
}

template <typename T>
auto gse::json::write_members(const T& in) -> value {
	value out = value::make_object();

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (!has_annotation<ignore>(m)) {
			constexpr std::string_view key = meta::field_key_of<m>();
			out.insert(std::string(key), from(in.[:m:]));
		}
	}

	return out;
}

template <typename T>
auto gse::json::to(const value& node) -> std::optional<T> {
	T out{};
	if (!read(node, out)) {
		return std::nullopt;
	}
	return out;
}

template <typename T>
auto gse::json::parse_as(const std::string_view text) -> std::expected<T, parse_error> {
	const auto root = parse(text);
	if (!root) {
		return std::unexpected(root.error());
	}

	T out{};
	if (!read(*root, out)) {
		return std::unexpected(parse_error{
			.code = error::type_mismatch,
			.offset = 0,
		});
	}
	return out;
}

template <typename T>
auto gse::json::stringify(const T& in) -> std::string {
	return write(from(in));
}

template <typename T>
auto gse::json::stringify(const T& in, const write_options& options) -> std::string {
	return write(from(in), options);
}
