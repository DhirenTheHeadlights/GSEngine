export module gse.save:save_system;

import std;

import gse.log;
import gse.math;
import gse.meta;
import gse.core;
import gse.concurrency;
import gse.ecs;

export namespace gse::save {
    using write_thunk = void(*)(
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        const void* obj
    );

    using read_thunk = void(*)(
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
        std::string_view category,
        void* obj
    );

    struct persisted {
        std::string category;
        void* obj = nullptr;
        write_thunk write = nullptr;
        read_thunk read = nullptr;
    };

    struct register_request {
        persisted entry;
    };

    struct save_request {};
    struct restart_request {};

    class system {
    public:
        struct state {
            std::vector<persisted> entries;
            std::filesystem::path auto_save_path;
            bool auto_save = false;
            std::function<void()> on_restart;
            std::unordered_map<
                std::string,
                std::unordered_map<std::string, std::string>
            > loaded;
        };

        static auto initialize(
            init_context& phase,
            state& s
        ) -> void;

        static auto update(
            update_context& ctx,
            state& s
        ) -> async::task<>;

        static auto shutdown(
            shutdown_context& phase,
            state& s
        ) -> void;

        template <typename T>
        static auto register_struct(
            const init_context& phase,
            std::string_view category,
            T& obj
        ) -> void;

        template <typename T>
        static auto register_struct(
            update_context& ctx,
            std::string_view category,
            T& obj
        ) -> void;

        template <typename T>
        static auto register_struct(
            state& s,
            std::string_view category,
            T& obj
        ) -> void;

        static auto set_auto_save(
            state& s,
            bool enabled,
            std::filesystem::path path = {}
        ) -> void;

        static auto on_restart(
            state& s,
            std::function<void()> fn
        ) -> void;

        template <typename T>
        [[nodiscard]] static auto read_one(
            const std::filesystem::path& path,
            std::string_view category,
            std::string_view name,
            T fallback = T{}
        ) -> T;

    private:
        using doc = std::unordered_map<
            std::string,
            std::unordered_map<std::string, std::string>
        >;

        static auto read_file(
            const std::filesystem::path& path
        ) -> std::expected<std::string, std::error_code>;

        static auto trim(
            std::string_view s
        ) -> std::string_view;

        static auto parse(
            std::string_view text
        ) -> doc;

        static auto emit(
            const doc& d
        ) -> std::string;

        static auto add(
            state& s,
            persisted entry
        ) -> void;

        static auto save_to_file(
            const state& s,
            const std::filesystem::path& path
        ) -> bool;

        static auto load_from_file(
            state& s,
            const std::filesystem::path& path
        ) -> bool;

        static auto save(
            const state& s
        ) -> bool;

        template <typename T>
        static auto write_struct_thunk(
            doc& d,
            std::string_view category,
            const void* obj
        ) -> void;

        template <typename T>
        static auto read_struct_thunk(
            const doc& d,
            std::string_view category,
            void* obj
        ) -> void;

        template <typename T>
        static auto make_persisted(
            std::string_view category,
            T& obj
        ) -> persisted;
    };
}

auto gse::save::system::read_file(const std::filesystem::path& path) -> std::expected<std::string, std::error_code> {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::unexpected(ec);
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected(std::make_error_code(std::errc::io_error));
    }

    std::string content(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        file.read(content.data(), static_cast<std::streamsize>(size));
        content.resize(static_cast<std::size_t>(file.gcount()));
    }
    return content;
}

auto gse::save::system::trim(const std::string_view s) -> std::string_view {
    std::size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(start, end - start);
}

auto gse::save::system::parse(const std::string_view text) -> doc {
    doc result;
    std::string current_section;

    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t line_end = text.find('\n', pos);
        const std::string_view line_raw = text.substr(
            pos,
            line_end == std::string_view::npos ? text.size() - pos : line_end - pos
        );
        pos = line_end == std::string_view::npos ? text.size() : line_end + 1;

        const auto line = trim(line_raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = std::string(trim(line.substr(1, line.size() - 2)));
            result.try_emplace(current_section);
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }

        const auto key = std::string(trim(line.substr(0, eq)));
        const auto value = std::string(trim(line.substr(eq + 1)));
        if (current_section.empty()) {
            continue;
        }
        result[current_section][key] = value;
    }

    return result;
}

auto gse::save::system::emit(const doc& d) -> std::string {
    std::string out;
    bool first = true;
    for (const auto& [section, entries] : d) {
        if (!first) {
            out.push_back('\n');
        }
        first = false;
        out.push_back('[');
        out.append(section);
        out.append("]\n");
        for (const auto& [key, value] : entries) {
            out.append(key);
            out.append(" = ");
            out.append(value);
            out.push_back('\n');
        }
    }
    return out;
}

template <typename T>
auto gse::save::system::write_struct_thunk(doc& d, const std::string_view category, const void* obj) -> void {
    const auto& typed = *static_cast<const T*>(obj);
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (has_annotation<settings::describe>(m)) {
            constexpr std::string_view name = meta::member_name(m);
            std::string value;
            std::format_to(std::back_inserter(value), "{}", typed.[:m:]);
            d[std::string(category)][std::string(name)] = std::move(value);
        }
    }
}

