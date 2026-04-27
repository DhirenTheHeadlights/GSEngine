module;

#include <meta>

export module gse.std_meta;

export namespace std {
	using ::std::define_static_array;
	using ::std::define_static_string;
}

export namespace std::meta {
	using ::std::meta::info;
	using ::std::meta::access_context;
	using ::std::meta::operators;

	using ::std::meta::identifier_of;
	using ::std::meta::display_string_of;
	using ::std::meta::u8identifier_of;
	using ::std::meta::u8display_string_of;
	using ::std::meta::has_identifier;
	using ::std::meta::operator_of;
	using ::std::meta::source_location_of;

	using ::std::meta::type_of;
	using ::std::meta::parent_of;
	using ::std::meta::dealias;
	using ::std::meta::object_of;
	using ::std::meta::constant_of;
	using ::std::meta::template_of;
	using ::std::meta::template_arguments_of;
	using ::std::meta::has_template_arguments;

	using ::std::meta::members_of;
	using ::std::meta::bases_of;
	using ::std::meta::static_data_members_of;
	using ::std::meta::nonstatic_data_members_of;
	using ::std::meta::enumerators_of;
	using ::std::meta::parameters_of;
	using ::std::meta::annotations_of;

	using ::std::meta::can_substitute;
	using ::std::meta::substitute;
	using ::std::meta::reflect_constant;
	using ::std::meta::reflect_object;
	using ::std::meta::reflect_function;
	using ::std::meta::extract;

	using ::std::meta::offset_of;
	using ::std::meta::extent;

	using ::std::meta::remove_const;
	using ::std::meta::remove_volatile;
	using ::std::meta::remove_cv;
	using ::std::meta::remove_reference;
	using ::std::meta::remove_cvref;
	using ::std::meta::add_const;
	using ::std::meta::add_volatile;
	using ::std::meta::add_cv;
	using ::std::meta::add_lvalue_reference;
	using ::std::meta::add_rvalue_reference;
	using ::std::meta::add_pointer;
	using ::std::meta::remove_pointer;

	using ::std::meta::is_public;
	using ::std::meta::is_protected;
	using ::std::meta::is_private;
	using ::std::meta::is_virtual;
	using ::std::meta::is_pure_virtual;
	using ::std::meta::is_override;
	using ::std::meta::is_deleted;
	using ::std::meta::is_defaulted;
	using ::std::meta::is_explicit;
	using ::std::meta::is_noexcept;
	using ::std::meta::is_bit_field;
	using ::std::meta::is_enumerator;
	using ::std::meta::is_const;
	using ::std::meta::is_volatile;
	using ::std::meta::is_mutable_member;
	using ::std::meta::is_lvalue_reference_qualified;
	using ::std::meta::is_rvalue_reference_qualified;
	using ::std::meta::has_static_storage_duration;
	using ::std::meta::has_thread_storage_duration;
	using ::std::meta::has_automatic_storage_duration;
	using ::std::meta::has_internal_linkage;
	using ::std::meta::has_module_linkage;
	using ::std::meta::has_external_linkage;
	using ::std::meta::has_linkage;
	using ::std::meta::is_class_member;
	using ::std::meta::is_namespace_member;
	using ::std::meta::is_nonstatic_data_member;
	using ::std::meta::is_static_member;
	using ::std::meta::is_base;
	using ::std::meta::is_data_member_spec;
	using ::std::meta::is_namespace;
	using ::std::meta::is_function;
	using ::std::meta::is_variable;
	using ::std::meta::is_type;
	using ::std::meta::is_type_alias;
	using ::std::meta::is_namespace_alias;
	using ::std::meta::is_complete_type;
	using ::std::meta::is_enumerable_type;
	using ::std::meta::is_template;
	using ::std::meta::is_function_template;
	using ::std::meta::is_variable_template;
	using ::std::meta::is_class_template;
	using ::std::meta::is_alias_template;
	using ::std::meta::is_conversion_function_template;
	using ::std::meta::is_operator_function_template;
	using ::std::meta::is_literal_operator_template;
	using ::std::meta::is_constructor_template;
	using ::std::meta::is_concept;
	using ::std::meta::is_structured_binding;
	using ::std::meta::has_default_member_initializer;
	using ::std::meta::is_conversion_function;
	using ::std::meta::is_array_type;
	using ::std::meta::is_same_type;
}
