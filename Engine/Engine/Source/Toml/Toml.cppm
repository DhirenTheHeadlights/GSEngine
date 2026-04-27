export module gse.toml:toml;

import std;
import gse.std_meta;

export namespace gse::toml {
    struct value;
    using table = std::map<std::string, value>;
    using array = std::vector<value>;

    struct parse_error {
        std::string message;
        std::size_t line = 0;
        std::size_t column = 0;
    };

    struct value {
        std::variant<std::monostate, bool, std::int64_t, double, std::string, array, table> data;

        auto is_null() const -> bool {
            return std::holds_alternative<std::monostate>(data);
        }
        auto is_bool() const -> bool {
            return std::holds_alternative<bool>(data);
        }
        auto is_int() const -> bool {
            return std::holds_alternative<std::int64_t>(data);
        }
        auto is_double() const -> bool {
            return std::holds_alternative<double>(data);
        }
        auto is_string() const -> bool {
            return std::holds_alternative<std::string>(data);
        }
        auto is_array() const -> bool {
            return std::holds_alternative<array>(data);
        }
        auto is_table() const -> bool {
            return std::holds_alternative<table>(data);
        }

        template <typename Self>
        auto as_bool(
            this Self& self
        ) -> decltype(auto) {
            return std::get<bool>(self.data);
        }
        template <typename Self>
        auto as_int(
            this Self& self
        ) -> decltype(auto) {
            return std::get<std::int64_t>(self.data);
        }
        template <typename Self>
        auto as_double(
            this Self& self
        ) -> decltype(auto) {
            return std::get<double>(self.data);
        }
        template <typename Self>
        auto as_string(
            this Self& self
        ) -> decltype(auto) {
            return std::get<std::string>(self.data);
        }
        template <typename Self>
        auto as_array(
            this Self& self
        ) -> decltype(auto) {
            return std::get<array>(self.data);
        }
        template <typename Self>
        auto as_table(
            this Self& self
        ) -> decltype(auto) {
            return std::get<table>(self.data);
        }

        template <typename T>
        auto value_or(
            T fallback
        ) const -> T;
    };

    auto parse(
        std::string_view text,
        std::string_view source = ""
    ) -> std::expected<value, parse_error>;

    auto emit(
        const value& v
    ) -> std::string;

    template <typename T>
    auto to_toml(
        const T& obj
    ) -> value;

    template <typename T>
    auto from_toml(
        const value& v,
        T& out
    ) -> bool;
}

namespace gse::toml::internal {
    struct parser {
        std::string_view m_text;
        std::string_view m_source;
        std::size_t m_pos = 0;
        std::size_t m_line = 1;
        std::size_t m_col = 1;

        auto eof() const -> bool {
            return m_pos >= m_text.size();
        }
        auto peek() const -> char {
            return eof() ? '\0' : m_text[m_pos];
        }
        auto peek_at(std::size_t offset) const -> char {
            const auto p = m_pos + offset;
            return p >= m_text.size() ? '\0' : m_text[p];
        }
        auto advance() -> char {
            if (eof()) {
                return '\0';
            }
            const char c = m_text[m_pos++];
            if (c == '\n') {
                ++m_line;
                m_col = 1;
            }
            else {
                ++m_col;
            }
            return c;
        }
        auto match(char c) -> bool {
            if (peek() == c) {
                advance();
                return true;
            }
            return false;
        }
        auto error(std::string msg) const -> std::unexpected<parse_error> {
            return std::unexpected(parse_error{
                .message = std::move(msg),
                .line = m_line,
                .column = m_col,
            });
        }

        auto skip_inline_ws(
        ) -> void;
        auto skip_blank_lines(
        ) -> void;
        auto parse_key(
        ) -> std::expected<std::string, parse_error>;
        auto parse_string(
        ) -> std::expected<std::string, parse_error>;
        auto parse_number(
        ) -> std::expected<value, parse_error>;
        auto parse_value(
        ) -> std::expected<value, parse_error>;
        auto parse_inline_array(
        ) -> std::expected<value, parse_error>;
        auto parse_header(
            table& root
        ) -> std::expected<table*, parse_error>;
        auto parse_document(
        ) -> std::expected<value, parse_error>;
    };

