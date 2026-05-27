export module gse.meta:format;

import std;
import gse.std_meta;

import :annotations;

export namespace gse {
	struct format_skip_tag {};
	constexpr format_skip_tag format_skip{};
}

namespace gse::internal {
	consteval auto is_user_namespace(
		std::meta::info type
	) -> bool;

	inline thread_local int format_depth = 0;

	template <typename T, typename CharT>
	concept reflectable_user_class =
		(std::is_class_v<T> && !std::is_polymorphic_v<T> && std::is_same_v<CharT, char> && is_user_namespace(^^T));
}

consteval auto gse::internal::is_user_namespace(std::meta::info type) -> bool {
	const auto entity = std::meta::has_template_arguments(type) ? std::meta::template_of(type) : type;
	auto parent = std::meta::parent_of(entity);
	while (std::meta::has_identifier(parent)) {
		const auto name = std::meta::identifier_of(parent);
		if (name == "std" || name.starts_with("__")) {
			return false;
		}
		parent = std::meta::parent_of(parent);
	}
	return true;
}

export template <typename T, typename CharT>
requires gse::internal::reflectable_user_class<T, CharT>
struct std::formatter<T, CharT> {
	template <typename ParseContext>
	constexpr auto parse(
		ParseContext& ctx
	);

	template <typename FormatContext>
	auto format(
		const T& value,
		FormatContext& ctx
	) const;
};

template <typename T, typename CharT>
requires gse::internal::reflectable_user_class<T, CharT>
template <typename ParseContext>
constexpr auto std::formatter<T, CharT>::parse(ParseContext& ctx) {
	return ctx.begin();
}

template <typename T, typename CharT>
requires gse::internal::reflectable_user_class<T, CharT>
template <typename FormatContext>
auto std::formatter<T, CharT>::format(const T& value, FormatContext& ctx) const {
	++gse::internal::format_depth;
	const auto inner = std::string(static_cast<std::size_t>(gse::internal::format_depth) * 2, ' ');
	const auto outer = std::string(
		static_cast<std::size_t>(gse::internal::format_depth - 1) * 2,
		' '
	);
	auto out = std::format_to(ctx.out(), "{{");
	bool first = true;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (!gse::has_annotation<gse::format_skip_tag>(m)) {
			if (!first) {
				out = std::format_to(out, ",");
			}
			first = false;
			out = std::format_to(out, "\n{}", inner);
			using member_t = [:std::meta::type_of(m):];
			constexpr auto name = std::meta::identifier_of(m);
			if constexpr (std::is_pointer_v<member_t>) {
				using pointee_t = std::remove_cv_t<std::remove_pointer_t<member_t>>;
				const auto* ptr = value.[:m:];
				if (ptr == nullptr) {
					out = std::format_to(out, "{}=nullptr", name);
				}
				else if constexpr (std::is_void_v<pointee_t>) {
					out = std::format_to(out, "{}={}", name, ptr);
				}
				else if constexpr (std::formattable<pointee_t, char>) {
					out = std::format_to(out, "{}=&{}", name, *ptr);
				}
				else {
					out = std::format_to(out, "{}=&{}", name, static_cast<const void*>(ptr));
				}
			}
			else if constexpr (std::formattable<member_t, char>) {
				out = std::format_to(out, "{}={}", name, value.[:m:]);
			}
			else {
				out = std::format_to(out, "{}=<unprintable>", name);
			}
		}
	}
	if (!first) {
		out = std::format_to(out, "\n{}", outer);
	}
	out = std::format_to(out, "}}");
	--gse::internal::format_depth;
	return out;
}
