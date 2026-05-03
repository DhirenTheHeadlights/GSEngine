export module gse.save:save_system;

import std;

import gse.log;
import gse.math;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.toml;

export namespace gse::save {
    class property_base;
    class state;

    struct no_constraint {};

    template <typename T>
    struct range_constraint {
        T min;
        T max;
        T step = T{ 1 };
    };

    template <typename T>
    struct enum_constraint {
        std::vector<std::pair<std::string, T>> options;
    };

    struct update_request {
        std::string category;
        std::string name;
        std::function<void(property_base&)> apply;
    };

    struct save_request {};
    struct restart_request {};

    struct bind_int_map_request {
        std::string category;
        std::map<std::string, int> initial_data;
    };

    struct int_map_sync {
        std::string category;
        std::map<std::string, int> data;
    };

    struct int_map_loaded {
        std::string category;
        std::map<std::string, int> data;
    };

    struct register_property {
        std::string category;
        std::string name;
        std::string description;
        void* ref = nullptr;
        std::type_index type = typeid(void);

        float range_min = 0.f;
        float range_max = 1.f;
        float range_step = 0.1f;

        std::vector<std::pair<std::string, int>> enum_options;
        bool requires_restart = false;

        std::function<std::unique_ptr<property_base>(state*, const register_property&)> factory;
    };

    template <internal::is_quantity Q>
    [[nodiscard]] auto make_property_registration(
        std::string category,
        std::string name,
        std::string description,
        Q& ref,
        bool requires_restart = false
    ) -> register_property;

    enum class constraint_type {
        none,
        range,
        enumeration
    };

    class property_base {
    public:
        virtual ~property_base() = default;

        [[nodiscard]] virtual auto name(
        ) const -> std::string_view = 0;

        [[nodiscard]] virtual auto category(
        ) const -> std::string_view = 0;

        [[nodiscard]] virtual auto description(
        ) const -> std::string_view = 0;

        [[nodiscard]] virtual auto type(
        ) const -> std::type_index = 0;

        [[nodiscard]] virtual auto dirty(
        ) const -> bool = 0;

        virtual auto mark_clean(
        ) -> void = 0;

        virtual auto reset_to_default(
        ) -> void = 0;

        virtual auto write_toml(
            toml::table& table,
            std::string_view key
        ) const -> void = 0;

        virtual auto read_toml(
            const toml::value& node
        ) -> bool = 0;

        [[nodiscard]] virtual auto as_void_ptr(
        ) -> void* = 0;

        [[nodiscard]] virtual auto as_void_ptr(
        ) const -> const void* = 0;

        [[nodiscard]] virtual auto constraint_kind(
        ) const -> constraint_type = 0;

        [[nodiscard]] virtual auto as_bool(
        ) const -> bool;

        virtual auto set_bool(
            bool value
        ) -> void;

        [[nodiscard]] virtual auto as_float(
        ) const -> float;

        virtual auto set_float(
            float value
        ) -> void;

        [[nodiscard]] virtual auto range_min(
        ) const -> float;

        [[nodiscard]] virtual auto range_max(
        ) const -> float;

        [[nodiscard]] virtual auto range_step(
        ) const -> float;

        [[nodiscard]] virtual auto as_int(
        ) const -> int;

        virtual auto set_int(
            int value
        ) -> void;

        [[nodiscard]] virtual auto enum_count(
        ) const -> std::size_t;

        [[nodiscard]] virtual auto enum_label(
            std::size_t index
        ) const -> std::string_view;

        [[nodiscard]] virtual auto enum_index(
        ) const -> std::size_t;

        virtual auto set_enum_index(
            std::size_t index
        ) -> void;

        [[nodiscard]] virtual auto as_string(
        ) const -> std::string_view;

        virtual auto set_string(
            std::string_view value
        ) -> void;

        [[nodiscard]] virtual auto requires_restart(
        ) const -> bool = 0;
    };

    template <typename T, typename Constraint = no_constraint>
    class property final : public property_base {
    public:
        property(
            state* st,
            std::string_view category,
            std::string_view name,
            std::string_view description,
            T& ref,
            T default_value,
            Constraint constraint = {},
            bool requires_restart = false
        );

        [[nodiscard]] auto name(
        ) const -> std::string_view override;

        [[nodiscard]] auto category(
        ) const -> std::string_view override;

        [[nodiscard]] auto description(
        ) const -> std::string_view override;

        [[nodiscard]] auto type(
        ) const -> std::type_index override;

        [[nodiscard]] auto dirty(
        ) const -> bool override;

        auto mark_clean(
        ) -> void override;

        auto reset_to_default(
        ) -> void override;

        auto write_toml(
            toml::table& table,
            std::string_view key
        ) const -> void override;