    auto is_bare_key_char(
        char c
    ) -> bool;

    auto emit_string(
        std::string& out,
        std::string_view s
    ) -> void;
    auto emit_key(
        std::string& out,
        std::string_view key
    ) -> void;
    auto emit_inline_value(
        std::string& out,
        const value& v
    ) -> void;
    auto is_array_of_tables(
        const value& v
    ) -> bool;
    auto emit_table(
        std::string& out,
        const table& t,
        const std::string& prefix
    ) -> void;

    template <typename T>
    struct is_optional : std::false_type {};
    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type {};

    template <typename T>
    struct is_vector : std::false_type {};
    template <typename T>
    struct is_vector<std::vector<T>> : std::true_type {};

    template <typename T>
    concept is_quantity_like = requires {
        typename T::value_type;
        typename T::default_unit;
        typename T::dimension;
        typename T::quantity_tag;
    };
}

template <typename T>
auto gse::toml::value::value_or(T fallback) const -> T {
    if constexpr (std::same_as<T, bool>) {
        return is_bool() ? std::get<bool>(data) : fallback;
    }
    else if constexpr (std::same_as<T, std::string>) {
        return is_string() ? std::get<std::string>(data) : fallback;
    }
    else if constexpr (std::integral<T>) {
        if (is_int()) {
            return static_cast<T>(std::get<std::int64_t>(data));
        }
        return fallback;
    }
    else if constexpr (std::floating_point<T>) {
        if (is_double()) {
            return static_cast<T>(std::get<double>(data));
        }
        if (is_int()) {
            return static_cast<T>(std::get<std::int64_t>(data));
        }
        return fallback;
    }
    else {
        return fallback;
    }
}

auto gse::toml::internal::is_bare_key_char(const char c) -> bool {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '_' || c == '-';
}

auto gse::toml::internal::parser::skip_inline_ws() -> void {
    while (!eof()) {
        const char c = peek();
        if (c == ' ' || c == '\t') {
            advance();
        }
        else if (c == '#') {
            while (!eof() && peek() != '\n') {
                advance();
            }
        }
        else {
            break;
        }
    }
}

auto gse::toml::internal::parser::skip_blank_lines() -> void {
    while (!eof()) {
        skip_inline_ws();
        if (peek() == '\r') {
            advance();
        }
        if (peek() == '\n') {
            advance();
            continue;
        }
        break;
    }
}

auto gse::toml::internal::parser::parse_string() -> std::expected<std::string, parse_error> {
    if (!match('"')) {
        return error("expected '\"' at start of string");
    }
    std::string out;
    while (!eof()) {
        const char c = advance();
        if (c == '"') {
            return out;
        }
        if (c == '\\') {
            const char esc = advance();
            switch (esc) {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case 'n':  out.push_back('\n'); break;
                case 't':  out.push_back('\t'); break;
                case 'r':  out.push_back('\r'); break;
                case '0':  out.push_back('\0'); break;
                default:
                    return error(std::format("unknown string escape '\\{}'", esc));
            }
            continue;
        }
        if (c == '\n') {
            return error("unterminated string literal");
        }
        out.push_back(c);
    }
    return error("unterminated string literal");
}

auto gse::toml::internal::parser::parse_key() -> std::expected<std::string, parse_error> {
    skip_inline_ws();
    if (peek() == '"') {
        return parse_string();
    }
    std::string key;
    while (!eof() && is_bare_key_char(peek())) {
        key.push_back(advance());
    }
    if (key.empty()) {
        return error("expected key");
    }
    return key;
}

