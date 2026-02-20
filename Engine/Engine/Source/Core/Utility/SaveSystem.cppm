export module gse.utility:save_system;

import std;
import tomlplusplus;

import :id;
import :concepts;
import :phase_context;

export namespace gse::save {
    class property_base;
    class state;
    template <typename T> class registration_builder;

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

    struct bind_int_map_request {
        std::string category;
        std::map<std::string, int>* map_ptr = nullptr;
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
    };

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

        virtual auto serialize(
            std::ostream& os
        ) const -> void = 0;

        virtual auto deserialize(
            std::istream& is
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

        [[nodiscard]] auto name() const -> std::string_view override;
        [[nodiscard]] auto category() const -> std::string_view override;
        [[nodiscard]] auto description() const -> std::string_view override;
        [[nodiscard]] auto type() const -> std::type_index override;
        [[nodiscard]] auto dirty() const -> bool override;

        auto mark_clean(
        ) -> void override;

        auto reset_to_default(
        ) -> void override;

        auto serialize(
            std::ostream& os
        ) const -> void override;

        auto deserialize(
            std::istream& is
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

        [[nodiscard]] auto begin() const -> std::span<property_base* const>::iterator;
        [[nodiscard]] auto end() const -> std::span<property_base* const>::iterator;
        [[nodiscard]] auto size() const -> std::size_t;
        [[nodiscard]] auto empty() const -> bool;
    private:
        std::span<property_base* const> m_properties;
    };

    class state {
    public:
        state() = default;

        auto do_initialize(initialize_phase& phase) -> void;
        auto do_update(update_phase& phase) -> void;
        auto do_shutdown(shutdown_phase& phase) -> void;

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
        ) const -> bool;

        auto load_from_file(
            const std::filesystem::path& path
        ) const -> bool;

        auto set_auto_save(
            bool enabled,
            std::filesystem::path path = {}
        ) -> void;

        auto save(
        ) const -> bool;

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

        auto notify_change(
            property_base& prop
        ) -> void;

        template <typename T>
        [[nodiscard]] auto read(
            std::string_view category,
            std::string_view name,
            T default_value = T{}
        ) const -> T;

        auto bind_int_map(
            std::string_view category,
            std::map<std::string, int>& map
        ) -> void;

        [[nodiscard]] auto int_map(
            std::string_view category
        ) const -> const std::map<std::string, int>*;

    private:
        std::vector<std::unique_ptr<property_base>> m_properties;
        mutable std::unordered_map<std::string, std::vector<property_base*>> m_by_category;
        std::vector<change_callback> m_callbacks;

        std::unordered_map<std::string, std::map<std::string, int>*> m_int_maps;
        mutable std::unordered_map<std::string, std::map<std::string, int>> m_int_map_cache;

        std::filesystem::path m_auto_save_path;
        bool m_auto_save = false;
        bool m_restart_pending = false;

        auto process_registration(
            const save::register_property& reg
        ) -> void;
    };

    struct system {
        static auto initialize(initialize_phase& phase, state& s) -> void {
            s.do_initialize(phase);
        }

        static auto update(update_phase& phase, state& s) -> void {
            s.do_update(phase);
        }

