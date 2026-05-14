export module gse.assets:asset_system;

import std;
import gse.std_meta;
import gse.core;
import gse.config;
import gse.log;
import gse.containers;
import gse.concurrency;
import gse.fs;

import :asset_format;
import :resource_handle;
import :resource_loader;
import :registry;

export namespace gse::asset {
    template <typename T>
    concept loadable = requires (T t, load_ctx& ctx) {
        t.load(ctx);
    };

    template <typename T>
    concept has_compile_path = requires { typename T::baked; } && has_asset_format<typename T::baked> && requires (const std::filesystem::path& src, typename T::baked& b) {
        { bake(src, b) } -> std::convertible_to<bool>;
    };

    template <has_compile_path T>
    auto bake_to_disk(
        const std::filesystem::path& src,
        const std::filesystem::path& dst
    ) -> bool;

    template <has_compile_path T>
    auto needs_recompile(
        const std::filesystem::path& src,
        const std::filesystem::path& dst
    ) -> bool;

    template <typename T>
    auto enumerate_resources(
    ) -> std::vector<std::string>;

    template <typename T>
    auto recompile_if_stale(
        const std::filesystem::path& baked_path
    ) -> bool;

    template <typename T>
    auto setup_hot_reload_for(
        state& s
    ) -> void;

    struct compile_result {
        std::size_t success_count = 0;
        std::size_t failure_count = 0;
        std::size_t skipped_count = 0;

        auto operator+=(
            const compile_result& other
        ) -> compile_result&;
    };

    template <typename... Ts>
    class system : public non_copyable {
    public:
        explicit system(
            state& s
        );

        ~system() = default;

        system(
            system&&
        ) noexcept = default;

        auto operator=(
            system&&
        ) noexcept -> system& = default;

        template <typename T>
        auto compile(
        ) -> compile_result;

        auto compile_all(
        ) -> compile_result;

        auto register_loaders(
        ) -> void;

        auto install_hot_reload_fns(
        ) -> void;

    private:
        state* m_state;
    };

    template <typename Pack>
    using system_for = typename Pack::template apply<system>;
}

auto gse::asset::compile_result::operator+=(const compile_result& other) -> compile_result& {
    success_count += other.success_count;
    failure_count += other.failure_count;
    skipped_count += other.skipped_count;
    return *this;
}

template <gse::asset::has_compile_path T>
auto gse::asset::bake_to_disk(const std::filesystem::path& src, const std::filesystem::path& dst) -> bool {
    constexpr auto fmt = format_of<typename T::baked>();
    typename T::baked b{};
    if (!bake(src, b)) {
        return false;
    }
    std::filesystem::create_directories(dst.parent_path());
    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    binary_writer ar(out, fmt.magic, fmt.version);
    ar & b;
    log::println(log::category::assets, "Asset compiled: {}", dst.filename().string());
    return true;
}

template <gse::asset::has_compile_path T>
auto gse::asset::needs_recompile(const std::filesystem::path& src, const std::filesystem::path& dst) -> bool {
    if (!std::filesystem::exists(dst)) {
        return true;
    }
    constexpr auto fmt = format_of<typename T::baked>();
    const auto dst_time = std::filesystem::last_write_time(dst);
    if (std::filesystem::last_write_time(src) > dst_time) {
        return true;
    }
    if constexpr (fmt.meta_sidecar) {
        const auto meta = src.parent_path() / (src.stem().string() + ".meta");
        if (std::filesystem::exists(meta) && std::filesystem::last_write_time(meta) > dst_time) {
            return true;
        }
    }
    return false;
}

template <typename T>
auto gse::asset::enumerate_resources() -> std::vector<std::string> {
    std::vector<std::string> result;
    if constexpr (!requires { typename T::baked; }) {
        return result;
    }
    else {
        constexpr auto fmt = format_of<typename T::baked>();
        const auto dir_path = config::baked_resource_path / fmt.baked_dir;
        if (!std::filesystem::exists(dir_path)) {
            return result;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension().string() != fmt.baked_ext) {
                continue;
            }
            result.push_back(entry.path().stem().string());
        }

        std::ranges::sort(result);
        return result;
    }
}