template <typename T>
auto gse::save::system::read_struct_thunk(const doc& d, const std::string_view category, void* obj) -> void {
    auto& typed = *static_cast<T*>(obj);
    const auto cat_it = d.find(std::string(category));
    if (cat_it == d.end()) {
        return;
    }
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (has_annotation<settings::describe>(m)) {
            constexpr std::string_view name = meta::member_name(m);
            if (const auto it = cat_it->second.find(std::string(name)); it != cat_it->second.end()) {
                gse::parse(it->second, typed.[:m:]);
            }
        }
    }
}

template <typename T>
auto gse::save::system::make_persisted(const std::string_view category, T& obj) -> persisted {
    return persisted{
        .category = std::string(category),
        .obj = &obj,
        .write = &write_struct_thunk<T>,
        .read = &read_struct_thunk<T>,
    };
}

template <typename T>
auto gse::save::system::register_struct(const init_context& phase, const std::string_view category, T& obj) -> void {
    phase.channels.push<register_request>({ .entry = make_persisted(category, obj) });
}

template <typename T>
auto gse::save::system::register_struct(update_context& ctx, const std::string_view category, T& obj) -> void {
    ctx.channels.push<register_request>({ .entry = make_persisted(category, obj) });
}

template <typename T>
auto gse::save::system::register_struct(state& s, const std::string_view category, T& obj) -> void {
    add(s, make_persisted(category, obj));
}

auto gse::save::system::add(state& s, persisted entry) -> void {
    if (entry.read) {
        entry.read(s.loaded, entry.category, entry.obj);
    }

    const auto match = std::ranges::find_if(s.entries, [&](const persisted& existing) {
        return existing.category == entry.category && existing.obj == entry.obj;
    });

    if (match != s.entries.end()) {
        *match = std::move(entry);
        return;
    }

    s.entries.push_back(std::move(entry));
}

auto gse::save::system::save_to_file(const state& s, const std::filesystem::path& path) -> bool {
    doc d;
    for (const auto& entry : s.entries) {
        if (entry.write && entry.obj) {
            entry.write(d, entry.category, entry.obj);
        }
    }

    std::ofstream file(path);
    if (!file) {
        return false;
    }
    file << emit(d);
    return true;
}

auto gse::save::system::load_from_file(state& s, const std::filesystem::path& path) -> bool {
    if (!std::filesystem::exists(path)) {
        log::println(log::level::warning, log::category::save_system, "Settings file does not exist: {}", path.string());
        return false;
    }

    const auto content = read_file(path);
    if (!content) {
        log::println(log::level::warning, log::category::save_system, "Failed to read {}: {}", path.string(), content.error().message());
        return false;
    }

    s.loaded = parse(*content);

    for (const auto& entry : s.entries) {
        if (entry.read && entry.obj) {
            entry.read(s.loaded, entry.category, entry.obj);
        }
    }
    return true;
}

auto gse::save::system::save(const state& s) -> bool {
    if (s.auto_save_path.empty()) {
        return false;
    }
    return save_to_file(s, s.auto_save_path);
}

auto gse::save::system::set_auto_save(state& s, const bool enabled, std::filesystem::path path) -> void {
    s.auto_save = enabled;
    if (!path.empty()) {
        s.auto_save_path = std::move(path);
    }
}

auto gse::save::system::on_restart(state& s, std::function<void()> fn) -> void {
    s.on_restart = std::move(fn);
}

auto gse::save::system::initialize(init_context&, state& s) -> void {
    if (!s.auto_save_path.empty() && std::filesystem::exists(s.auto_save_path)) {
        if (!load_from_file(s, s.auto_save_path)) {
            log::println(log::level::warning, log::category::save_system, "Failed to load settings from {}", s.auto_save_path.string());
        }
    }
}

auto gse::save::system::update(update_context& ctx, state& s) -> async::task<> {
    for (auto& req : ctx.read_channel<register_request>()) {
        add(s, std::move(req.entry));
    }

    if (!ctx.read_channel<save_request>().empty()) {
        save(s);
    }

    if (!ctx.read_channel<restart_request>().empty()) {
        save(s);
        if (s.on_restart) {
            s.on_restart();
        }
    }
    co_return;
}

auto gse::save::system::shutdown(shutdown_context&, state& s) -> void {
    if (s.auto_save && !s.auto_save_path.empty()) {
        if (!save_to_file(s, s.auto_save_path)) {
            log::println(log::level::warning, log::category::save_system, "Failed to save settings to {}", s.auto_save_path.string());
        }
    }
}

template <typename T>
auto gse::save::system::read_one(const std::filesystem::path& path, const std::string_view category, const std::string_view name, T fallback) -> T {
    if (!std::filesystem::exists(path)) {
        return fallback;
    }
    const auto content = read_file(path);
    if (!content) {
        return fallback;
    }
    const auto d = parse(*content);
    const auto cat_it = d.find(std::string(category));
    if (cat_it == d.end()) {
        return fallback;
    }
    const auto val_it = cat_it->second.find(std::string(name));
    if (val_it == cat_it->second.end()) {
        return fallback;
    }
    T result = fallback;
    if (!gse::parse(val_it->second, result)) {
        return fallback;
    }
    return result;
}