auto gse::toml::internal::parser::parse_number() -> std::expected<value, parse_error> {
    const std::size_t start = m_pos;
    if (peek() == '+' || peek() == '-') {
        advance();
    }
    bool has_digits = false;
    while (!eof()) {
        const char c = peek();
        if ((c >= '0' && c <= '9') || c == '_') {
            advance();
            if (c != '_') {
                has_digits = true;
            }
        }
        else {
            break;
        }
    }
    if (!has_digits) {
        return error("expected digits in number");
    }
    bool is_float = false;
    if (peek() == '.') {
        is_float = true;
        advance();
        while (!eof()) {
            const char c = peek();
            if ((c >= '0' && c <= '9') || c == '_') {
                advance();
            }
            else {
                break;
            }
        }
    }
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        while (!eof()) {
            const char c = peek();
            if ((c >= '0' && c <= '9') || c == '_') {
                advance();
            }
            else {
                break;
            }
        }
    }
    std::string raw;
    raw.reserve(m_pos - start);
    for (std::size_t i = start; i < m_pos; ++i) {
        if (m_text[i] != '_') {
            raw.push_back(m_text[i]);
        }
    }
    if (is_float) {
        try {
            return value{ .data = std::stod(raw) };
        } catch (...) {
            return error(std::format("invalid float literal '{}'", raw));
        }
    }
    try {
        return value{ .data = static_cast<std::int64_t>(std::stoll(raw)) };
    } catch (...) {
        return error(std::format("invalid integer literal '{}'", raw));
    }
}

auto gse::toml::internal::parser::parse_inline_array() -> std::expected<value, parse_error> {
    if (!match('[')) {
        return error("expected '[' at start of array");
    }
    array arr;
    skip_blank_lines();
    if (match(']')) {
        return value{ .data = std::move(arr) };
    }
    while (true) {
        skip_blank_lines();
        auto elem = parse_value();
        if (!elem) {
            return std::unexpected(elem.error());
        }
        arr.push_back(std::move(*elem));
        skip_blank_lines();
        if (match(',')) {
            skip_blank_lines();
            if (peek() == ']') {
                advance();
                break;
            }
            continue;
        }
        if (match(']')) {
            break;
        }
        return error("expected ',' or ']' in array");
    }
    return value{ .data = std::move(arr) };
}

auto gse::toml::internal::parser::parse_value() -> std::expected<value, parse_error> {
    skip_inline_ws();
    const char c = peek();
    if (c == '"') {
        auto s = parse_string();
        if (!s) {
            return std::unexpected(s.error());
        }
        return value{ .data = std::move(*s) };
    }
    if (c == '[') {
        return parse_inline_array();
    }
    if (c == 't' || c == 'f') {
        if (m_text.substr(m_pos).starts_with("true")) {
            m_pos += 4;
            m_col += 4;
            return value{ .data = true };
        }
        if (m_text.substr(m_pos).starts_with("false")) {
            m_pos += 5;
            m_col += 5;
            return value{ .data = false };
        }
        return error("expected 'true' or 'false'");
    }
    if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
        return parse_number();
    }
    return error("unexpected character in value");
}

auto gse::toml::internal::parser::parse_header(table& root) -> std::expected<table*, parse_error> {
    if (!match('[')) {
        return error("expected '['");
    }
    const bool is_array_of_tables = match('[');
    skip_inline_ws();
    auto name = parse_key();
    if (!name) {
        return std::unexpected(name.error());
    }
    skip_inline_ws();
    if (!match(']')) {
        return error("expected ']'");
    }
    if (is_array_of_tables && !match(']')) {
        return error("expected ']]'");
    }
    if (is_array_of_tables) {
        auto& slot = root[*name];
        if (!slot.is_array()) {
            slot.data = array{};
        }
        auto& arr = slot.as_array();
        value tbl;
        tbl.data = table{};
        arr.push_back(std::move(tbl));
        return &arr.back().as_table();
    }
    auto& slot = root[*name];
    if (!slot.is_table()) {
        slot.data = table{};
    }
    return &slot.as_table();
}

auto gse::toml::internal::parser::parse_document() -> std::expected<value, parse_error> {
    table root;
    table* current = &root;
    skip_blank_lines();
    while (!eof()) {
        skip_inline_ws();
        if (eof()) {
            break;
        }
        if (peek() == '[') {
            auto next = parse_header(root);
            if (!next) {
                return std::unexpected(next.error());
            }
            current = *next;
            skip_inline_ws();
            skip_blank_lines();
            continue;
        }
        auto key = parse_key();
        if (!key) {
            return std::unexpected(key.error());
        }
        skip_inline_ws();
        if (!match('=')) {
            return error("expected '=' after key");
        }
        skip_inline_ws();
        auto val = parse_value();
        if (!val) {
            return std::unexpected(val.error());
        }
        (*current)[*key] = std::move(*val);
        skip_inline_ws();
        skip_blank_lines();
    }
    return value{ .data = std::move(root) };
}