        auto read_toml(
            const toml::value& node
        ) -> bool override;

        [[nodiscard]] auto as_void_ptr(
        ) -> void* override;

        [[nodiscard]] auto as_void_ptr(
        ) const -> const void* override;

        [[nodiscard]] auto constraint_kind(
        ) const -> constraint_type override;

        [[nodiscard]] auto as_bool(
        ) const -> bool override;

        auto set_bool(
            bool value
        ) -> void override;

        [[nodiscard]] auto as_float(
        ) const -> float override;

        auto set_float(
            float value
        ) -> void override;

        [[nodiscard]] auto range_min(
        ) const -> float override;

        [[nodiscard]] auto range_max(
        ) const -> float override;

        [[nodiscard]] auto range_step(
        ) const -> float override;

        [[nodiscard]] auto as_int(
        ) const -> int override;

        auto set_int(
            int value
        ) -> void override;

        [[nodiscard]] auto enum_count(
        ) const -> std::size_t override;

        [[nodiscard]] auto enum_label(
            std::size_t index
        ) const -> std::string_view override;

        [[nodiscard]] auto enum_index(
        ) const -> std::size_t override;

        auto set_enum_index(
            std::size_t index
        ) -> void override;

        [[nodiscard]] auto as_string(
        ) const -> std::string_view override;

        auto set_string(
            std::string_view value
        ) -> void override;

        [[nodiscard]] auto requires_restart(
        ) const -> bool override;

        [[nodiscard]] auto value(
        ) const -> const T&;

        [[nodiscard]] auto default_value(
        ) const -> const T&;

        [[nodiscard]] auto constraint(
        ) const -> const Constraint&;

        auto set(
            T value
        ) -> void;
    private:
        state* m_state;
        std::string m_category;
        std::string m_name;
        std::string m_description;
        T& m_ref;
        T m_default;
        Constraint m_constraint;
        bool m_dirty = false;
        bool m_requires_restart = false;
    };

    template <typename T>
    class property_builder {
    public:
        property_builder(
            state& st,
            std::string_view category,
            std::string_view name,
            T& ref
        );

        auto description(
            std::string_view desc
        ) -> property_builder&;

        auto default_value(
            T value
        ) -> property_builder&;

        template <typename C>
        auto with_constraint(
            C constraint
        ) -> property_builder&;

        auto range(
            T min,
            T max,
            T step = T{ 1 }
        ) -> property_builder& requires std::is_arithmetic_v<T>;

        auto options(
            std::initializer_list<std::pair<std::string, T>> opts
        ) -> property_builder&;

        auto options(
            std::vector<std::pair<std::string, T>> opts
        ) -> property_builder&;

        auto restart_required(
        ) -> property_builder&;

        auto commit(
        ) -> property_base*;
    private:
        state& m_state;
        std::string m_category;
        std::string m_name;
        std::string m_description;
        T& m_ref;
        T m_default;
        std::any m_constraint;
        bool m_requires_restart = false;
    };

    class category_view {
    public:
        explicit category_view(
            std::span<property_base* const> properties
        );

        [[nodiscard]] auto begin(
        ) const -> std::span<property_base* const>::iterator;

        [[nodiscard]] auto end(
        ) const -> std::span<property_base* const>::iterator;

        [[nodiscard]] auto size(
        ) const -> std::size_t;

        [[nodiscard]] auto empty(
        ) const -> bool;
    private:
        std::span<property_base* const> m_properties;
    };

    class state : non_copyable {
    public:
        state() = default;

        auto do_initialize(
            init_context& phase
        ) -> void;

        auto do_update(
            update_context& ctx
        ) -> void;

        auto do_shutdown(
            shutdown_context& phase
        ) -> void;

        template <typename T>
        auto bind(
            std::string_view category, 
            std::string_view name, 
            T& ref
        ) -> property_builder<T>;

        auto register_property(
            std::unique_ptr<property_base> prop
        ) -> property_base*;

        [[nodiscard]] auto categories(
        ) const -> std::vector<std::string_view>;

        [[nodiscard]] auto properties_in(
            std::string_view category
        ) const -> category_view;

        [[nodiscard]] auto all_properties(
        ) const -> std::span<const std::unique_ptr<property_base>>;

        template <typename T>
        [[nodiscard]] auto get(
            std::string_view category,
            std::string_view name
        ) const -> const property<T>*;

        [[nodiscard]] auto find(
            std::string_view category,
            std::string_view name
        ) const -> property_base*;

        auto save_to_file(
            const std::filesystem::path& path
        ) -> bool;

        auto load_from_file(
            const std::filesystem::path& path
        ) -> bool;

        auto set_auto_save(
            bool enabled,
            std::filesystem::path path = {}
        ) -> void;

        auto save(
        ) -> bool;

