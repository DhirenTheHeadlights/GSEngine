export module gse.meta:annotations;

import std;

export namespace gse {
	template <auto V>
	struct at_least {
		static constexpr auto value = V;
	};

	template <auto V>
	struct at_most {
		static constexpr auto value = V;
	};

	template <auto Lo, auto Hi>
	struct within {
		static constexpr auto min = Lo;
		static constexpr auto max = Hi;
	};

	template <typename Tag>
	consteval auto has_annotation(
		std::meta::info member
	) -> bool;

	consteval auto first_annotation_of_type(
		std::meta::info enumerator,
		std::meta::info annotation_type
	) -> std::meta::info;

	template <typename E>
	consteval auto first_annotated_enumerator() -> std::meta::info;

	template <typename E>
	consteval auto first_enumerator_has_annotation() -> bool;

	template <typename E>
	consteval auto any_enumerator_has_annotation() -> bool;

	template <typename AnnotationType, typename MatchType, typename Enum>
	constexpr auto enum_from_annotation(const MatchType& value, const Enum default_value) -> Enum;

	template <typename AnnotationType, typename Enum>
	constexpr auto annotation_from_enum(const Enum value, const AnnotationType default_value) -> AnnotationType;

	template <typename AnnotationType, typename Enum>
	constexpr auto enum_has_annotation(const Enum value) -> bool;

	template <typename Source, typename Tag>
	struct project_holder;

	template <typename Source, typename Tag>
	using project_by_annotation = typename project_holder<Source, Tag>::type;

	template <typename Source, typename Tag>
	struct project_holder {
		struct type;

		consteval {
			std::vector<std::meta::info> members;
			const auto all = std::meta::nonstatic_data_members_of(^^Source, std::meta::access_context::unchecked());
			for (auto m : all) {
				if (has_annotation<Tag>(m)) {
					auto m_type = std::meta::type_of(m);

					bool has_nested = false;
					if (std::meta::is_class_type(m_type) || std::meta::is_union_type(m_type)) {
						for (auto sm : std::meta::nonstatic_data_members_of(m_type, std::meta::access_context::unchecked())) {
							if (has_annotation<Tag>(sm)) {
								has_nested = true;
								break;
							}
						}
					}

					if (has_nested) {
						m_type = std::meta::substitute(
							^^project_by_annotation,
							{
								m_type,
								^^Tag }
						);
					}

					members.push_back(std::meta::data_member_spec(
						m_type,
						{
							.name = std::meta::identifier_of(m),
						}
					));
				}
			}
			std::meta::define_aggregate(^^type, members);
		}
	};

	template <typename Schema>
	consteval auto find_field_by_name(
		std::string_view name
	) -> std::meta::info;

	template <typename Schema, typename T>
	consteval auto apply_annotations() -> std::optional<Schema>;
}

template <typename Tag>
consteval auto gse::has_annotation(const std::meta::info member) -> bool {
	constexpr auto tag_type = std::meta::remove_cvref(std::meta::dealias(^^Tag));
	for (auto ann : std::meta::annotations_of(member)) {
		const auto ann_type = std::meta::remove_cvref(std::meta::dealias(std::meta::type_of(ann)));
		if (std::meta::is_same_type(ann_type, tag_type)) {
			return true;
		}
	}
	return false;
}

consteval auto gse::first_annotation_of_type(const std::meta::info enumerator, const std::meta::info annotation_type) -> std::meta::info {
	const std::meta::info requested_type = std::meta::remove_cvref(std::meta::dealias(annotation_type));
	for (std::meta::info ann : std::meta::annotations_of(enumerator)) {
		const std::meta::info ann_type = std::meta::remove_cvref(std::meta::dealias(std::meta::type_of(ann)));
		if (std::meta::is_same_type(ann_type, requested_type)) {
			return ann;
		}
	}
	return {};
}

template <typename E>
consteval auto gse::first_annotated_enumerator() -> std::meta::info {
	if constexpr (std::is_enum_v<E>) {
		for (auto e : std::meta::enumerators_of(^^E)) {
			if (!std::meta::annotations_of(e).empty()) {
				return e;
			}
		}
	}
	return {};
}

template <typename E>
consteval auto gse::first_enumerator_has_annotation() -> bool {
	if constexpr (std::is_enum_v<E>) {
		auto enums = std::meta::enumerators_of(^^E);
		if (enums.empty()) {
			return false;
		}
		return !std::meta::annotations_of(enums[0]).empty();
	}
	return false;
}

template <typename E>
consteval auto gse::any_enumerator_has_annotation() -> bool {
	if constexpr (std::is_enum_v<E>) {
		for (auto e : std::meta::enumerators_of(^^E)) {
			if (!std::meta::annotations_of(e).empty()) {
				return true;
			}
		}
	}
	return false;
}

template <typename Schema>
consteval auto gse::find_field_by_name(const std::string_view name) -> std::meta::info {
	for (auto field : std::meta::nonstatic_data_members_of(^^Schema, std::meta::access_context::unchecked())) {
		if (std::meta::identifier_of(field) == name) {
			return field;
		}
	}
	return std::meta::info{};
}

namespace gse {
	template <typename T>
	consteval auto annotations_array() {
		constexpr auto count = std::meta::annotations_of(^^T).size();
		std::array<std::meta::info, count> out{};
		const auto annotations = std::meta::annotations_of(^^T);
		for (std::size_t i = 0; i < count; ++i) {
			out[i] = annotations[i];
		}
		return out;
	}
}

template <typename Schema, typename T>
consteval auto gse::apply_annotations() -> std::optional<Schema> {
	Schema out{};
	bool any = false;

	template for (constexpr auto ann : annotations_array<T>()) {
		constexpr auto ann_type = std::meta::dealias(std::meta::type_of(ann));
		constexpr auto key = std::meta::has_template_arguments(ann_type) ? std::meta::template_of(ann_type) : ann_type;
		constexpr auto matched_field = find_field_by_name<Schema>(std::meta::identifier_of(key));

		if constexpr (matched_field != std::meta::info{}) {
			out.[:matched_field:] = [:ann_type:] ::value;
			any = true;
		}
	}

	if (any) {
		return out;
	}
	return std::nullopt;
}

template <typename AnnotationType, typename MatchType, typename Enum>
constexpr auto gse::enum_from_annotation(const MatchType& value, const Enum default_value) -> Enum {
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
		constexpr auto ann = first_annotation_of_type(e, ^^AnnotationType);
		if constexpr (ann != std::meta::info{}) {
			if (static_cast<MatchType>([:std::meta::constant_of(ann):]) == value) {
				return [:e:];
			}
		}
	}
	return default_value;
}

template <typename AnnotationType, typename Enum>
constexpr auto gse::annotation_from_enum(const Enum value, const AnnotationType default_value) -> AnnotationType {
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
		constexpr auto ann = first_annotation_of_type(e, ^^AnnotationType);
		if constexpr (ann != std::meta::info{}) {
			if ([:e:] == value) {
				return [:std::meta::constant_of(ann):];
			}
		}
	}
	return default_value;
}

template <typename AnnotationType, typename Enum>
constexpr auto gse::enum_has_annotation(const Enum value) -> bool {
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
		constexpr bool present = first_annotation_of_type(e, ^^AnnotationType) != std::meta::info{};
		if ([:e:] == value) {
			return present;
		}
	}
	return false;
}