auto gse::toml::parse(const std::string_view text, const std::string_view source) -> std::expected<value, parse_error> {
    internal::parser p{ .m_text = text, .m_source = source };
    return p.parse_document();
}

auto gse::toml::internal::emit_string(std::string& out, const std::string_view s) -> void {
    out.push_back('"');
    for (const char c : s) {
        switch (c) {
            case '"':  out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\n': out.append("\\n");  break;
            case '\t': out.append("\\t");  break;
            case '\r': out.append("\\r");  break;
            case '\0': out.append("\\0");  break;
            default:   out.push_back(c);   break;
        }
    }
    out.push_back('"');
}

auto gse::toml::internal::emit_inline_value(std::string& out, const value& v) -> void {
    if (v.is_bool()) {
        out.append(v.as_bool() ? "true" : "false");
        return;
    }
    if (v.is_int()) {
        out.append(std::to_string(v.as_int()));
        return;
    }
    if (v.is_double()) {
        const double d = v.as_double();
        if (std::isfinite(d) && d == std::trunc(d) && std::abs(d) < 1e18) {
            out.append(std::format("{:.1f}", d));
        }
        else {
            out.append(std::format("{}", d));
        }
        return;
    }
    if (v.is_string()) {
        emit_string(out, v.as_string());
        return;
    }
    if (v.is_array()) {
        out.append("[ ");
        const auto& arr = v.as_array();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                out.append(", ");
            }
            emit_inline_value(out, arr[i]);
        }
        out.append(" ]");
        return;
    }
}

auto gse::toml::internal::emit_key(std::string& out, const std::string_view key) -> void {
    const bool bare = !key.empty() && std::ranges::all_of(key, is_bare_key_char);
    if (bare) {
        out.append(key);
    }
    else {
        emit_string(out, key);
    }
}

auto gse::toml::internal::is_array_of_tables(const value& v) -> bool {
    if (!v.is_array()) {
        return false;
    }
    const auto& arr = v.as_array();
    return !arr.empty() && std::ranges::all_of(arr, [](const auto& e) { return e.is_table(); });
}

auto gse::toml::internal::emit_table(std::string& out, const table& t, const std::string& prefix) -> void {
    bool wrote_scalar = false;
    for (const auto& [key, val] : t) {
        if (val.is_table() || is_array_of_tables(val)) {
            continue;
        }
        emit_key(out, key);
        out.append(" = ");
        emit_inline_value(out, val);
        out.push_back('\n');
        wrote_scalar = true;
    }
    for (const auto& [key, val] : t) {
        if (val.is_table()) {
            if (wrote_scalar || !prefix.empty()) {
                out.push_back('\n');
            }
            std::string section = prefix.empty() ? key : prefix + "." + key;
            out.push_back('[');
            emit_key(out, section);
            out.append("]\n");
            emit_table(out, val.as_table(), section);
            wrote_scalar = true;
            continue;
        }
        if (is_array_of_tables(val)) {
            const std::string section = prefix.empty() ? key : prefix + "." + key;
            for (const auto& elem : val.as_array()) {
                if (wrote_scalar || !prefix.empty()) {
                    out.push_back('\n');
                }
                out.append("[[");
                emit_key(out, section);
                out.append("]]\n");
                emit_table(out, elem.as_table(), section);
                wrote_scalar = true;
            }
        }
    }
}

auto gse::toml::emit(const value& v) -> std::string {
    std::string out;
    if (v.is_table()) {
        internal::emit_table(out, v.as_table(), std::string{});
    }
    else {
        internal::emit_inline_value(out, v);
        out.push_back('\n');
    }
    return out;
}

