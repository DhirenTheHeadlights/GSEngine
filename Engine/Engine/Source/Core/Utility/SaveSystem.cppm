export module gse.utility:save_system;

import std;

import :id;
import :system;
import :concepts;

export namespace gse::save {
    class property_base;
    class system;

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
    };

    template <typename T, typename Constraint = no_constraint>
    class property final : public property_base {
    public:
        property(
            system* sys,
            std::string_view category,
            std::string_view name,
            std::string_view description,
            T& ref,
            T default_value,
            Constraint constraint = {}
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
        system* m_system;
        std::string m_category;
        std::string m_name;
        std::string m_description;
        T& m_ref;
        T m_default;
        Constraint m_constraint;
        bool m_dirty = false;
    };

    template <typename T>
    class property_builder {
    public:
        property_builder(
            system& sys,
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

        auto commit(
        ) -> property_base*;
    private:
        system& m_system;
        std::string m_category;
        std::string m_name;
        std::string m_description;
        T& m_ref;
        T m_default;
        std::any m_constraint;
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

    class system final : public gse::system {
    public:
        system() = default;

        auto initialize(
        ) -> void override;

        auto update(
        ) -> void override;

        auto shutdown(
        ) -> void override;

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

        [[nodiscard]] auto has_unsaved_changes(
        ) const -> bool;

        auto mark_all_clean(
        ) const -> void;

        using change_callback = std::function<void(property_base&)>;
        auto on_change(
            change_callback cb
        ) -> void;

        auto notify_change(
            property_base& prop
        ) -> void;
    private:
        std::vector<std::unique_ptr<property_base>> m_properties;
        mutable std::unordered_map<std::string, std::vector<property_base*>> m_by_category;
        std::vector<change_callback> m_callbacks;

        std::filesystem::path m_auto_save_path;
        bool m_auto_save = false;

        auto process_registration(
            const save::register_property& reg
        ) -> void;
    };

    struct property_changed {
        std::string_view category;
        std::string_view name;
        std::type_index type;
    };
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
    system* sys,
    const std::string_view category,
    const std::string_view name,
    const std::string_view description,
    T& ref,
    T default_value,
    Constraint constraint
) : m_system(sys)
  , m_category(category)
  , m_name(name)
  , m_description(description)
  , m_ref(ref)
  , m_default(std::move(default_value))
  , m_constraint(std::move(constraint)) {}

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
        if (m_system) {
            m_system->notify_change(*this);
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
        for (std::size_t i = 0; i < m_constraint.options.size(); ++i) {
            if (m_constraint.options[i].second == m_ref) {
                return i;
            }
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
        if (m_system) {
            m_system->notify_change(*this);
        }
    }
}

template <typename T>
gse::save::property_builder<T>::property_builder(
    system& sys,
    const std::string_view category,
    const std::string_view name,
    T& ref
) : m_system(sys)
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
auto gse::save::property_builder<T>::commit() -> property_base* {
    std::unique_ptr<property_base> prop;

    if (m_constraint.has_value()) {
        if (auto* range_c = std::any_cast<range_constraint<T>>(&m_constraint)) {
            prop = std::make_unique<property<T, range_constraint<T>>>(
                &m_system, m_category, m_name, m_description, m_ref, m_default, *range_c
            );
        }
        else if (auto* enum_c = std::any_cast<enum_constraint<T>>(&m_constraint)) {
            prop = std::make_unique<property<T, enum_constraint<T>>>(
                &m_system, m_category, m_name, m_description, m_ref, m_default, *enum_c
            );
        }
    }

    if (!prop) {
        prop = std::make_unique<property<T>>(
            &m_system, m_category, m_name, m_description, m_ref, m_default
        );
    }

    return m_system.register_property(std::move(prop));
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

auto gse::save::system::initialize() -> void {
    if (!m_auto_save_path.empty() && std::filesystem::exists(m_auto_save_path)) {
        if (!load_from_file(m_auto_save_path)) {
            std::println("Failed to load settings from {}", m_auto_save_path.string());
        }
    }
}

auto gse::save::system::update() -> void {
    for (const auto& reg : channel_of<save::register_property>()) {
        process_registration(reg);
    }

    for (const auto& [category, name, apply] : channel_of<update_request>()) {
        if (auto* prop = find(category, name)) {
            apply(*prop);
        }
    }
}

auto gse::save::system::shutdown() -> void {
    if (m_auto_save && !m_auto_save_path.empty()) {
        if (!save_to_file(m_auto_save_path)) {
            std::println("Failed to save settings to {}", m_auto_save_path.string());
        }
    }
}

auto gse::save::system::process_registration(const save::register_property& reg) -> void {
    if (find(reg.category, reg.name)) {
        return;
    }

    std::unique_ptr<property_base> prop;

    if (reg.type == typeid(bool)) {
        auto* ref = static_cast<bool*>(reg.ref);
        prop = std::make_unique<property<bool>>(
            this, reg.category, reg.name, reg.description, *ref, *ref
        );
    }
    else if (!reg.enum_options.empty()) {
        auto* ref = static_cast<int*>(reg.ref);
        enum_constraint<int> constraint;
        constraint.options = reg.enum_options;
        prop = std::make_unique<property<int, enum_constraint<int>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint
        );
    }
    else if (reg.type == typeid(float)) {
        auto* ref = static_cast<float*>(reg.ref);
        range_constraint<float> constraint{ reg.range_min, reg.range_max, reg.range_step };
        prop = std::make_unique<property<float, range_constraint<float>>>(
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint
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
            this, reg.category, reg.name, reg.description, *ref, *ref, constraint
        );
    }

    if (prop) {
        register_property(std::move(prop));
    }
}

template <typename T>
auto gse::save::system::bind(const std::string_view category, const std::string_view name, T& ref) -> property_builder<T> {
    return property_builder<T>(*this, category, name, ref);
}

auto gse::save::system::register_property(std::unique_ptr<property_base> prop) -> property_base* {
    auto* ptr = prop.get();
    m_by_category[std::string(prop->category())].push_back(ptr);
    m_properties.push_back(std::move(prop));
    return ptr;
}

auto gse::save::system::categories() const -> std::vector<std::string_view> {
    std::vector<std::string_view> result;
    result.reserve(m_by_category.size());
    for (const auto& cat : m_by_category | std::views::keys) {
        result.push_back(cat);
    }
    std::ranges::sort(result);
    return result;
}

auto gse::save::system::properties_in(const std::string_view category) const -> category_view {
    if (const auto it = m_by_category.find(std::string(category)); it != m_by_category.end()) {
        return category_view(it->second);
    }
    return category_view({});
}

auto gse::save::system::all_properties() const -> std::span<const std::unique_ptr<property_base>> {
    return m_properties;
}

template <typename T>
auto gse::save::system::get(const std::string_view category, const std::string_view name) const -> const property<T>* {
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

auto gse::save::system::find(const std::string_view category, const std::string_view name) const -> property_base* {
    for (const auto& prop : m_properties) {
        if (prop->category() == category && prop->name() == name) {
            return prop.get();
        }
    }
    return nullptr;
}

auto gse::save::system::save_to_file(const std::filesystem::path& path) const -> bool {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    for (const auto& prop : m_properties) {
        file << prop->category() << "." << prop->name() << " = ";
        prop->serialize(file);
        file << "\n";
    }

    mark_all_clean();
    return true;
}

auto gse::save::system::load_from_file(const std::filesystem::path& path) const -> bool {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::unordered_map<std::string, property_base*> lookup;
    for (const auto& prop : m_properties) {
        auto key = std::string(prop->category()) + "." + std::string(prop->name());
        lookup[key] = prop.get();
    }

    std::string line;
    while (std::getline(file, line)) {
        if (const auto eq = line.find(" = "); eq != std::string::npos) {
            auto key = line.substr(0, eq);
            auto value = line.substr(eq + 3);

            if (const auto it = lookup.find(key); it != lookup.end()) {
                std::istringstream iss(value);
                it->second->deserialize(iss);
            }
        }
    }

    mark_all_clean();
    return true;
}

auto gse::save::system::set_auto_save(const bool enabled, std::filesystem::path path) -> void {
    m_auto_save = enabled;
    if (!path.empty()) {
        m_auto_save_path = std::move(path);
    }
}

auto gse::save::system::has_unsaved_changes() const -> bool {
    return std::ranges::any_of(m_properties, [](const std::unique_ptr<property_base>& p) {
        return p->dirty();
    });
}

auto gse::save::system::mark_all_clean() const -> void {
    for (const auto& prop : m_properties) {
        prop->mark_clean();
    }
}

auto gse::save::system::on_change(change_callback cb) -> void {
    m_callbacks.push_back(std::move(cb));
}

auto gse::save::system::notify_change(property_base& prop) -> void {
    for (const auto& cb : m_callbacks) {
        cb(prop);
    }

    publish([&prop](channel<property_changed>& chan) {
        chan.emplace(prop.category(), prop.name(), prop.type());
    });
}