        [[nodiscard]] auto has_unsaved_changes(
        ) const -> bool;

        [[nodiscard]] auto has_pending_restart_changes(
        ) const -> bool;

        auto mark_all_clean(
        ) const -> void;

        auto clear_restart_pending(
        ) -> void;

        using change_callback = std::function<void(property_base&)>;
        auto on_change(
            change_callback cb
        ) -> void;

        auto on_restart(
            std::function<void()> fn
        ) -> void;

        auto notify_change(
            property_base& prop
        ) -> void;

        template <typename T>
        [[nodiscard]] auto read(
            std::string_view category,
            std::string_view name,
            T default_value = T{}
        ) const -> T;

        auto apply_int_map(
            std::string_view category,
            std::map<std::string, int> data
        ) -> void;

        [[nodiscard]] auto int_map(
            std::string_view category
        ) const -> const std::map<std::string, int>*;

    private:
        std::vector<std::unique_ptr<property_base>> m_properties;
        std::unordered_map<std::string, std::vector<property_base*>> m_by_category;
        std::vector<change_callback> m_callbacks;

        std::unordered_map<std::string, std::map<std::string, int>> m_int_maps;

        std::filesystem::path m_auto_save_path;
        toml::table m_loaded_toml;
        bool m_auto_save = false;
        bool m_restart_pending = false;
        std::function<void()> m_restart_fn;

        auto process_registration(
            const save::register_property& reg
        ) -> void;
    };

    struct system {
        static auto initialize(
            init_context& phase,
            state& s
        ) -> void {
            s.do_initialize(phase);
        }

        static auto update(
            update_context& ctx,
            state& s
        ) -> async::task<> {
            s.do_update(ctx);
            co_return;
        }

        static auto shutdown(
            shutdown_context& phase,
            state& s
        ) -> void {
            s.do_shutdown(phase);
        }
    };

    [[nodiscard]] auto read_setting_early(
        const std::filesystem::path& path,
        std::string_view category,
        std::string_view name
    ) -> std::optional<std::string>;

    [[nodiscard]] auto read_bool_setting_early(
        const std::filesystem::path& path,
        std::string_view category,
        std::string_view name,
        bool default_value = false
    ) -> bool;

    struct property_changed {
        std::string_view category;
        std::string_view name;
        std::type_index type;
    };

}

namespace gse::save {
    [[nodiscard]] auto read_file_to_string(
        const std::filesystem::path& path
    ) -> std::optional<std::string>;
}

auto gse::save::read_file_to_string(const std::filesystem::path& path) -> std::optional<std::string> {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        file.read(content.data(), static_cast<std::streamsize>(size));
        content.resize(static_cast<std::size_t>(file.gcount()));
    }
    return content;
}

auto gse::save::property_base::as_bool() const -> bool {
	return false;
}

auto gse::save::property_base::set_bool(bool) -> void {
	
}

auto gse::save::property_base::as_float() const -> float {
	return 0.f;
}

auto gse::save::property_base::set_float(float) -> void {
	
}

auto gse::save::property_base::range_min() const -> float {
	return 0.f;
}

auto gse::save::property_base::range_max() const -> float {
	return 1.f;
}

auto gse::save::property_base::range_step() const -> float {
	return 0.1f;
}
auto gse::save::property_base::as_int() const -> int {
	return 0;
}

auto gse::save::property_base::set_int(int) -> void {
	
}

auto gse::save::property_base::enum_count() const -> std::size_t {
	return 0;
}

auto gse::save::property_base::enum_label(std::size_t) const -> std::string_view {
    return "";
}

auto gse::save::property_base::enum_index() const -> std::size_t {
	return 0;
}

auto gse::save::property_base::set_enum_index(std::size_t) -> void {
	
}

auto gse::save::property_base::as_string() const -> std::string_view {
	return "";
}

auto gse::save::property_base::set_string(std::string_view) -> void {
	
}