        static auto shutdown(shutdown_phase& phase, state& s) -> void {
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

    template <typename T>
    class registration_builder {
    public:
        registration_builder(
            channel<register_property>& ch,
            std::string_view category,
            std::string_view name,
            T& ref
        );

        auto description(
            std::string_view desc
        ) -> registration_builder&;

        auto default_value(
            T value
        ) -> registration_builder&;

        auto range(
            T min,
            T max,
            T step = T{ 1 }
        ) -> registration_builder& requires std::is_arithmetic_v<T>;

        auto options(
            std::initializer_list<std::pair<std::string, T>> opts
        ) -> registration_builder&;

        auto options(
            std::vector<std::pair<std::string, T>> opts
        ) -> registration_builder&;

        auto restart_required(
        ) -> registration_builder&;

        auto commit(
        ) const -> void;

    private:
        channel<register_property>& m_channel;
        std::string m_category;
        std::string m_name;
        std::string m_description;
        T& m_ref;
        float m_range_min = 0.f;
        float m_range_max = 1.f;
        float m_range_step = 0.1f;
        std::vector<std::pair<std::string, int>> m_enum_options;
        bool m_requires_restart = false;
    };

    template <typename T>
    auto bind(
        channel<register_property>& ch,
        std::string_view category,
        std::string_view name,
        T& ref
    ) -> registration_builder<T>;
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
gse::save::property<T, Constraint>::property(
    state* st,
    const std::string_view category,
    const std::string_view name,
    const std::string_view description,
    T& ref,
    T default_value,
    Constraint constraint,
    const bool requires_restart
) : m_state(st)
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
auto gse::save::property<T, Constraint>::serialize(std::ostream& os) const -> void {
    if constexpr (std::is_same_v<T, std::string>) {
        os << std::quoted(m_ref);
    }
    else if constexpr (std::is_same_v<T, bool>) {
        os << (m_ref ? "true" : "false");
    }
    else if constexpr (std::is_enum_v<T>) {
        os << static_cast<std::underlying_type_t<T>>(m_ref);
    }
    else {
        os << m_ref;
    }
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::deserialize(std::istream& is) -> bool {
    if constexpr (std::is_same_v<T, std::string>) {
        T value{};
        if (!(is >> std::quoted(value))) return false;
        set(std::move(value));
    }
    else if constexpr (std::is_same_v<T, bool>) {
        std::string s;
        if (!(is >> s)) return false;
        set(s == "true" || s == "1");
    }
    else if constexpr (std::is_enum_v<T>) {
        std::underlying_type_t<T> value{};
        if (!(is >> value)) return false;
        set(static_cast<T>(value));
    }
    else {
        T value{};
        if (!(is >> value)) return false;
        set(std::move(value));
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
    if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
        return static_cast<float>(m_ref);
    }
    return 0.f;
}

template <typename T, typename Constraint>
auto gse::save::property<T, Constraint>::set_float(const float value) -> void {
    if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
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
gse::save::property_builder<T>::property_builder(
    state& st,
    const std::string_view category,
    const std::string_view name,
    T& ref
) : m_state(st)
  , m_category(category)
  , m_name(name)
  , m_ref(ref)
  , m_default(ref) {}

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

auto gse::save::state::do_initialize(initialize_phase&) -> void {
    if (!m_auto_save_path.empty() && std::filesystem::exists(m_auto_save_path)) {
        if (!load_from_file(m_auto_save_path)) {
            std::println("Failed to load settings from {}", m_auto_save_path.string());
        }
    }
}

auto gse::save::state::do_update(update_phase& phase) -> void {
    for (const auto& reg : phase.read_channel<save::register_property>()) {
        process_registration(reg);
    }

    for (const auto& [category, map_ptr] : phase.read_channel<bind_int_map_request>()) {
        if (map_ptr) {
            bind_int_map(category, *map_ptr);
        }
    }

    if (!phase.read_channel<save_request>().empty()) {
        save();
    }

    for (const auto& [category, name, apply] : phase.read_channel<update_request>()) {
        if (auto* prop = find(category, name)) {
            apply(*prop);
        }
    }
}

auto gse::save::state::do_shutdown(shutdown_phase&) -> void {
    if (m_auto_save && !m_auto_save_path.empty()) {
        if (!save_to_file(m_auto_save_path)) {
            std::println("Failed to save settings to {}", m_auto_save_path.string());
        }
    }
}

auto gse::save::state::process_registration(const save::register_property& reg) -> void {
    if (find(reg.category, reg.name)) {
        return;
    }

    std::unique_ptr<property_base> prop;

    if (reg.type == typeid(bool)) {
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
        range_constraint<float> constraint{ reg.range_min, reg.range_max, reg.range_step };
        prop = std::make_unique<property<float, range_constraint<float>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint, reg.requires_restart
        );
    }
    else if (reg.type == typeid(int)) {
        auto* ref = static_cast<int*>(reg.ref);
        range_constraint<int> constraint{
            static_cast<int>(reg.range_min),
            static_cast<int>(reg.range_max),
            static_cast<int>(reg.range_step)
        };
        prop = std::make_unique<property<int, range_constraint<int>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint, reg.requires_restart
        );
    }

    if (prop) {
        register_property(std::move(prop));
    }
}

template <typename T>
auto gse::save::state::bind(const std::string_view category, const std::string_view name, T& ref) -> property_builder<T> {
    return property_builder<T>(*this, category, name, ref);
}

auto gse::save::state::register_property(std::unique_ptr<property_base> prop) -> property_base* {
    const std::string cat(prop->category());
    const std::string name(prop->name());

    auto it = std::ranges::find_if(m_properties, [&](const auto& p) {
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

auto gse::save::state::save_to_file(const std::filesystem::path& path) const -> bool {
    toml::table root;

    for (const auto& prop : m_properties) {
        std::string category(prop->category());
        std::string name(prop->name());

        if (!root.contains(category)) {
            root.insert(category, toml::table{});
        }

        auto& cat_table = *root[category].as_table();

        if (prop->type() == typeid(bool)) {
            cat_table.insert(name, prop->as_bool());
        } else if (prop->type() == typeid(float)) {
            cat_table.insert(name, static_cast<double>(prop->as_float()));
        } else if (prop->type() == typeid(int)) {
            cat_table.insert(name, static_cast<std::int64_t>(prop->as_int()));
        } else if (prop->type() == typeid(std::string)) {
            cat_table.insert(name, std::string(prop->as_string()));
        } else if (prop->constraint_kind() == constraint_type::enumeration) {
            cat_table.insert(name, static_cast<std::int64_t>(prop->as_int()));
        } else {
            std::ostringstream oss;
            prop->serialize(oss);
            cat_table.insert(name, oss.str());
        }
    }

    for (const auto& [category, map_ptr] : m_int_maps) {
        if (!map_ptr || map_ptr->empty()) continue;

        toml::table map_table;
        for (const auto& [key, value] : *map_ptr) {
            map_table.insert(key, static_cast<std::int64_t>(value));
        }
        root.insert(category, std::move(map_table));
    }

    std::ostringstream oss;
    oss << root;
    std::string content = oss.str();

    std::ranges::replace(content, '\'', '"');

    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << content;
    mark_all_clean();
    return true;
}

auto gse::save::state::load_from_file(const std::filesystem::path& path) const -> bool {
    if (!std::filesystem::exists(path)) {
        std::println("Settings file does not exist: {}", path.string());
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    toml::table root;
    try {
        root = toml::parse(content, path.string());
    } catch (const toml::parse_error& err) {
        std::println("TOML parse error in {}: {}", path.string(), err.what());
        return false;
    }

    for (const auto& [category, cat_node] : root) {
        const auto* cat_table = cat_node.as_table();
        if (!cat_table) continue;

        std::string cat_str(category.str());

        bool all_integers = !cat_table->empty();
        for (const auto& [name, value_node] : *cat_table) {
            if (!value_node.is_integer()) {
                all_integers = false;
                break;
            }
        }

        if (all_integers) {
            auto& cached = m_int_map_cache[cat_str];
            cached.clear();
            for (const auto& [name, value_node] : *cat_table) {
                cached[std::string(name.str())] = static_cast<int>(value_node.value_or<std::int64_t>(0));
            }

            if (auto it = m_int_maps.find(cat_str); it != m_int_maps.end() && it->second) {
                *it->second = cached;
            }
            continue;
        }

        for (const auto& [name, value_node] : *cat_table) {
            std::string key = cat_str + "." + std::string(name.str());

            property_base* prop = nullptr;
            for (const auto& p : m_properties) {
                if (std::string(p->category()) + "." + std::string(p->name()) == key) {
                    prop = p.get();
                    break;
                }
            }

            if (!prop) continue;

            if (value_node.is_boolean()) {
                prop->set_bool(value_node.value_or(false));
            } else if (value_node.is_integer()) {
                prop->set_int(static_cast<int>(value_node.value_or<std::int64_t>(0)));
            } else if (value_node.is_floating_point()) {
                prop->set_float(static_cast<float>(value_node.value_or(0.0)));
            } else if (value_node.is_string()) {
                prop->set_string(value_node.value_or<std::string>(""));
            }
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

auto gse::save::state::save() const -> bool {
    if (m_auto_save_path.empty()) {
        return false;
    }
    return save_to_file(m_auto_save_path);
}

auto gse::save::state::bind_int_map(const std::string_view category, std::map<std::string, int>& map) -> void {
    std::string cat_str(category);
    m_int_maps[cat_str] = &map;

    if (auto it = m_int_map_cache.find(cat_str); it != m_int_map_cache.end()) {
        map = it->second;
    }
}

auto gse::save::state::int_map(const std::string_view category) const -> const std::map<std::string, int>* {
    if (const auto it = m_int_maps.find(std::string(category)); it != m_int_maps.end()) {
        return it->second;
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

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    std::ranges::replace(content, '\'', '"');

    toml::table root;
    try {
        root = toml::parse(content, path.string());
    } catch (const toml::parse_error& err) {
        std::println("TOML parse error (early read) in {}: {}", path.string(), err.what());
        return std::nullopt;
    }

    const auto* cat_table = root[category].as_table();
    if (!cat_table) {
        return std::nullopt;
    }

    const auto* value_node = cat_table->get(name);
    if (!value_node) {
        return std::nullopt;
    }

    if (value_node->is_boolean()) {
        return value_node->value_or(false) ? "true" : "false";
    } else if (value_node->is_integer()) {
        return std::to_string(value_node->value_or<std::int64_t>(0));
    } else if (value_node->is_floating_point()) {
        return std::to_string(value_node->value_or(0.0));
    } else if (value_node->is_string()) {
        return value_node->value_or<std::string>("");
    }

    return std::nullopt;
}

auto gse::save::read_bool_setting_early(const std::filesystem::path& path, const std::string_view category, const std::string_view name, const bool default_value) -> bool {
    if (!std::filesystem::exists(path)) {
        return default_value;
    }

    std::ifstream file(path);
    if (!file) {
        return default_value;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    toml::table root;
    try {
        root = toml::parse(content, path.string());
    } catch (const toml::parse_error& err) {
        return default_value;
    }

    const auto* cat_table = root[category].as_table();
    if (!cat_table) {
        return default_value;
    }

    const auto* value_node = cat_table->get(name);
    if (!value_node) {
        return default_value;
    }

    return value_node->value_or(default_value);
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
        std::istringstream iss(*value_str);
        std::underlying_type_t<T> int_val{};
        if (iss >> int_val) {
            return static_cast<T>(int_val);
        }
        return default_value;
    }
    else {
        std::istringstream iss(*value_str);
        T result{};
        if (iss >> result) {
            return result;
        }
        return default_value;
    }
}

template <typename T>
gse::save::registration_builder<T>::registration_builder(
    channel<register_property>& ch,
    const std::string_view category,
    const std::string_view name,
    T& ref
) : m_channel(ch)
  , m_category(category)
  , m_name(name)
  , m_ref(ref) {}

template <typename T>
auto gse::save::registration_builder<T>::description(const std::string_view desc) -> registration_builder& {
    m_description = desc;
    return *this;
}

template <typename T>
auto gse::save::registration_builder<T>::default_value(T) -> registration_builder& {
    return *this;
}

template <typename T>
auto gse::save::registration_builder<T>::range(T min, T max, T step) -> registration_builder& requires std::is_arithmetic_v<T> {
    m_range_min = static_cast<float>(min);
    m_range_max = static_cast<float>(max);
    m_range_step = static_cast<float>(step);
    return *this;
}

template <typename T>
auto gse::save::registration_builder<T>::options(std::initializer_list<std::pair<std::string, T>> opts) -> registration_builder& {
    if constexpr (std::is_convertible_v<T, int>) {
        for (const auto& [label, value] : opts) {
            m_enum_options.emplace_back(label, static_cast<int>(value));
        }
    }
    return *this;
}

template <typename T>
auto gse::save::registration_builder<T>::options(std::vector<std::pair<std::string, T>> opts) -> registration_builder& {
    if constexpr (std::is_convertible_v<T, int>) {
        for (const auto& [label, value] : opts) {
            m_enum_options.emplace_back(label, static_cast<int>(value));
        }
    }
    return *this;
}

template <typename T>
auto gse::save::registration_builder<T>::restart_required() -> registration_builder& {
    m_requires_restart = true;
    return *this;
}

template <typename T>
auto gse::save::registration_builder<T>::commit() const -> void {
    m_channel.push({
        .category = m_category,
        .name = m_name,
        .description = m_description,
        .ref = reinterpret_cast<void*>(&m_ref),
        .type = typeid(T),
        .range_min = m_range_min,
        .range_max = m_range_max,
        .range_step = m_range_step,
        .enum_options = m_enum_options,
        .requires_restart = m_requires_restart
    });
}

template <typename T>
auto gse::save::bind(channel<register_property>& ch, const std::string_view category, const std::string_view name, T& ref) -> registration_builder<T> {
    return registration_builder<T>(ch, category, name, ref);
}