template <typename T>
auto gse::asset::recompile_if_stale(const std::filesystem::path& baked_path) -> bool {
    if constexpr (!has_compile_path<T>) {
        return false;
    }
    else {
        constexpr auto fmt = format_of<typename T::baked>();
        auto rel = baked_path.lexically_relative(config::baked_resource_path / fmt.baked_dir);
        auto src_rel = rel;
        src_rel.replace_extension(fmt.source_exts.empty() ? "" : std::string(fmt.source_exts.front()));
        const auto src = config::resource_path / fmt.source_dir / src_rel;
        if (!std::filesystem::exists(src)) {
            return false;
        }
        if (!needs_recompile<T>(src, baked_path)) {
            return true;
        }
        return bake_to_disk<T>(src, baked_path);
    }
}

template <typename T>
auto gse::asset::setup_hot_reload_for(state& s) -> void {
    if constexpr (!has_compile_path<T>) {
        return;
    }
    else {
        constexpr auto fmt = format_of<typename T::baked>();
        const auto source_root = config::resource_path / fmt.source_dir;
        if (!std::filesystem::exists(source_root)) {
            return;
        }
        std::vector<std::string> exts(fmt.source_exts.begin(), fmt.source_exts.end());
        s.watcher.watch_directory(
            source_root,
            [&s](const std::filesystem::path& changed_file) {
                constexpr auto fmt = format_of<typename T::baked>();
                auto rel = std::filesystem::relative(changed_file, config::resource_path / fmt.source_dir);
                rel.replace_extension(std::string(fmt.baked_ext));
                const auto dst = config::baked_resource_path / fmt.baked_dir / rel;
                if (bake_to_disk<T>(changed_file, dst)) {
                    log::println(log::category::assets, "Hot reload recompiled: {}", changed_file.filename().string());
                    if constexpr (loadable<T>) {
                        if (auto it = s.resource_loaders.find(id_of<T>()); it != s.resource_loaders.end()) {
                            auto* loader = static_cast<resource::loader<T>*>(it->second.get());
                            loader->queue_reload_by_path(dst);
                        }
                    }
                }
                else {
                    log::println(log::level::warning, log::category::assets, "Hot reload failed to recompile: {}", changed_file.filename().string());
                }
            },
            exts,
            true
        );
    }
}

template <typename... Ts>
gse::asset::system<Ts...>::system(state& s) : m_state(&s) {}

template <typename... Ts>
auto gse::asset::system<Ts...>::register_loaders() -> void {
    auto try_register = [this]<typename T>() {
        if constexpr (loadable<T>) {
            auto* loader = add_loader<T>(*m_state);
            loader->set_pre_load_fn([](const std::filesystem::path& baked_path) {
                recompile_if_stale<T>(baked_path);
            });
        }
    };
    (try_register.template operator()<Ts>(), ...);
}

template <typename... Ts>
auto gse::asset::system<Ts...>::install_hot_reload_fns() -> void {
    auto* s = m_state;
    s->enable_hot_reload_fn = [s] {
        (setup_hot_reload_for<Ts>(*s), ...);
    };
    s->disable_hot_reload_fn = [s] {
        s->watcher.clear();
    };
}

template <typename... Ts>
template <typename T>
auto gse::asset::system<Ts...>::compile() -> compile_result {
    compile_result result{};

    if constexpr (!has_compile_path<T>) {
        return result;
    }
    else {
        constexpr auto fmt = format_of<typename T::baked>();
        const auto source_root = config::resource_path / fmt.source_dir;
        const auto baked_root = config::baked_resource_path / fmt.baked_dir;

        if (!std::filesystem::exists(source_root)) {
            return result;
        }

        std::filesystem::create_directories(baked_root);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            if (std::ranges::find(fmt.source_exts, ext) == fmt.source_exts.end()) {
                continue;
            }

            auto rel = std::filesystem::relative(entry.path(), source_root);
            rel.replace_extension(std::string(fmt.baked_ext));
            const auto dst = baked_root / rel;

            if (!needs_recompile<T>(entry.path(), dst)) {
                ++result.skipped_count;
            }
            else if (bake_to_disk<T>(entry.path(), dst)) {
                ++result.success_count;
            }
            else {
                ++result.failure_count;
                continue;
            }

            if constexpr (loadable<T>) {
                if (!std::filesystem::exists(dst)) {
                    continue;
                }
                if (auto it = m_state->resource_loaders.find(id_of<T>()); it != m_state->resource_loaders.end()) {
                    static_cast<resource::loader<T>*>(it->second.get())->queue_by_path(dst);
                }
            }
        }

        return result;
    }
}

template <typename... Ts>
auto gse::asset::system<Ts...>::compile_all() -> compile_result {
    compile_result total{};
    ((total += compile<Ts>()), ...);
    return total;
}