template <typename T, typename Constraint>
gse::save::property<T, Constraint>::property(state* st, const std::string_view category, const std::string_view name, const std::string_view description, T& ref, T default_value, Constraint constraint, const bool requires_restart) 
	: m_state(st)
	, m_category(category)
	, m_name(name)
	, m_description(description)
	, m_ref(ref)
	, m_default(std::move(default_value))
	, m_constraint(std::move(constraint))
	, m_requires_restart(requires_restart) {}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::name() const -> std::string_view {
	return m_name;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::category() const -> std::string_view {
	return m_category;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::description() const -> std::string_view {
	return m_description;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::type() const -> std::type_index {
	return typeid(T);
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::dirty() const -> bool {
	return m_dirty;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::mark_clean() -> void {
	m_dirty = false;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::reset_to_default() -> void {
    if (m_ref != m_default) {
        m_ref = m_default;
        m_dirty = true;
        if (m_state) {
            m_state->notify_change(*this);
        }
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::write_toml(toml::table& table, const std::string_view key) const -> void {
    const std::string k(key);
    if constexpr (internal::is_quantity<T>) {
        using underlying = typename T::value_type;
        const underlying v = m_ref.template as<typename T::default_unit>();
        if constexpr (std::is_integral_v<underlying>) {
            table.emplace(k, toml::value{ .data = static_cast<std::int64_t>(v) });
        }
        else {
            table.emplace(k, toml::value{ .data = static_cast<double>(v) });
        }
    }
    else if constexpr (std::is_same_v<T, bool>) {
        table.emplace(k, toml::value{ .data = m_ref });
    }
    else if constexpr (std::is_same_v<T, float>) {
        table.emplace(k, toml::value{ .data = static_cast<double>(m_ref) });
    }
    else if constexpr (std::is_same_v<T, int>) {
        table.emplace(k, toml::value{ .data = static_cast<std::int64_t>(m_ref) });
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        table.emplace(k, toml::value{ .data = m_ref });
    }
    else if constexpr (std::is_enum_v<T>) {
        table.emplace(k, toml::value{ .data = static_cast<std::int64_t>(m_ref) });
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::read_toml(const toml::value& node) -> bool {
    if constexpr (internal::is_quantity<T>) {
        using underlying = typename T::value_type;
        using default_unit = typename T::default_unit;
        if constexpr (std::is_integral_v<underlying>) {
            if (!node.is_int()) {
                return false;
            }
            set(T::template from<default_unit>(static_cast<underlying>(node.as_int())));
        }
        else {
            if (node.is_double()) {
                set(T::template from<default_unit>(static_cast<underlying>(node.as_double())));
            }
            else if (node.is_int()) {
                set(T::template from<default_unit>(static_cast<underlying>(node.as_int())));
            }
            else {
                return false;
            }
        }
    }
    else if constexpr (std::is_same_v<T, bool>) {
        if (!node.is_bool()) return false;
        set(node.as_bool());
    }
    else if constexpr (std::is_same_v<T, float>) {
        if (node.is_double()) {
            set(static_cast<float>(node.as_double()));
        }
        else if (node.is_int()) {
            set(static_cast<float>(node.as_int()));
        }
        else {
            return false;
        }
    }
    else if constexpr (std::is_same_v<T, int>) {
        if (!node.is_int()) return false;
        set(static_cast<int>(node.as_int()));
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        if (!node.is_string()) return false;
        set(node.as_string());
    }
    else if constexpr (std::is_enum_v<T>) {
        if (!node.is_int()) return false;
        set(static_cast<T>(node.as_int()));
    }
    else {
        return false;
    }
    return true;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::as_void_ptr() -> void* {
	return &m_ref;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::as_void_ptr() const -> const void* {
	return &m_ref;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::constraint_kind() const -> constraint_type {
    if constexpr (std::is_same_v<Constraint, no_constraint>) {
        return constraint_type::none;
    }
    else if constexpr (requires { Constraint::min; Constraint::max; }) {
        return constraint_type::range;
    }
    else if constexpr (requires { std::declval<Constraint>().options; }) {
        return constraint_type::enumeration;
    }
    else {
        return constraint_type::none;
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::as_bool() const -> bool {
    if constexpr (std::is_same_v<T, bool>) return m_ref;
    return false;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set_bool(const bool value) -> void {
    if constexpr (std::is_same_v<T, bool>) set(value);
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::as_float() const -> float {
    if constexpr (internal::is_quantity<T>) {
        return static_cast<float>(m_ref.template as<typename T::default_unit>());
    }
    else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
        return static_cast<float>(m_ref);
    }
    return 0.f;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set_float(const float value) -> void {
    if constexpr (internal::is_quantity<T>) {
        using underlying = typename T::value_type;
        set(T::template from<typename T::default_unit>(static_cast<underlying>(value)));
    }
    else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
        set(static_cast<T>(value));
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::range_min() const -> float {
    if constexpr (requires { m_constraint.min; }) {
        return static_cast<float>(m_constraint.min);
    }
    return 0.f;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::range_max() const -> float {
    if constexpr (requires { m_constraint.max; }) {
        return static_cast<float>(m_constraint.max);
    }
    return 1.f;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::range_step() const -> float {
    if constexpr (requires { m_constraint.step; }) {
        return static_cast<float>(m_constraint.step);
    }
    return 0.1f;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::as_int() const -> int {
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        return static_cast<int>(m_ref);
    }
    else if constexpr (std::is_enum_v<T>) {
        return static_cast<int>(m_ref);
    }
    return 0;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set_int(const int value) -> void {
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        set(static_cast<T>(value));
    }
    else if constexpr (std::is_enum_v<T>) {
        set(static_cast<T>(value));
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::enum_count() const -> std::size_t {
    if constexpr (requires { m_constraint.options; }) {
        return m_constraint.options.size();
    }
    return 0;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::enum_label(const std::size_t index) const -> std::string_view {
    if constexpr (requires { m_constraint.options; }) {
        if (index < m_constraint.options.size()) {
            return m_constraint.options[index].first;
        }
    }
    return "";
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::enum_index() const -> std::size_t {
    if constexpr (requires { m_constraint.options; }) {
        const auto it = std::ranges::find_if(m_constraint.options, [this](const auto& opt) { return opt.second == m_ref; });
        if (it != m_constraint.options.end()) {
            return static_cast<std::size_t>(std::ranges::distance(m_constraint.options.begin(), it));
        }
    }
    return 0;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set_enum_index(const std::size_t index) -> void {
    if constexpr (requires { m_constraint.options; }) {
        if (index < m_constraint.options.size()) {
            set(m_constraint.options[index].second);
        }
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::as_string() const -> std::string_view {
    if constexpr (std::is_same_v<T, std::string>) return m_ref;
    return "";
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set_string(const std::string_view value) -> void {
    if constexpr (std::is_same_v<T, std::string>) set(std::string(value));
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::requires_restart() const -> bool {
    return m_requires_restart;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::value() const -> const T& { return m_ref; }

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::default_value() const -> const T& { return m_default; }

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::constraint() const -> const Constraint& { return m_constraint; }

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set(T value) -> void {
    if constexpr (std::is_same_v<Constraint, range_constraint<T>>) {
        value = std::clamp(value, m_constraint.min, m_constraint.max);
    }

    if (m_ref != value) {
        m_ref = std::move(value);
        m_dirty = true;
        if (m_state) {
            m_state->notify_change(*this);
        }
    }
}

template <typename T>
gse::save::property_builder<T>::property_builder(state& st, const std::string_view category, const std::string_view name, T& ref) : m_state(st), m_category(category), m_name(name), m_ref(ref), m_default(ref) {}

template <typename T>
auto gse::save::property_builder<T>::description(const std::string_view desc) -> property_builder& {
    m_description = desc;
    return *this;
}

template <typename T>
auto gse::save::property_builder<T>::default_value(T value) -> property_builder& {
    m_default = std::move(value);
    return *this;
}

template <typename T>
template <typename C>
auto gse::save::property_builder<T>::with_constraint(C constraint) -> property_builder& {
    m_constraint = std::move(constraint);
    return *this;
}

template <typename T>
auto gse::save::property_builder<T>::range(T min, T max, T step) -> property_builder& requires std::is_arithmetic_v<T> {
    m_constraint = range_constraint<T>{ min, max, step };
    return *this;
}

template <typename T>
auto gse::save::property_builder<T>::options(std::initializer_list<std::pair<std::string, T>> opts) -> property_builder& {
    enum_constraint<T> c;
    c.options = opts;
    m_constraint = std::move(c);
    return *this;
}

template <typename T>
auto gse::save::property_builder<T>::options(std::vector<std::pair<std::string, T>> opts) -> property_builder& {
    enum_constraint<T> c;
    c.options = std::move(opts);
    m_constraint = std::move(c);
    return *this;
}

template <typename T>
auto gse::save::property_builder<T>::restart_required() -> property_builder& {
    m_requires_restart = true;
    return *this;
}

template <typename T>
auto gse::save::property_builder<T>::commit() -> property_base* {
    m_ref = m_state.read<T>(m_category, m_name, m_default);

    std::unique_ptr<property_base> prop;

    if (m_constraint.has_value()) {
        if (auto* range_c = std::any_cast<range_constraint<T>>(&m_constraint)) {
            prop = std::make_unique<property<T, range_constraint<T>>>(
                &m_state, m_category, m_name, m_description, m_ref, m_default, *range_c, m_requires_restart
            );
        }
        else if (auto* enum_c = std::any_cast<enum_constraint<T>>(&m_constraint)) {
            prop = std::make_unique<property<T, enum_constraint<T>>>(
                &m_state, m_category, m_name, m_description, m_ref, m_default, *enum_c, m_requires_restart
            );
        }
    }

    if (!prop) {
        prop = std::make_unique<property<T>>(
            &m_state, m_category, m_name, m_description, m_ref, m_default, no_constraint{}, m_requires_restart
        );
    }

    return m_state.register_property(std::move(prop));
}

gse::save::category_view::category_view(const std::span<property_base* const> properties)
    : m_properties(properties) {}

auto gse::save::category_view::begin() const -> std::span<property_base* const>::iterator {
    return m_properties.begin();
}

auto gse::save::category_view::end() const -> std::span<property_base* const>::iterator {
    return m_properties.end();
}

auto gse::save::category_view::size() const -> std::size_t {
    return m_properties.size();
}

auto gse::save::category_view::empty() const -> bool {
    return m_properties.empty();
}

auto gse::save::state::do_initialize(init_context&) -> void {
    if (!m_auto_save_path.empty() && std::filesystem::exists(m_auto_save_path)) {
        if (!load_from_file(m_auto_save_path)) {
            log::println(log::level::warning, log::category::save_system, "Failed to load settings from {}", m_auto_save_path.string());
        }
    }
}

auto gse::save::state::do_update(update_context& ctx) -> void {
    for (const auto& reg : ctx.read_channel<save::register_property>()) {
        process_registration(reg);
    }

    for (const auto& [category, initial_data] : ctx.read_channel<bind_int_map_request>()) {
        apply_int_map(category, initial_data);
        ctx.channels.push<int_map_loaded>({ category, m_int_maps[category] });
    }

    for (const auto& [category, data] : ctx.read_channel<int_map_sync>()) {
        m_int_maps[category] = data;
    }

    if (!ctx.read_channel<save_request>().empty()) {
        save();
    }

    if (!ctx.read_channel<restart_request>().empty()) {
        save();
        if (m_restart_fn) {
            m_restart_fn();
        }
    }

    for (const auto& [category, name, apply] : ctx.read_channel<update_request>()) {
        if (auto* prop = find(category, name)) {
            apply(*prop);
        }
    }
}

auto gse::save::state::do_shutdown(shutdown_context&) -> void {
    if (m_auto_save && !m_auto_save_path.empty()) {
        if (!save_to_file(m_auto_save_path)) {
            log::println(log::level::warning, log::category::save_system, "Failed to save settings to {}", m_auto_save_path.string());
        }
    }
}

auto gse::save::state::process_registration(const save::register_property& reg) -> void {
    if (find(reg.category, reg.name)) {
        return;
    }

    std::unique_ptr<property_base> prop;

    if (reg.factory) {
        prop = reg.factory(this, reg);
    }
    else if (reg.type == typeid(bool)) {
        auto* ref = static_cast<bool*>(reg.ref);
        prop = std::make_unique<property<bool>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, no_constraint{}, reg.requires_restart
        );
    }
    else if (!reg.enum_options.empty()) {
        auto* ref = static_cast<int*>(reg.ref);
        enum_constraint<int> constraint;
        constraint.options = reg.enum_options;
        prop = std::make_unique<property<int, enum_constraint<int>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint, reg.requires_restart
        );
    }
    else if (reg.type == typeid(float)) {
        auto* ref = static_cast<float*>(reg.ref);
        range_constraint constraint{ 
        	.min = reg.range_min, 
        	.max = reg.range_max, 
        	.step = reg.range_step
        };
        prop = std::make_unique<property<float, range_constraint<float>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint, reg.requires_restart
        );
    }
    else if (reg.type == typeid(int)) {
        auto* ref = static_cast<int*>(reg.ref);
        range_constraint constraint{
            .min = static_cast<int>(reg.range_min),
            .max = static_cast<int>(reg.range_max),
            .step = static_cast<int>(reg.range_step)
        };
        prop = std::make_unique<property<int, range_constraint<int>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint, reg.requires_restart
        );
    }

    if (prop) {
        auto* registered = register_property(std::move(prop));

        if (const auto cat_it = m_loaded_toml.find(reg.category); cat_it != m_loaded_toml.end() && cat_it->second.is_table()) {
            const auto& cat_table = cat_it->second.as_table();
            if (const auto val_it = cat_table.find(reg.name); val_it != cat_table.end()) {
                registered->read_toml(val_it->second);
                registered->mark_clean();
            }
        }
    }
}

template <typename T>
auto gse::save::state::bind(const std::string_view category, const std::string_view name, T& ref) -> property_builder<T> {
    return property_builder<T>(*this, category, name, ref);
}

template <gse::internal::is_quantity Q>
auto gse::save::make_property_registration(std::string category, std::string name, std::string description, Q& ref, const bool requires_restart) -> register_property {
    register_property reg;
    reg.category = std::move(category);
    reg.name = std::move(name);
    reg.description = std::move(description);
    reg.ref = static_cast<void*>(&ref);
    reg.type = typeid(Q);
    reg.requires_restart = requires_restart;
    reg.factory = [](state* s, const register_property& r) -> std::unique_ptr<property_base> {
        Q& typed_ref = *static_cast<Q*>(r.ref);
        return std::make_unique<property<Q>>(
            s, r.category, r.name, r.description, typed_ref, typed_ref, no_constraint{}, r.requires_restart
        );
    };
    return reg;
}

auto gse::save::state::register_property(std::unique_ptr<property_base> prop) -> property_base* {
    const std::string cat(prop->category());
    const std::string name(prop->name());

    const auto it = std::ranges::find_if(m_properties, [&](const auto& p) {
        return p->category() == cat && p->name() == name;
    });
    if (it != m_properties.end()) {
        auto& cat_vec = m_by_category[cat];
        std::erase(cat_vec, it->get());

        auto* ptr = prop.get();
        *it = std::move(prop);
        cat_vec.push_back(ptr);
        return ptr;
    }

    auto* ptr = prop.get();
    m_by_category[cat].push_back(ptr);
    m_properties.push_back(std::move(prop));
    return ptr;
}

auto gse::save::state::categories() const -> std::vector<std::string_view> {
    std::vector<std::string_view> result;
    result.reserve(m_by_category.size());
    std::ranges::copy(m_by_category | std::views::keys, std::back_inserter(result));
    std::ranges::sort(result);
    return result;
}

auto gse::save::state::properties_in(const std::string_view category) const -> category_view {
    if (const auto it = m_by_category.find(std::string(category)); it != m_by_category.end()) {
        return category_view(it->second);
    }
    return category_view({});
}

auto gse::save::state::all_properties() const -> std::span<const std::unique_ptr<property_base>> {
    return m_properties;
}

template <typename T>
auto gse::save::state::get(const std::string_view category, const std::string_view name) const -> const property<T>* {
    for (const auto& prop : m_properties) {
        if (prop->category() == category && prop->name() == name) {
            if (prop->type() == typeid(T)) {
                return static_cast<const property<T>*>(prop.get());
            }
            return nullptr;
        }
    }
    return nullptr;
}

auto gse::save::state::find(const std::string_view category, const std::string_view name) const -> property_base* {
    for (const auto& prop : m_properties) {
        if (prop->category() == category && prop->name() == name) {
            return prop.get();
        }
    }
    return nullptr;
}

auto gse::save::state::save_to_file(const std::filesystem::path& path) -> bool {
    toml::table root;

    for (const auto& prop : m_properties) {
        std::string category(prop->category());

        auto [it, _] = root.try_emplace(category, toml::value{ .data = toml::table{} });
        prop->write_toml(it->second.as_table(), prop->name());
    }

    for (const auto& [category, map_data] : m_int_maps) {
        if (map_data.empty()) continue;

        toml::table map_table;
        for (const auto& [key, value] : map_data) {
            map_table.emplace(key, toml::value{ .data = static_cast<std::int64_t>(value) });
        }
        root.insert_or_assign(category, toml::value{ .data = std::move(map_table) });
    }

    const std::string content = toml::emit(toml::value{ .data = std::move(root) });

    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << content;
    mark_all_clean();
    return true;
}

auto gse::save::state::load_from_file(const std::filesystem::path& path) -> bool {
    if (!std::filesystem::exists(path)) {
        log::println(log::level::warning, log::category::save_system, "Settings file does not exist: {}", path.string());
        return false;
    }

    const auto content = read_file_to_string(path);
    if (!content) {
        return false;
    }

    auto parsed = toml::parse(*content, path.string());
    if (!parsed) {
        log::println(log::level::warning, log::category::save_system, "TOML parse error in {} at {}:{}: {}", path.string(), parsed.error().line, parsed.error().column, parsed.error().message);
        return false;
    }
    if (!parsed->is_table()) {
        return false;
    }
    m_loaded_toml = std::move(parsed->as_table());

    for (const auto& [category, cat_node] : m_loaded_toml) {
        if (!cat_node.is_table()) continue;
        const auto& cat_table = cat_node.as_table();

        std::map<std::string, int> int_map_entries;

        for (const auto& [name, value_node] : cat_table) {
            if (auto* prop = find(category, name)) {
                prop->read_toml(value_node);
            }
            else if (value_node.is_int()) {
                int_map_entries[name] = static_cast<int>(value_node.as_int());
            }
        }

        if (!int_map_entries.empty()) {
            m_int_maps[category] = std::move(int_map_entries);
        }
    }

    mark_all_clean();
    return true;
}

auto gse::save::state::set_auto_save(const bool enabled, std::filesystem::path path) -> void {
    m_auto_save = enabled;
    if (!path.empty()) {
        m_auto_save_path = std::move(path);
    }
}

auto gse::save::state::save() -> bool {
    if (m_auto_save_path.empty()) {
        return false;
    }
    return save_to_file(m_auto_save_path);
}

auto gse::save::state::apply_int_map(const std::string_view category, std::map<std::string, int> data) -> void {
    const std::string cat_str(category);
    if (const auto it = m_int_maps.find(cat_str); it != m_int_maps.end()) {
        for (const auto& [k, v] : data) {
            it->second.try_emplace(k, v);
        }
    }
    else {
        m_int_maps[cat_str] = std::move(data);
    }
}

auto gse::save::state::int_map(const std::string_view category) const -> const std::map<std::string, int>* {
    if (const auto it = m_int_maps.find(std::string(category)); it != m_int_maps.end()) {
        return &it->second;
    }
    return nullptr;
}

auto gse::save::state::has_unsaved_changes() const -> bool {
    return std::ranges::any_of(m_properties, [](const std::unique_ptr<property_base>& p) {
        return p->dirty();
    });
}

auto gse::save::state::has_pending_restart_changes() const -> bool {
    return m_restart_pending;
}

auto gse::save::state::clear_restart_pending() -> void {
    m_restart_pending = false;
}

auto gse::save::state::mark_all_clean() const -> void {
    for (const auto& prop : m_properties) {
        prop->mark_clean();
    }
}

auto gse::save::state::on_change(change_callback cb) -> void {
    m_callbacks.push_back(std::move(cb));
}

auto gse::save::state::on_restart(std::function<void()> fn) -> void {
    m_restart_fn = std::move(fn);
}

auto gse::save::state::notify_change(property_base& prop) -> void {
    if (prop.requires_restart()) {
        m_restart_pending = true;
    }

    for (const auto& cb : m_callbacks) {
        cb(prop);
    }
}

auto gse::save::read_setting_early(const std::filesystem::path& path, const std::string_view category, const std::string_view name) -> std::optional<std::string> {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    const auto content = read_file_to_string(path);
    if (!content) {
        return std::nullopt;
    }

    auto parsed = toml::parse(*content, path.string());
    if (!parsed) {
        log::println(log::level::warning, log::category::save_system, "TOML parse error (early read) in {} at {}:{}: {}", path.string(), parsed.error().line, parsed.error().column, parsed.error().message);
        return std::nullopt;
    }
    if (!parsed->is_table()) {
        return std::nullopt;
    }

    const auto& root = parsed->as_table();
    const auto cat_it = root.find(std::string(category));
    if (cat_it == root.end() || !cat_it->second.is_table()) {
        return std::nullopt;
    }
    const auto& cat_table = cat_it->second.as_table();
    const auto val_it = cat_table.find(std::string(name));
    if (val_it == cat_table.end()) {
        return std::nullopt;
    }
    const auto& v = val_it->second;

    if (v.is_bool()) {
        return v.as_bool() ? "true" : "false";
    }
    if (v.is_int()) {
        return std::to_string(v.as_int());
    }
    if (v.is_double()) {
        return std::to_string(v.as_double());
    }
    if (v.is_string()) {
        return v.as_string();
    }

    return std::nullopt;
}

auto gse::save::read_bool_setting_early(const std::filesystem::path& path, const std::string_view category, const std::string_view name, const bool default_value) -> bool {
    if (!std::filesystem::exists(path)) {
        return default_value;
    }

    const auto content = read_file_to_string(path);
    if (!content) {
        return default_value;
    }

    auto parsed = toml::parse(*content, path.string());
    if (!parsed || !parsed->is_table()) {
        return default_value;
    }

    const auto& root = parsed->as_table();
    const auto cat_it = root.find(std::string(category));
    if (cat_it == root.end() || !cat_it->second.is_table()) {
        return default_value;
    }
    const auto& cat_table = cat_it->second.as_table();
    const auto val_it = cat_table.find(std::string(name));
    if (val_it == cat_table.end()) {
        return default_value;
    }

    return val_it->second.value_or(default_value);
}

template <typename T>
auto gse::save::state::read(const std::string_view category, const std::string_view name, T default_value) const -> T {
    if (m_auto_save_path.empty()) {
        return default_value;
    }

    const auto value_str = read_setting_early(m_auto_save_path, category, name);
    if (!value_str) {
        return default_value;
    }

    if constexpr (std::is_same_v<T, bool>) {
        return *value_str == "true" || *value_str == "1";
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        if (value_str->size() >= 2 && value_str->front() == '"' && value_str->back() == '"') {
            return value_str->substr(1, value_str->size() - 2);
        }
        return *value_str;
    }
    else if constexpr (std::is_enum_v<T>) {
        std::underlying_type_t<T> int_val{};
        const auto* first = value_str->data();
        const auto* last = first + value_str->size();
        if (std::from_chars(first, last, int_val).ec == std::errc{}) {
            return static_cast<T>(int_val);
        }
        return default_value;
    }
    else {
        T result{};
        const auto* first = value_str->data();
        const auto* last = first + value_str->size();
        if (std::from_chars(first, last, result).ec == std::errc{}) {
            return result;
        }
        return default_value;
    }
}

