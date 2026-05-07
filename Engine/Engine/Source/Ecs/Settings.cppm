export module gse.ecs:settings;

import std;
import gse.std_meta;

import gse.core;
import gse.meta;

export namespace gse::settings {
    template <typename T>
    struct change_request {
        std::function<void(T&)> apply;
    };

    template <typename T>
    struct changed {
        T old_value;
        T new_value;
    };

    using write_settings_thunk = void(*)(
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        const void* settings_ptr
    );

    using read_settings_thunk = void(*)(
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        void* settings_ptr
    );

    struct register_settings_type {
        std::string category;
        id type_id;
        void* settings_ptr = nullptr;
        write_settings_thunk write = nullptr;
        read_settings_thunk read = nullptr;
    };

    template <typename T>
    concept has_parser_specialization = requires(std::string_view raw, T v) {
        { parser<T>::parse(raw, v) } -> std::same_as<bool>;
    };

    template <typename T>
    concept is_scalar_settings_field = std::is_arithmetic_v<T> || std::is_enum_v<T>
        || std::same_as<T, bool> || std::same_as<T, std::string>
        || has_parser_specialization<T>;

    template <typename T>
    auto write_settings_with_prefix(
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        std::string_view prefix,
        const T& value
    ) -> void;

    template <typename T>
    auto read_settings_with_prefix(
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        std::string_view prefix,
        T& value
    ) -> void;

    template <typename T>
    auto write_settings_for(
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        const void* settings_ptr
    ) -> void;

    template <typename T>
    auto read_settings_for(
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        void* settings_ptr
    ) -> void;

    template <typename T>
    consteval auto category_of(
    ) -> std::string_view;
}

template <typename T>
auto gse::settings::write_settings_with_prefix(std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const std::string_view prefix, const T& value) -> void {
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (meta::find_describe(m) != std::meta::info{}) {
            using F = [:std::meta::type_of(m):];
            constexpr std::string_view name = meta::member_name(m);
            const std::string key = prefix.empty()
                ? std::string(name)
                : std::format("{}.{}", prefix, name);

            if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
                write_settings_with_prefix<F>(doc, category, key, value.[:m:]);
            }
            else {
                std::string formatted;
                std::format_to(std::back_inserter(formatted), "{}", value.[:m:]);
                doc[std::string(category)][key] = std::move(formatted);
            }
        }
    }
}

template <typename T>
auto gse::settings::read_settings_with_prefix(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const std::string_view prefix, T& value) -> void {
    const auto cat_it = doc.find(std::string(category));
    if (cat_it == doc.end()) {
        return;
    }
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (meta::find_describe(m) != std::meta::info{}) {
            using F = [:std::meta::type_of(m):];
            constexpr std::string_view name = meta::member_name(m);
            const std::string key = prefix.empty()
                ? std::string(name)
                : std::format("{}.{}", prefix, name);

            if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
                read_settings_with_prefix<F>(doc, category, key, value.[:m:]);
            }
            else if (const auto it = cat_it->second.find(key); it != cat_it->second.end()) {
                gse::parse(it->second, value.[:m:]);
            }
        }
    }
}

template <typename T>
auto gse::settings::write_settings_for(std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const void* settings_ptr) -> void {
    write_settings_with_prefix<T>(doc, category, {}, *static_cast<const T*>(settings_ptr));
}

template <typename T>
auto gse::settings::read_settings_for(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, void* settings_ptr) -> void {
    read_settings_with_prefix<T>(doc, category, {}, *static_cast<T*>(settings_ptr));
}

template <typename T>
consteval auto gse::settings::category_of() -> std::string_view {
    if constexpr (requires { T::category; }) {
        return T::category;
    }
    else {
        return std::string_view{};
    }
}