template <typename T>
auto gse::toml::to_toml(const T& obj) -> value {
    using namespace internal;
    if constexpr (std::same_as<T, bool>) {
        return value{ .data = obj };
    }
    else if constexpr (std::same_as<T, std::string>) {
        return value{ .data = obj };
    }
    else if constexpr (std::is_enum_v<T>) {
        return value{ .data = static_cast<std::int64_t>(obj) };
    }
    else if constexpr (std::integral<T>) {
        return value{ .data = static_cast<std::int64_t>(obj) };
    }
    else if constexpr (std::floating_point<T>) {
        return value{ .data = static_cast<double>(obj) };
    }
    else if constexpr (is_quantity_like<T>) {
        using u = typename T::default_unit;
        using vt = typename T::value_type;
        const vt raw = obj.template as<u>();
        if constexpr (std::integral<vt>) {
            return value{ .data = static_cast<std::int64_t>(raw) };
        }
        else {
            return value{ .data = static_cast<double>(raw) };
        }
    }
    else if constexpr (is_vector<T>::value) {
        array arr;
        arr.reserve(obj.size());
        for (const auto& e : obj) {
            arr.push_back(to_toml(e));
        }
        return value{ .data = std::move(arr) };
    }
    else if constexpr (is_optional<T>::value) {
        if (obj.has_value()) {
            return to_toml(*obj);
        }
        return value{};
    }
    else if constexpr (std::is_aggregate_v<T> || std::is_class_v<T>) {
        table t;
        template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
            const std::string key(std::meta::identifier_of(m));
            t.emplace(key, to_toml(obj.[:m:]));
        }
        return value{ .data = std::move(t) };
    }
    else {
        return value{};
    }
}

template <typename T>
auto gse::toml::from_toml(const value& v, T& out) -> bool {
    using namespace internal;
    if constexpr (std::same_as<T, bool>) {
        if (!v.is_bool()) {
            return false;
        }
        out = v.as_bool();
        return true;
    }
    else if constexpr (std::same_as<T, std::string>) {
        if (!v.is_string()) {
            return false;
        }
        out = v.as_string();
        return true;
    }
    else if constexpr (std::is_enum_v<T>) {
        if (!v.is_int()) {
            return false;
        }
        out = static_cast<T>(v.as_int());
        return true;
    }
    else if constexpr (std::integral<T>) {
        if (!v.is_int()) {
            return false;
        }
        out = static_cast<T>(v.as_int());
        return true;
    }
    else if constexpr (std::floating_point<T>) {
        if (v.is_double()) {
            out = static_cast<T>(v.as_double());
            return true;
        }
        if (v.is_int()) {
            out = static_cast<T>(v.as_int());
            return true;
        }
        return false;
    }
    else if constexpr (is_quantity_like<T>) {
        using u = typename T::default_unit;
        using vt = typename T::value_type;
        if constexpr (std::integral<vt>) {
            if (!v.is_int()) {
                return false;
            }
            out = T::template from<u>(static_cast<vt>(v.as_int()));
            return true;
        }
        else {
            if (v.is_double()) {
                out = T::template from<u>(static_cast<vt>(v.as_double()));
                return true;
            }
            if (v.is_int()) {
                out = T::template from<u>(static_cast<vt>(v.as_int()));
                return true;
            }
            return false;
        }
    }
    else if constexpr (is_vector<T>::value) {
        if (!v.is_array()) {
            return false;
        }
        out.clear();
        out.reserve(v.as_array().size());
        for (const auto& e : v.as_array()) {
            typename T::value_type elem{};
            if (!from_toml(e, elem)) {
                return false;
            }
            out.push_back(std::move(elem));
        }
        return true;
    }
    else if constexpr (is_optional<T>::value) {
        if (v.is_null()) {
            out.reset();
            return true;
        }
        typename T::value_type inner{};
        if (!from_toml(v, inner)) {
            return false;
        }
        out = std::move(inner);
        return true;
    }
    else if constexpr (std::is_aggregate_v<T> || std::is_class_v<T>) {
        if (!v.is_table()) {
            return false;
        }
        const auto& t = v.as_table();
        template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
            const std::string key(std::meta::identifier_of(m));
            if (const auto it = t.find(key); it != t.end()) {
                if (!from_toml(it->second, out.[:m:])) {
                    return false;
                }
            }
        }
        return true;
    }
    else {
        return false;
    }
}
